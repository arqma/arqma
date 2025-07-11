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

if(DEPENDS)
  add_library(libzmq_vendor STATIC IMPORTED GLOBAL)
  set_target_properties(libzmq_vendor PROPERTIES IMPORTED_LOCATION ${ZMQ_LIBRARIES})
  target_include_directories(libzmq_vendor INTERFACE ${ZMQ_INCLUDE_DIRS})
  statusinfo("${ZMQ_INCLUDE_DIRS} ${ZMQ_LIBRARIES}")
elseif(APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZMQ libzmq>=4.3.2 REQUIRED IMPORTED_TARGET)
  add_library(libzmq_vendor INTERFACE)
  target_link_libraries(libzmq_vendor INTERFACE PkgConfig::ZMQ)
  set_target_properties(libzmq_vendor PROPERTIES IMPORTED_TARGET_LOCATION ${ZMQ_LIBRARIES})
  target_include_directories(libzmq_vendor INTERFACE ${ZMQ_INCLUDE_DIRS})
else()
  include(ExternalProject)
  include(ProcessorCount)

  set(zmq_prefix ${CMAKE_BINARY_DIR}/external/libzmq)
  set(zmq_patch git stash && patch -p1 < ${PROJECT_SOURCE_DIR}/utils/patches/zmq.patch)
  set(zmq_autogen ./autogen.sh)
  set(zmq_configure ./configure --prefix=${zmq_prefix})

  set(zmq_configure ${zmq_configure} --without-docs --enable-static=yes --enable-shared=no --with-libsodium=yes --with-pgm=no --with-norm=no --disable-perf --disable-Werror --disable-drafts --enable-option-checking --enable-libunwind=no)

  ExternalProject_add(libzmq_external
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libzmq
    PREFIX ${zmq_prefix}
    PATCH_COMMAND ${zmq_patch}
    CONFIGURE_COMMAND ${zmq_autogen} && ${zmq_configure}
    BUILD_COMMAND make -j${PROCESSOR_COUNT}
    INSTALL_COMMAND ${MAKE}
    BUILD_IN_SOURCE ON
    BUILD_BY_PRODUCTS ${zmq_prefix}/lib/libzmq.a ${zmq_prefix}/include
  )
  add_library(libzmq_vendor STATIC IMPORTED GLOBAL)
  add_dependencies(libzmq_vendor libzmq_external)
  set_property(TARGET libzmq_vendor PROPERTY IMPORTED_LOCATION ${zmq_prefix}/lib/libzmq.a)
  target_include_directories(libzmq_vendor INTERFACE ${zmq_prefix}/include)

  set(ZMQ_INCLUDE_DIRS ${zmq_prefix}/include CACHE STRING "ZMQ Include path")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(UNBOUND libunbound REQUIRED IMPORTED_TARGET)
statusinfo("Using in-tree miniupnpc")
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

if(UNBOUND_FOUND)
  statusinfo("Found PkgConfig::libunbound")
  target_link_libraries(libunbound INTERFACE PkgConfig::UNBOUND)
  set_target_properties(libunbound PROPERTIES IMPORTED_TARGET_LOCATION ${UNBOUND_LIBRARIES})
  target_include_directories(libunbound INTERFACE ${UNBOUND_INCLUDE_DIRS})
else()
  statusinfo("Using in-tree unbound")
  add_subdirectory(unbound)
  target_link_libraries(libunbound INTERFACE unbound)
  target_include_directories(libunbound INTERFACE unbound/libunbound)
  set(UNBOUND_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/unbound/libunbound" PARENT_SCOPE)
endif()

add_subdirectory(easylogging++)
add_subdirectory(qrcodegen)
add_subdirectory(randomarq EXCLUDE_FROM_ALL)
