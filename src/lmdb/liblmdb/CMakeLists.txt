# Copyright (c) 2018-2022, The Arqma Network
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


add_library(lmdb
  mdb.c
  midl.c)

target_include_directories(lmdb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lmdb PRIVATE ${CMAKE_THREAD_LIBS_INIT})

if(FREEBSD)
  add_definitions(-DMDB_DSYNC=O_SYNC)
endif()

if(ANDROID)
  add_definitions("-DANDROID=1")
endif()

if(${ARCH_WIDTH} EQUAL 32)
  target_compile_definitions(lmdb PUBLIC -DMDB_VL32)
endif()

if(WIN32)
  set(CMAKE_C_FLAGS "-Wall -Wno-unused-but-set-variable -Wno-array-bounds -Wno-stringop-overflow -Wno-return-local-addr -Wno-format -Wno-format-truncation -Wno-unknown-pragmas")
endif()

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-truncation")
endif()

# GUI/libwallet install target
if(BUILD_GUI_DEPS)
  if(IOS)
    set(lib_folder lib-${ARCH})
  else()
    set(lib_folder lib)
  endif()
  install(TARGETS lmdb
    ARCHIVE DESTINATION ${lib_folder}
    LIBRARY DESTINATION ${lib_folder})
endif()
