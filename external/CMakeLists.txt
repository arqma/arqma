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

set(LIBZMQ_PREFIX ${CMAKE_BINARY_DIR}/libzmq)
set(ZeroMQ_VERSION 4.3.2)
set(LIBZMQ_URL https://github.com/zeromq/libzmq/releases/download/v${ZeroMQ_VERSION}/zeromq-${ZeroMQ_VERSION}.tar.gz)
set(LIBZMQ_HASH SHA512=b6251641e884181db9e6b0b705cced7ea4038d404bdae812ff47bdd0eed12510b6af6846b85cb96898e253ccbac71eca7fe588673300ddb9c3109c973250c8e4)

set(LIBPGM_PREFIX ${CMAKE_BINARY_DIR}/libpgm)
set(LIBPGM_VERSION 5.2.122)
set(LIBPGM_URL https://github.com/steve-o/openpgm/archive/release-5-2-122.tar.gz)
set(LIBPGM_HASH SHA512=f8b29fb69acb548536179262665c0cdcc8bb1510d3e970d6abebaf33e301f87fbfa7e2c46e387973d8b65470350af66b3130482269b9fbb15fe7bf98b52f6404)

set(LIBNORM_PREFIX ${CMAKE_BINARY_DIR}/libnorm)
set(LIBNORM_GITHUB https://github.com/USNavalResearchLaboratory/norm.git)

file(MAKE_DIRECTORY ${LIBPGM_PREFIX}/include)
file(MAKE_DIRECTORY ${LIBZMQ_PREFIX}/include)
file(MAKE_DIRECTORY ${LIBNORM_PREFIX}/include)

include(ProcessorCount)
include(ExternalProject)

set(LIBPGM_BOOTSTRAP ./bootstrap.sh)
set(LIBPGM_CONFIGURE ./configure --prefix=${LIBPGM_PREFIX} --enable-static=yes --enable-shared=no)
set(LIBPGM_PATCH patch -p1 openpgm/pgm/version_generator.py < ${PROJECT_SOURCE_DIR}/utils/patches/pgm.patch)

if(APPLE OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  list(APPEND ${LIBPGM_CONFIGURE} "--host=x86_64-apple-darwin")
  list(APPEND ${LIBZMQ_CONFIGURE} "--host=x86_64-apple-darwin")
endif()

ExternalProject_Add(openpgm
      BUILD_IN_SOURCE 1
      BUILD_ALWAYS TRUE
      PREFIX ${LIBPGM_PREFIX}
      URL ${LIBPGM_URL}
      URL_HASH ${LIBPGM_HASH}
      PATCH_COMMAND ${LIBPGM_PATCH}
      SOURCE_DIR ${LIBPGM_PREFIX}/src
      SOURCE_SUBDIR openpgm/pgm
      STAMP_DIR ${LIBPGM_PREFIX}
      CONFIGURE_COMMAND ${LIBPGM_BOOTSTRAP} && ${LIBPGM_CONFIGURE}
      BUILD_COMMAND make -j${PROCESSOR_COUNT}
      INSTALL_COMMAND ${MAKE}
      BUILD_BYPRODUCTS ${LIBPGM_PREFIX}/lib/libpgm.a ${LIBPGM_PREFIX}/include/pgm-5.2
)

add_library(libpgm_vendor STATIC IMPORTED GLOBAL)
add_dependencies(libpgm_vendor openpgm)
set_property(TARGET libpgm_vendor PROPERTY IMPORTED_LOCATION ${LIBPGM_PREFIX}/lib/libpgm.a)
target_include_directories(libpgm_vendor INTERFACE ${LIBPGM_PREFIX}/include/pgm-5.2)

set(PGM_INCLUDE_DIRS ${LIBPGM_PREFIX}/include/pgm-5.2 CACHE STRING "PGM Include path")

set(LIBNORM_CONFIGURE ./waf configure --prefix=${LIBNORM_PREFIX})
set(LIBNORM_BUILD ./waf build --enable-static-library)
set(LIBNORM_INSTALL ./waf install)

ExternalProject_Add(norm
      BUILD_IN_SOURCE 1
      BUILD_ALWAYS TRUE
      PREFIX ${LIBNORM_PREFIX}
      GIT_REPOSITORY ${LIBNORM_GITHUB}
      GIT_SUBMODULES_RECURSE TRUE
      CONFIGURE_COMMAND ${LIBNORM_CONFIGURE}
      BUILD_COMMAND ${LIBNORM_BUILD}
      INSTALL_COMMAND ${LIBNORM_INSTALL}
      BUILD_BYPRODUCTS ${LIBNORM_PREFIX}/lib/libnorm.a ${LIBNORM_PREFIX}/include
)

add_library(libnorm_vendor STATIC IMPORTED GLOBAL)
add_dependencies(libnorm_vendor norm)
set_property(TARGET libnorm_vendor PROPERTY IMPORTED_LOCATION ${LIBNORM_PREFIX}/lib/libnorm.a)
target_include_directories(libnorm_vendor INTERFACE ${LIBNORM_PREFIX}/include)

set(NORM_INCLUDE_DIRS ${LIBNORM_PREFIX}/include CACHE STRING "Libnorm include dir")

set(ZeroMQ_PATCH patch -p1 src/thread.cpp < ${PROJECT_SOURCE_DIR}/utils/patches/zmq.patch)
set(ZeroMQ_PGM pgm_CFLAGS="-I${LIBPGM_PREFIX}/include/pgm-5.2" pgm_LIBS="-L${LIBPGM_PREFIX}/lib -lpgm -lpthread -lm")
set(ZeroMQ_NORM norm_CFLAGS="-I${LIBNORM_PREFIX}/include" norm_LIBS="-L${LIBNORM_PREFIX}/lib -lnorm -lpthread -lm")
set(ZeroMQ_CONFIGURE ./configure --prefix=${LIBZMQ_PREFIX} --without-docs --enable-static=yes --enable-shared=no --with-libsodium=yes --with-pgm=yes --with-norm=yes --disable-perf --disable-Werror --disable-drafts --enable-option-checking --enable-libunwind=no ${ZeroMQ_PGM} ${ZeroMQ_NORM})

ExternalProject_Add(libzmq_external
      BUILD_IN_SOURCE ON
      PREFIX ${LIBZMQ_PREFIX}
      URL ${LIBZMQ_URL}
      URL_HASH ${LIBZMQ_HASH}
      PATCH_COMMAND ${ZeroMQ_PATCH}
      DEPENDS openpgm norm
      CONFIGURE_COMMAND ${ZeroMQ_CONFIGURE}
      BUILD_COMMAND make -j${PROCESSOR_COUNT}
      INSTALL_COMMAND ${MAKE}
      BUILD_BYPRODUCTS ${LIBZMQ_PREFIX}/lib/libzmq.a ${LIBZMQ_PREFIX}/include
)

add_library(libzmq_vendor STATIC IMPORTED GLOBAL)
add_dependencies(libzmq_vendor libzmq_external)
set_property(TARGET libzmq_vendor PROPERTY IMPORTED_LOCATION ${LIBZMQ_PREFIX}/lib/libzmq.a)
target_include_directories(libzmq_vendor
                           INTERFACE ${LIBZMQ_PREFIX}/include
                           INTERFACE ${LIBPGM_PREFIX}/include/pgm-5.2/pgm/include
                           INTERFACE ${LIBNORM_PREFIX}/include)

set(ZMQ_INCLUDE_DIRS ${LIBZMQ_PREFIX}/include CACHE STRING "ZMQ Include path")

add_subdirectory(easylogging++)
add_subdirectory(qrcodegen)
add_subdirectory(randomarq EXCLUDE_FROM_ALL)
