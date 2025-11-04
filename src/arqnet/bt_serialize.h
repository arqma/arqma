#pragma once

#include <string>
#ifdef __cpp_lib_string_view
#include <string_view>
#endif
#include <vector>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <ostream>
#include <istream>
#include <sstream>
#include <boost/variant.hpp>
#include "common.h"
#include "common/osrb.h"
#include <variant>

namespace arqnet {

class bt_deserialize_invalid : public std::invalid_argument
{
  using std::invalid_argument::invalid_argument;
};

class bt_deserialize_invalid_type : public bt_deserialize_invalid
{
  using bt_deserialize_invalid::bt_deserialize_invalid;
};

using bt_value = boost::make_recursive_variant<
  std::string, int64_t,
  std::list<boost::recursive_variant_>,
  std::unordered_map<std::string, boost::recursive_variant_>
>::type;

using bt_dict = std::unordered_map<std::string, bt_value>;

using bt_list = std::list<bt_value>;

namespace detail {

template <typename T, typename SFINAE = void>
struct bt_serialize { static_assert(!std::is_same<T, T>::value, "Cannot serialize T: unsupported type for bt serialization"); };

template <typename T, typename SFINAE = void>
struct bt_deserialize { static_assert(!std::is_same<T, T>::value, "Cannot deserialize T: unsupported type for bt deserialization"); };

inline void bt_no_eof(std::istream &is)
{
  if (!is.good())
    throw bt_deserialize_invalid(std::string(is.eof() ? "Unexpected EOF" : "I/O error") + " while deserializing at location " + std::to_string(is.tellg()));
}

template <typename Exception = bt_deserialize_invalid>
void bt_expect(std::istream &is, char expect)
{
  char t;
  is.get(t);
  bt_no_eof(is);
  if (t != expect)
  {
    is.unget();
    throw Exception("Expected '" + std::string(1, expect) + "' but found '" + std::string(1, t) + "' at input location " + std::to_string(is.tellg()));
  }
}

extern template void bt_expect<bt_deserialize_invalid>(std::istream &is, char expect);
extern template void bt_expect<bt_deserialize_invalid_type>(std::istream &is, char expect);

union maybe_signed_int64_t { int64_t i64; uint64_t u64; };

std::pair<maybe_signed_int64_t, bool> bt_deserialize_integer(std::istream &is);

template <typename T>
struct bt_serialize<T, std::enable_if_t<std::is_integral<T>::value>>
{
  static_assert(sizeof(T) <= sizeof(uint64_t), "Serialization of integers larger than uint64_t is not supported");
  void operator()(std::ostream &os, const T &val)
  {
    using output_type = std::conditional_t<(sizeof(T) > 1), T, std::conditional_t<std::is_signed<T>::value, int, unsigned>>;
    os << 'i' << static_cast<output_type>(val) << 'e';
  }
};

template <typename T>
struct bt_deserialize<T, std::enable_if_t<std::is_integral<T>::value>>
{
  void operator()(std::istream &is, T &val)
  {
    constexpr uint64_t umax = static_cast<uint64_t>(std::numeric_limits<T>::max());
    constexpr int64_t smin = static_cast<int64_t>(std::numeric_limits<T>::min()), smax = static_cast<int64_t>(std::numeric_limits<T>::max());

    auto read = bt_deserialize_integer(is);
    if (std::is_signed<T>::value) {
      if (!read.second) { // read a positive value
        if (read.first.u64 > umax)
          throw bt_deserialize_invalid("Found too-large value " + std::to_string(read.first.u64) + " when deserializing just before input location " + std::to_string(is.tellg()));
        val = static_cast<T>(read.first.u64);
      } else {
        bool oob = read.first.i64 < smin;
             oob |= read.first.i64 > smax;
        if (sizeof(T) < sizeof(int64_t) && oob)
          throw bt_deserialize_invalid("Found out-of-range value " + std::to_string(read.first.i64) + " when deserializing just before input location " + std::to_string(is.tellg()));
        val = static_cast<T>(read.first.i64);
      }
    } else {
      if (read.second)
        throw bt_deserialize_invalid("Found invalid negative value " + std::to_string(read.first.i64) + " when deserializing just before input location " + std::to_string(is.tellg()));
        if (sizeof(T) < sizeof(uint64_t) && read.first.u64 > umax)
          throw bt_deserialize_invalid("Found too-large value " + std::to_string(read.first.u64) + " when deserializing just before input location " + std::to_string(is.tellg()));
        val = static_cast<T>(read.first.u64);
    }
  }
};

extern template struct bt_deserialize<int64_t>;
extern template struct bt_deserialize<uint64_t>;

/// String specialization
template <>
struct bt_serialize<std::string> {
    void operator()(std::ostream &os, const std::string &val) { os << val.size() << ':' << val; }
};
template <>
struct bt_deserialize<std::string> {
    void operator()(std::istream &is, std::string &val);
};

/// char * and string literals -- we allow serialization for convenience, but not deserialization
template <>
struct bt_serialize<char *> {
  void operator()(std::ostream &os, const char *str) { auto len = std::strlen(str); os << len << ':'; os.write(str, len); }
};
template <size_t N>
struct bt_serialize<char[N]> {
    void operator()(std::ostream &os, const char *str) { os << N-1 << ':'; os.write(str, N-1); }
};

/// Partial dict validity; we don't check the second type for serializability, that will be handled
/// via the base case static_assert if invalid.
template <typename T, typename = void> struct is_bt_input_dict_container : std::false_type {};
template <typename T>
struct is_bt_input_dict_container<T, std::enable_if_t<
    std::is_same<std::string, std::remove_cv_t<typename T::value_type::first_type>>::value,
    void_t<typename T::const_iterator /* is const iterable */,
           typename T::value_type::second_type /* has a second type */>>>
: std::true_type {};

template <typename T, typename = void> struct is_bt_insertable : std::false_type {};
template <typename T>
struct is_bt_insertable<T,
    void_t<decltype(std::declval<T>().insert(std::declval<T>().end(), std::declval<typename T::value_type>()))>>
: std::true_type {};

template <typename T, typename = void> struct is_bt_output_dict_container : std::false_type {};
template <typename T>
struct is_bt_output_dict_container<T, std::enable_if_t<
    std::is_same<std::string, std::remove_cv_t<typename T::value_type::first_type>>::value &&
    is_bt_insertable<T>::value,
    void_t<typename T::value_type::second_type /* has a second type */>>>
: std::true_type {};


/// Specialization for a dict-like container (such as an unordered_map).  We accept anything for a
/// dict that is const iterable over something that looks like a pair with std::string for first
/// value type.  The value (i.e. second element of the paid) also must be serializable.
template <typename T>
struct bt_serialize<T, std::enable_if_t<is_bt_input_dict_container<T>::value>> {
    using second_type = typename T::value_type::second_type;
    using ref_pair = std::reference_wrapper<const typename T::value_type>;
    void operator()(std::ostream &os, const T &dict) {
        os << 'd';
        std::vector<ref_pair> pairs;
        pairs.reserve(dict.size());
        for (const auto &pair : dict)
            pairs.emplace(pairs.end(), pair);
        std::sort(pairs.begin(), pairs.end(), [](ref_pair a, ref_pair b) { return a.get().first < b.get().first; });
        for (auto &ref : pairs) {
            bt_serialize<std::string>{}(os, ref.get().first);
            bt_serialize<second_type>{}(os, ref.get().second);
        }
        os << 'e';
    }
};

template <typename T>
struct bt_deserialize<T, std::enable_if_t<is_bt_output_dict_container<T>::value>> {
    using second_type = typename T::value_type::second_type;
    void operator()(std::istream &is, T &dict) {
        bt_expect<bt_deserialize_invalid_type>(is, 'd');
        dict.clear();
        bt_deserialize<std::string> key_deserializer;
        bt_deserialize<second_type> val_deserializer;
        while (is.peek() != 'e') {
            bt_no_eof(is);
            std::string key;
            second_type val;
            try {
                key_deserializer(is, key);
                val_deserializer(is, val);
            } catch (const bt_deserialize_invalid_type &e) {
                // Rethrow a sub-element invalid type as a regular error (because the type *was* a list)
                throw bt_deserialize_invalid(e.what());
            }
            dict.insert(dict.end(), typename T::value_type{std::move(key), std::move(val)});
        }
        bt_expect(is, 'e');
    }
};


/// Accept anything that looks iterable; value serialization validity isn't checked here (it fails
/// via the base case static assert).
template <typename T, typename = void> struct is_bt_input_list_container : std::false_type {};
template <typename T>
struct is_bt_input_list_container<T, std::enable_if_t<
    !std::is_same<T, std::string>::value &&
    !is_bt_input_dict_container<T>::value,
    void_t<typename T::const_iterator, typename T::value_type>>>
: std::true_type {};

template <typename T, typename = void> struct is_bt_output_list_container : std::false_type {};
template <typename T>
struct is_bt_output_list_container<T, std::enable_if_t<
    !std::is_same<T, std::string>::value &&
    !is_bt_output_dict_container<T>::value &&
    is_bt_insertable<T>::value>>
: std::true_type {};


/// List specialization
template <typename T>
struct bt_serialize<T, std::enable_if_t<is_bt_input_list_container<T>::value>> {
    void operator()(std::ostream &os, const T &list) {
        os << 'l';
        for (const auto &v : list)
            bt_serialize<std::remove_cv_t<typename T::value_type>>{}(os, v);
        os << 'e';
    }
};
template <typename T>
struct bt_deserialize<T, std::enable_if_t<is_bt_output_list_container<T>::value>> {
    using value_type = typename T::value_type;
    void operator()(std::istream &is, T &list) {
        bt_expect<bt_deserialize_invalid_type>(is, 'l');
        list.clear();
        bt_deserialize<value_type> deserializer;
        while (is.peek() != 'e') {
            bt_no_eof(is);
            value_type v;
            try {
                deserializer(is, v);
            } catch (const bt_deserialize_invalid_type &e) {
                // Rethrow a sub-element invalid type as a regular error (because the type *was* a list)
                throw bt_deserialize_invalid(e.what());
            }
            list.insert(list.end(), std::move(v));
        }
        bt_expect(is, 'e');
    }
};

/// variant visitor; serializes whatever is contained
class bt_serialize_visitor {
    std::ostream &os;
public:
    using result_type = void;
    bt_serialize_visitor(std::ostream &os) : os{os} {}
    template <typename T> void operator()(const T &val) const {
        bt_serialize<T>{}(os, val);
    }
};

template <typename T>
using is_bt_deserializable = std::integral_constant<bool,
    std::is_same<T, std::string>::value || std::is_integral<T>::value ||
    is_bt_output_dict_container<T>::value || is_bt_output_list_container<T>::value>;

template <typename SFINAE, typename Variant, typename... Ts>
struct bt_deserialize_try_variant_impl {
    void operator()(std::istream &, Variant &) {
        throw bt_deserialize_invalid("Deserialization failed: could not deserialize value into any variant type");
    }
};

template <typename... Ts, typename Variant>
void bt_deserialize_try_variant(std::istream &is, Variant &variant) {
    bt_deserialize_try_variant_impl<void, Variant, Ts...>{}(is, variant);
}


template <typename Variant, typename T, typename... Ts>
struct bt_deserialize_try_variant_impl<std::enable_if_t<is_bt_deserializable<T>::value>, Variant, T, Ts...> {
    void operator()(std::istream &is, Variant &variant) {
        // Try to load the T variant.  If deserialization fails with a invalid_type error then we
        // can try to next one (that error leaves the istream in the same state).
        try {
            T val;
            bt_deserialize<T>{}(is, val);
            variant = std::move(val);
        }
        catch (bt_deserialize_invalid_type &e) {
            bt_deserialize_try_variant<Ts...>(is, variant);
        }
        // Don't catch other exceptions: they aren't retriable failures
    }
};
template <typename Variant, typename T, typename... Ts>
struct bt_deserialize_try_variant_impl<std::enable_if_t<!is_bt_deserializable<T>::value>, Variant, T, Ts...> {
    void operator()(std::istream &is, Variant &variant) {
        // Unsupported deserialization type, skip ahead
        bt_deserialize_try_variant<Ts...>(is, variant);
    }
};

/// Serialize a boost::variant
template <typename... Ts>
struct bt_serialize<boost::variant<Ts...>> {
    void operator()(std::ostream &os, const boost::variant<Ts...> &val) {
        boost::apply_visitor(bt_serialize_visitor{os}, val);
    }
};

template <typename... Ts>
struct bt_deserialize<boost::variant<Ts...>> {
    void operator()(std::istream &is, boost::variant<Ts...> &val) {
        bt_deserialize_try_variant<Ts...>(is, val);
    }
};

template <>
struct bt_deserialize<bt_value, void> {
    void operator()(std::istream &is, bt_value &val);
};

#ifdef __cpp_lib_variant
/// C++17 std::variant support
template <typename... Ts>
struct bt_serialize<std::variant<Ts...>> {
    void operator()(std::ostream &os, const std::variant<Ts...> &val) {
        std::visit(bt_serialize_visitor{os}, val);
    }
};

template <typename... Ts>
struct bt_deserialize<std::variant<Ts...>> {
    void operator()(std::istream &is, std::variant<Ts...> &val) {
        bt_deserialize_try_variant<Ts...>(is, val);
    }
};
#endif

template <typename T>
struct bt_stream_serializer {
    const T &val;
    explicit bt_stream_serializer(const T &val) : val{val} {}
    operator std::string() const {
        std::ostringstream oss;
        oss << *this;
        return oss.str();
    }
};
template <typename T>
std::ostream &operator<<(std::ostream &os, const bt_stream_serializer<T> &s) {
    bt_serialize<T>{}(os, s.val);
    return os;
}

template <typename T>
struct bt_stream_deserializer {
    T &val;
    bt_stream_deserializer(T &val) : val{val} {}
};
template <typename T>
std::istream &operator>>(std::istream &is, bt_stream_deserializer<T> &s) {
    bt_deserialize<T>{}(is, s.val);
    return is;
};
template <typename T>
std::istream &operator>>(std::istream &is, bt_stream_deserializer<T> &&s) {
    bt_deserialize<T>{}(is, s.val);
    return is;
};

} // namespace detail


/// Returns a wrapper around a value reference that can serialize the value directly to an output
/// stream or return it as a string.  This class is intended to be used inline (i.e. without being
/// stored) as in:
///
///     int number = 42;
///     std::string encoded = bt_serialize(number);
///     // Equivalent:
///     //auto encoded = (std::string) bt_serialize(number);
///
///     std::list<int> my_list{{1,2,3}};
///     std::cout << bt_serialize(my_list);
///
/// While it is possible to store the returned object and use it, such as:
///
///     auto encoded = bt_serialize(42);
///     std::cout << encoded;
///
/// this approach is not generally recommended: the returned object stores a reference to the
/// passed-in type, which may not survive.  If doing this note that it is the caller's
/// responsibility to ensure the serializer is not used past the end of the lifetime of the value
/// being serialized.
///
/// Also note that serializing directly to an output stream is more efficient as no intermediate
/// string containing the entire serialization has to be constructed.
///
template <typename T>
detail::bt_stream_serializer<T> bt_serializer(const T &val) { return detail::bt_stream_serializer<T>{val}; }

/// Serializes into a std::string.  This is exactly equivalant to casting the above to a std::string.
template <typename T>
std::string bt_serialize(const T &val) { return bt_serializer(val); }

/// Returns a wrapper around a value non-const reference so that you can deserialize from an input
/// stream or a string using:
///
///     int value;
///     is >> bt_deserializer(value);
///
template <typename T>
detail::bt_stream_deserializer<T> bt_deserializer(T &val) { return detail::bt_stream_deserializer<T>{val}; }

/// Deserializes from a char * and size directly into `val`.  Usage:
///
///     const char *encoded = "i42e";
///     size_t n = 4;
///     int value;
///     bt_deserialize(encoded, n, value); // Sets value to 42
template <typename T, std::enable_if_t<!std::is_const<T>::value, int> = 0>
void bt_deserialize(const char *data, size_t len, T &val) {
    tools::one_shot_read_buffer buf{data, len};
    std::istream is{&buf};
    is >> bt_deserializer(val);
}

/// Deserializes the given string directly into `val`.  Usage:
///
///     std::string encoded = "i42e";
///     int value;
///     bt_deserialize(encoded, value); // Sets value to 42
///
template <typename T, std::enable_if_t<!std::is_const<T>::value, int> = 0>
void bt_deserialize(
#ifdef __cpp_lib_string_view
        std::string_view s,
#else
        const std::string &s,
#endif
        T &val) {
    return bt_deserialize(s.data(), s.size(), val);
}


/// Deserializes the given string into a `T`, which is returned.
///
///     std::string encoded = "li1ei2ei3ee"; // bt-encoded list of ints: [1,2,3]
///     auto mylist = bt_deserialize<std::list<int>>(encoded);
///
template <typename T>
T bt_deserialize(
#ifdef __cpp_lib_string_view
        std::string_view s
#else
        const std::string &s
#endif
        ) {
    T val;
    bt_deserialize(s, val);
    return val;
}

/// Deserializes the given C-style string into a `T`, which is returned.
///
///     char *encoded = "li1ei2ei3ee"; // bt-encoded list of ints: [1,2,3]
///     auto mylist = bt_deserialize_cstr<std::list<int>>(encoded, strlen(encoded));
///
template <typename T>
T bt_deserialize_cstr(const char *data, size_t len) {
    T val;
    bt_deserialize(data, len, val);
    return val;
}

/// Deserializes the given value into a generic `bt_value` boost::variant which is capable of
/// holding all possible BT-encoded values.
///
/// Example:
/// 
///     std::string encoded = "i42e";
///     auto val = bt_get(encoded);
///     int v = boost::get<int>(val); // fails unless the encoded value was actually an integer
///
inline bt_value bt_get(
#ifdef __cpp_lib_string_view
        std::string_view s
#else
        const std::string &s
#endif
        ) {
    return bt_deserialize<bt_value>(s);
}

/// Helper functions to extract a value of some integral type from a bt_value which contains an
/// integer.  Does range checking, throwing std::overflow_error if the stored value is outside the
/// range of the target type.
///
/// Example:
///
///     std::string encoded = "i123456789e";
///     auto val = bt_get(encoded);
///     auto v = get<uint32_t>(val); // throws if the decoded value doesn't fit in a uint32_t
template <typename IntType, std::enable_if_t<std::is_integral<IntType>::value, int> = 0>
IntType get_int(const bt_value &v) {
    // It's highly unlikely that this code ever runs on a non-2s-complement architecture, but check
    // at compile time if converting to a uint64_t (because while int64_t -> uint64_t is
    // well-defined, uint64_t -> int64_t only does the right thing under 2's complement).
    static_assert(!std::is_unsigned<IntType>::value || sizeof(IntType) != sizeof(int64_t) || -1 == ~0,
            "Non 2s-complement architecture not supported!");
    int64_t value = boost::get<int64_t>(v);
    if (sizeof(IntType) < sizeof(int64_t)) {
        if (value > static_cast<int64_t>(std::numeric_limits<IntType>::max())
                || value < static_cast<int64_t>(std::numeric_limits<IntType>::min()))
            throw std::overflow_error("Unable to extract integer value: stored value is outside the range of the requested type");
    }
    return static_cast<IntType>(value);
}

}
