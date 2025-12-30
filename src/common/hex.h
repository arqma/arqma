#pragma once
#include <string>
#include <string_view>
#include <array>
#include <iterator>
#include <cassert>
#include <type_traits>
#include "span.h"

namespace tools {

namespace detail {

struct hex_table {
    char from_hex_lut[256];
    char to_hex_lut[16];
    constexpr hex_table() noexcept : from_hex_lut{}, to_hex_lut{} {
        for (unsigned char c = 0; c < 10; c++) {
            from_hex_lut[(unsigned char)('0' + c)] =  0  + c;
            to_hex_lut[  (unsigned char)( 0  + c)] = '0' + c;
        }
        for (unsigned char c = 0; c < 6; c++) {
            from_hex_lut[(unsigned char)('a' + c)] = 10  + c;
            from_hex_lut[(unsigned char)('A' + c)] = 10  + c;
            to_hex_lut[  (unsigned char)(10  + c)] = 'a' + c;
        }
    }
    constexpr char from_hex(unsigned char c) const noexcept { return from_hex_lut[c]; }
    constexpr char to_hex(unsigned char b) const noexcept { return to_hex_lut[b]; }
} constexpr hex_lut;

static_assert(hex_lut.from_hex('a') == 10 && hex_lut.from_hex('F') == 15 && hex_lut.to_hex(13) == 'd', "");

} // namespace detail

template <typename InputIt, typename OutputIt>
void to_hex(InputIt begin, InputIt end, OutputIt out) {
    static_assert(sizeof(decltype(*begin)) == 1, "to_hex requires chars/bytes");
    for (; begin != end; ++begin) {
        uint8_t c = static_cast<uint8_t>(*begin);
        *out++ = detail::hex_lut.to_hex(c >> 4);
        *out++ = detail::hex_lut.to_hex(c & 0x0f);
    }
}

template <typename It>
std::string to_hex(It begin, It end) {
    std::string hex;
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<It>::iterator_category>)
        hex.reserve(2 * std::distance(begin, end));
    to_hex(begin, end, std::back_inserter(hex));
    return hex;
}

template <typename CharT>
std::string to_hex(std::basic_string_view<CharT> s) { return to_hex(s.begin(), s.end()); }
inline std::string to_hex(std::string_view s) { return to_hex<>(s); }

template <typename It>
constexpr bool is_hex(It begin, It end) {
    static_assert(sizeof(decltype(*begin)) == 1, "is_hex requires chars/bytes");
    for (; begin != end; ++begin) {
        if (detail::hex_lut.from_hex(static_cast<unsigned char>(*begin)) == 0 && static_cast<unsigned char>(*begin) != '0')
            return false;
    }
    return true;
}

template <typename CharT>
constexpr bool is_hex(std::basic_string_view<CharT> s) { return is_hex(s.begin(), s.end()); }
constexpr bool is_hex(std::string_view s) { return is_hex(s.begin(), s.end()); }

constexpr char from_hex_digit(unsigned char x) noexcept {
    return detail::hex_lut.from_hex(x);
}

constexpr char from_hex_pair(unsigned char a, unsigned char b) noexcept { return (from_hex_digit(a) << 4) | from_hex_digit(b); }

template <typename InputIt, typename OutputIt>
void from_hex(InputIt begin, InputIt end, OutputIt out) {
    using std::distance;
    assert(distance(begin, end) % 2 == 0);
    while (begin != end) {
        auto a = *begin++;
        auto b = *begin++;
        *out++ = from_hex_pair(static_cast<unsigned char>(a), static_cast<unsigned char>(b));
    }
}

template <typename It>
std::string from_hex(It begin, It end) {
    std::string bytes;
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<It>::iterator_category>)
        bytes.reserve(std::distance(begin, end) / 2);
    from_hex(begin, end, std::back_inserter(bytes));
    return bytes;
}

template <typename CharT>
std::string from_hex(std::basic_string_view<CharT> s) { return from_hex(s.begin(), s.end()); }
inline std::string from_hex(std::string_view s) { return from_hex<>(s); }

template <typename T, typename = std::enable_if_t<
  !std::is_const_v<T> && (std::is_trivially_copyable_v<T> || epee::is_byte_spannable<T>)
>>
bool hex_to_type(std::string_view hex, T& x) {
  if (!is_hex(hex) || hex.size() != 2*sizeof(T))
    return false;
  from_hex(hex.begin(), hex.end(), reinterpret_cast<char*>(&x));
  return true;
}

/// Converts a standard layout, padding-free type into a hex string of its contents.
template <typename T, typename = std::enable_if_t<
  (std::is_standard_layout_v<T> && std::has_unique_object_representations_v<T>)
    || epee::is_byte_spannable<T>
>>
std::string type_to_hex(const T& val) {
  return to_hex(std::string_view{reinterpret_cast<const char*>(&val), sizeof(val)});
}

}