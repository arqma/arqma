# Copyright (c) 2018-2022, The Arqma Network
# Copyright (c) 2014-2018, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set(epee_include_dir "${CMAKE_CURRENT_SOURCE_DIR}/../include")

arqma_find_all_headers(epee_headers_public "${epee_include_dir}")

add_library(epee
    byte_slice.cpp
    byte_stream.cpp
    hex.cpp
    abstract_http_client.cpp
    http_auth.cpp
    mlog.cpp
    buffer.cpp
    net_helper.cpp
    net_utils_base.cpp
    connection_basic.cpp
    file_io_utils.cpp
    http_base.cpp
    levin_base.cpp
    memwipe.c
    misc_language.cpp
    mlocker.cpp
    net_parse_helpers.cpp
    net_ssl.cpp
    network_throttle.cpp
    network_throttle-detail.cpp
    portable_storage.cpp
    string_tools.cpp
    tiny_ini.cpp
    wipeable_string.cpp

    ${epee_headers_public})

if(TARGET readline)
  target_sources(epee PUBLIC readline_buffer.cpp)
  target_link_libraries(epee PUBLIC readline)
endif()

set_property(SOURCE memwipe.c PROPERTY C_STANDARD 11)

# Build and install libepee if we're building for GUI
if(BUILD_GUI_DEPS)
  if(IOS)
    set(lib_folder lib-${ARCH})
  else()
    set(lib_folder lib)
  endif()
  install(TARGETS epee ARCHIVE DESTINATION ${lib_folder})
endif()

target_link_libraries(epee
  PUBLIC
    easylogging
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${OPENSSL_LIBRARIES}
  PRIVATE
    ${EXTRA_LIBRARIES})

target_include_directories(epee
  PUBLIC
    "${epee_include_dir}"
    "${OPENSSL_INCLUDE_DIR}")