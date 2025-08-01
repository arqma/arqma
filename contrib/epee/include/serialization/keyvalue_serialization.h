// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include "misc_log_ex.h"
#include "enableable.h"
#include "keyvalue_serialization_overloads.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "serialization"

namespace epee
{
  /************************************************************************/
  /* Serialize map declarations                                           */
  /************************************************************************/
#define BEGIN_KV_SERIALIZE_MAP() \
public: \
  template<class t_storage> \
  bool store( t_storage& st, typename t_storage::hsection hparent_section = nullptr) const\
  {\
    return serialize_map<true>(*this, st, hparent_section);\
  }\
  template<class t_storage> \
  bool _load( t_storage& stg, typename t_storage::hsection hparent_section = nullptr)\
  {\
    return serialize_map<false>(*this, stg, hparent_section);\
  }\
  template<class t_storage> \
  bool load( t_storage& stg, typename t_storage::hsection hparent_section = nullptr)\
  {\
    try{\
    return serialize_map<false>(*this, stg, hparent_section);\
    }\
    catch(const std::exception& err) \
    { \
      (void)(err); \
      LOG_ERROR("Exception on unserializing: " << err.what());\
      return false; \
    }\
  }\
  template<bool is_store, class this_type, class t_storage> \
  static bool serialize_map(this_type& this_ref, t_storage& stg, typename t_storage::hsection hparent_section) \
  {

#define KV_SERIALIZE_VALUE(variable) \
  epee::serialization::selector<is_store>::serialize(variable, stg, hparent_section, #variable);

#define KV_SERIALIZE_N(variable, val_name) \
  epee::serialization::selector<is_store>::serialize(this_ref.variable, stg, hparent_section, val_name);

  template<typename T> inline void serialize_default(const T &t, T v) { }
  template<typename T> inline void serialize_default(T &t, T v) { t = v; }

#define KV_SERIALIZE_OPT_N(variable, val_name, default_value) \
  do { \
    if(!epee::serialization::selector<is_store>::serialize(this_ref.variable, stg, hparent_section, val_name)) \
      epee::serialize_default(this_ref.variable, default_value); \
  } while (0);

#define KV_SERIALIZE_OPT_N2(variable, val_name, default_value) \
do { \
  if (!epee::serialization::selector<is_store>::serialize(this_ref.variable, stg, hparent_section, val_name)) { \
    epee::serialize_default(this_ref.variable, default_value); \
  } else { \
    this_ref.explicitly_set = true; \
  } \
} while (0);

#define KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(variable, val_name) \
  epee::serialization::selector<is_store>::serialize_t_val_as_blob(this_ref.variable, stg, hparent_section, val_name);

#define KV_SERIALIZE_VAL_POD_AS_BLOB_N(variable, val_name) \
  static_assert(std::is_trivially_copyable_v<decltype(this_ref.variable)>, "t_type must be a trivially copyable type."); \
  static_assert(std::is_standard_layout_v<decltype(this_ref.variable)>, "t_type must be a standard layout type."); \
  KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(variable, val_name)

#define KV_SERIALIZE_VAL_POD_AS_BLOB_OPT_N(variable, val_name, default_value) \
  do { \
    static_assert(std::is_trivially_copyable_v<decltype(this_ref.variable)>, "t_type must be a trivially copyable type."); \
    static_assert(std::is_standard_layout_v<decltype(this_ref.variable)>, "t_type must be a standard layout type."); \
    bool ret = KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(variable, val_name); \
    if (!ret) \
      epee::serialize_default(this_ref.variable, default_value); \
  } while(0);

#define KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(variable, val_name) \
  epee::serialization::selector<is_store>::serialize_stl_container_pod_val_as_blob(this_ref.variable, stg, hparent_section, val_name);

#define END_KV_SERIALIZE_MAP() return true;}

#define KV_SERIALIZE(variable)                            KV_SERIALIZE_N(variable, #variable)
#define KV_SERIALIZE_VAL_POD_AS_BLOB(variable)            KV_SERIALIZE_VAL_POD_AS_BLOB_N(variable, #variable)
#define KV_SERIALIZE_VAL_POD_AS_BLOB_OPT(variable, def)   KV_SERIALIZE_VAL_POD_AS_BLOB_OPT_N(variable, #variable, def)
#define KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(variable)      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE_N(variable, #variable) //skip is_trivially_copyable and is_standard_layout compile time check
#define KV_SERIALIZE_CONTAINER_POD_AS_BLOB(variable)      KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(variable, #variable)
#define KV_SERIALIZE_OPT(variable,default_value)          KV_SERIALIZE_OPT_N(variable, #variable, default_value)
#define KV_SERIALIZE_OPT2(variable,default_value)         KV_SERIALIZE_OPT_N2(variable, #variable, default_value)
#define KV_SERIALIZE_ENUM(enum_) do { \
  using enum_t = std::remove_const_t<decltype(this_ref.enum_)>; \
  using int_t = std::underlying_type_t<enum_t>; \
  int_t int_value = is_store ? static_cast<int_t>(this_ref.enum_) : 0; \
  epee::serialization::selector<is_store>::serialize(int_value, stg, hparent_section, #enum_); \
  if (!is_store) \
    const_cast<enum_t&>(this_ref.enum_) = static_cast<enum_t>(int_value); \
} while(0);

}
