#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include "serialization.h"

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::vector<T> &v);
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::vector<T> &v);

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::deque<T> &v);
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::deque<T> &v);

template<typename K, typename V>
class serializable_unordered_map: public std::unordered_map<K, V>
{
public:
  typedef typename std::pair<K, V> value_type;
  typename std::unordered_map<K, V> &parent() { return *this; }
};

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_unordered_map<K, V> &v);
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_unordered_map<K, V> &v);

template<typename K, typename V>
class serializable_map: public std::map<K, V>
{
public:
  typedef typename std::pair<K, V> value_type;
  typename std::map<K, V> &parent() { return *this; }
};

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_map<K, V> &v);
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_map<K, V> &v);

template<typename K, typename V>
class serializable_unordered_multimap: public std::unordered_multimap<K, V>
{
public:
  typedef typename std::pair<K, V> value_type;
  typename std::unordered_multimap<K, V> &parent() { return *this; }
};

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_unordered_multimap<K, V> &v);
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_unordered_multimap<K, V> &v);

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::unordered_set<T> &v);
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::unordered_set<T> &v);

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::set<T> &v);
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::set<T> &v);

namespace serialization
{
  namespace detail
  {
    template <typename T> void do_reserve(std::vector<T> &c, size_t N) { c.reserve(N); }
    template <typename T> void do_add(std::vector<T> &c, T &&e) { c.emplace_back(std::forward<T>(e)); }

    template <typename T> void do_add(std::deque<T> &c, T &&e) { c.emplace_back(std::forward<T>(e)); }

    template <typename K, typename V> void do_add(serializable_unordered_map<K, V> &c, std::pair<K, V> &&e) { c.insert(std::forward<std::pair<K, V>>(e)); }

    template <typename K, typename V> void do_add(serializable_map<K, V> &c, std::pair<K, V> &&e) { c.insert(std::forward<std::pair<K, V>>(e)); }

    template <typename K, typename V> void do_add(serializable_unordered_multimap<K, V> &c, std::pair<K, V> &&e) { c.insert(std::forward<std::pair<K, V>>(e)); }

    template <typename T> void do_add(std::unordered_set<T> &c, T &&e) { c.insert(std::forward<T>(e)); }

    template <typename T> void do_add(std::set<T> &c, T &&e) { c.insert(std::forward<T>(e)); }
  }
}

#include "container.h"

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::vector<T> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::vector<T> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::deque<T> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::deque<T> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_unordered_map<K, V> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_unordered_map<K, V> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_map<K, V> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_map<K, V> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<false> &ar, serializable_unordered_multimap<K, V> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, typename K, typename V> bool do_serialize(Archive<true> &ar, serializable_unordered_multimap<K, V> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::unordered_set<T> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::unordered_set<T> &v) { return do_serialize_container(ar, v); }

template <template <bool> class Archive, class T> bool do_serialize(Archive<false> &ar, std::set<T> &v) { return do_serialize_container(ar, v); }
template <template <bool> class Archive, class T> bool do_serialize(Archive<true> &ar, std::set<T> &v) { return do_serialize_container(ar, v); }
