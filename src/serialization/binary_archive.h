// Copyright (c) 2018-2022, The Arqma Network
// Copyright (c) 2014-2018, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

/*! \file binary_archive.h
 *
 * Portable (low-endian) binary archive */
#pragma once

#include <cassert>
#include <iostream>
#include <iterator>
#include <boost/type_traits/make_unsigned.hpp>

#include "common/varint.h"
#include "warnings.h"

/* I have no clue what these lines means */
PUSH_WARNINGS
DISABLE_VS_WARNINGS(4244)

//TODO: fix size_t warning in x32 platform

/*! \struct binary_archive_base
 *
 * \brief base for the binary archive type
 *
 * \detailed It isn't used outside of this file, which its only
 * purpse is to define the functions used for the binary_archive. Its
 * a header, basically. I think it was declared simply to save typing...
 */
template <class Stream, bool IsSaving>
struct binary_archive_base
{
  typedef Stream stream_type;
  typedef binary_archive_base<Stream, IsSaving> base_type;
  typedef boost::mpl::bool_<IsSaving> is_saving;

  typedef uint8_t variant_tag_type;

  explicit binary_archive_base(stream_type &s) : stream_(s) { }

  /* definition of standard API functions */
  void tag(const char *) { }
  void begin_object() { }
  void end_object() { }
  void begin_variant() { }
  void end_variant() { }
  /* I just want to leave a comment saying how this line really shows
     flaws in the ownership model of many OOP languages, that is all. */
  stream_type &stream() { return stream_; }

protected:
  stream_type &stream_;
};

/* \struct binary_archive
 *
 * \brief the actually binary archive type
 *
 * \detailed The boolean template argument /a W is the is_saving
 * parameter for binary_archive_base.
 *
 * The is_saving parameter says whether the archive is being read from
 * (false) or written to (true)
 */
template <bool W>
struct binary_archive;


template <>
struct binary_archive<false> : public binary_archive_base<std::istream, false>
{

  explicit binary_archive(stream_type &s) : base_type(s) {
    stream_type::pos_type pos = stream_.tellg();
    stream_.seekg(0, std::ios_base::end);
    eof_pos_ = stream_.tellg();
    stream_.seekg(pos);
  }

  template <class T>
  void serialize_int(T &v)
  {
    serialize_uint(*(typename boost::make_unsigned<T>::type *)&v);
  }

  /*! \fn serialize_uint
   *
   * \brief serializes an unsigned integer
   */
  template <class T>
  void serialize_uint(T &v, size_t width = sizeof(T))
  {
    T ret = 0;
    unsigned shift = 0;
    for (size_t i = 0; i < width; i++) {
      //std::cerr << "tell: " << stream_.tellg() << " value: " << ret << std::endl;
      char c;
      stream_.get(c);
      T b = (unsigned char)c;
      ret += (b << shift);	// can this be changed to OR, i think it can.
      shift += 8;
    }
    v = ret;
  }

  void serialize_blob(void *buf, size_t len, const char *delimiter="")
  {
    stream_.read((char *)buf, len);
  }

  template <class T>
  void serialize_varint(T &v)
  {
    serialize_uvarint(*(typename boost::make_unsigned<T>::type *)(&v));
  }

  template <class T>
  void serialize_uvarint(T &v)
  {
    typedef std::istreambuf_iterator<char> it;
    if(tools::read_varint(it(stream_), it(), v) < 0)
      stream_.setstate(std::ios_base::failbit);
  }

  void begin_array(size_t &s)
  {
    serialize_varint(s);
  }

  void begin_array() { }
  void delimit_array() { }
  void end_array() { }

  void begin_string(const char *delimiter /*="\""*/) { }
  void end_string(const char *delimiter   /*="\""*/) { }

  void read_variant_tag(variant_tag_type &t) {
    serialize_int(t);
  }

  size_t remaining_bytes() {
    if (!stream_.good())
      return 0;
    //std::cerr << "tell: " << stream_.tellg() << std::endl;
    assert(stream_.tellg() <= eof_pos_);
    return eof_pos_ - stream_.tellg();
  }
protected:
  std::streamoff eof_pos_;
};

template <>
struct binary_archive<true> : public binary_archive_base<std::ostream, true>
{
  explicit binary_archive(stream_type &s) : base_type(s) { }

  template <class T>
  void serialize_int(T v)
  {
    serialize_uint(static_cast<typename boost::make_unsigned<T>::type>(v));
  }
  template <class T>
  void serialize_uint(T v)
  {
    for (size_t i = 0; i < sizeof(T); i++) {
      stream_.put((char)(v & 0xff));
      if constexpr (1 < sizeof(T)) { v >>= 8; }
    }
  }

  void serialize_blob(void *buf, size_t len, const char *delimiter="")
  {
    stream_.write((char *)buf, len);
  }

  template <class T>
  void serialize_varint(T &v)
  {
    serialize_uvarint(*(typename boost::make_unsigned<T>::type *)(&v));
  }

  template <class T>
  void serialize_uvarint(T &v)
  {
    typedef std::ostreambuf_iterator<char> it;
    tools::write_varint(it(stream_), v);
  }
  void begin_array(size_t s)
  {
    serialize_varint(s);
  }
  void begin_array() { }
  void delimit_array() { }
  void end_array() { }

  void begin_string(const char *delimiter="\"") { }
  void end_string(const char *delimiter="\"") { }

  void write_variant_tag(variant_tag_type t) {
    serialize_int(t);
  }
};

POP_WARNINGS
