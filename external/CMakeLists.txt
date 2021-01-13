# Copyright (c) 2018-2020, The Arqma Network
# Copyright (c) 2014-2020, The Monero Project
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


# This is broken up into two parts: first we check for miniupnp, compile it if we can't
# find it, and thereafter we check for libunbound, and compile it if we can't find it.
# We always compile if we are building statically to reduce static dependency issues...
# ...except for FreeBSD, because FreeBSD is a special case that doesn't play well with
# others.

if(DEPENDS)
  add_library(libzmq_vendor STATIC IMPORTED GLOBAL)
  set_target_properties(libzmq_vendor PROPERTIES IMPORTED_LOCATION ${ZMQ_LIBRARIES})
  target_include_directories(libzmq_vendor INTERFACE ${ZMQ_INCLUDE_DIRS})
  statusinfo("${ZMQ_INCLUDE_DIRS} ${ZMQ_LIBRARIES}")
else()
  include(ExternalProject)
  include(ProcessorCount)

  set(zmq_prefix ${CMAKE_BINARY_DIR}/external/libzmq)
  set(zmq_patch patch -R -p0 -i ${PROJECT_SOURCE_DIR}/utils/patches/zmq.patch)
  set(zmq_autogen ./autogen.sh)
  set(zmq_configure ./configure --prefix=${zmq_prefix})
  if(APPLE OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(zmq_configure ${zmq_configure} --host=x86_64-apple-darwin)
  endif()

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

  # NOTE: If STATIC is true, CMAKE_FIND_LIBRARY_SUFFIXES has been reordered.
  # unbound has config tests which used OpenSSL libraries, so -ldl may need to
  # be set in this case.
  # The unbound CMakeLists.txt can set it, since it's also needed for the
  # static OpenSSL libraries set up there after with target_link_libraries.

statusinfo("Using in-tree unbound")
add_subdirectory(unbound)
target_link_libraries(libunbound INTERFACE unbound)
target_include_directories(libunbound INTERFACE unbound/libunbound)
set(UNBOUND_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/unbound/libunbound" PARENT_SCOPE)

add_subdirectory(easylogging++)
add_subdirectory(qrcodegen)
add_subdirectory(randomarq EXCLUDE_FROM_ALL)
