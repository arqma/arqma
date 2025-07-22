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
#
# Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

set(DEFAULT_WITH_MINIUPNPC ON)
if(ANDROID OR IOS)
  set(DEFAULT_WITH_MINIUPNPC OFF)
endif()

option(WITH_MINIUPNPC "Enable miniupnpc support for IGD NAT hole punching" ${DEFAULT_WITH_MINIUPNPC})

find_package(PkgConfig REQUIRED)

if(WITH_MINIUPNPC)
  pkg_check_modules(MINIUPNPC miniupnpc>=2.1)
endif()
pkg_check_modules(UNBOUND libunbound)

if(NOT WITH_MINIUPNPC)
  message(STATUS "miniupnpc support disabled")
  target_compile_definitions(miniupnpc INTERFACE WITHOUT_MINIUPNPC)
elseif(MINIUPNPC_FOUND)
  message(STATUS "Found miniupnpc")
  link_dep_libs(miniupnpc INTERFACE "${MINIUPNPC_LIBRARY_DIRS}" ${MINIUPNPC_LIBRARIES})
  target_include_directories(miniupnpc INTERFACE ${MINIUPNPC_INCLUDE_DIRS})
else()
  message(STATUS "Using in-tree miniupnpc")
  add_subdirectory(miniupnp/miniupnpc)
  if(MSVC)
    target_compile_options(libminiupnpc-static PRIVATE -wd4244 -wd4267)
  else()
    target_compile_options(libminiupnpc-static PRIVATE -Wno-undef -Wno-unused-result -Wno-unused-value)
  endif()
  if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    target_compile_definitions(libminiupnpc-static PRIVATE _NETBSD_SOURCE)
  endif()

  target_link_libraries(miniupnpc INTERFACE libminiupnpc-static)
endif()

add_subdirectory(easylogging++)
add_subdirectory(qrcodegen)
add_subdirectory(randomarq EXCLUDE_FROM_ALL)
