# Copyright (c) 2018 - 2026, The Arqma Network
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

function (write_static_version_header hash)
  set(VERSIONTAG "${hash}")
  configure_file("${CMAKE_SOURCE_DIR}/src/version.cpp.in" "${CMAKE_BINARY_DIR}/version.cpp")
endfunction ()

function (write_static_network_id_header seed dirty)
  set(ARQMA_NET_ID_COMMIT "${seed}")
  set(ARQMA_NET_ID_DIRTY ${dirty})
  set(ARQMA_NET_ID_DIRTY_HASH "")
  set(ARQMA_NET_ID_SEED "${seed}")
  # Mainnet stays on the historical fixed UUID (live P2P continuity).
  set(ARQMA_NET_ID_MAINNET_BYTES "0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1A")
  foreach(_net IN ITEMS testnet stagenet)
    string(SHA256 _hex "arqma-network-id|${_net}|${seed}")
    string(TOLOWER "${_hex}" _hex)
    set(_bytes "")
    set(_i 0)
    while(_i LESS 32)
      math(EXPR _next "${_i} + 2")
      string(SUBSTRING "${_hex}" ${_i} 2 _b)
      if(_bytes STREQUAL "")
        set(_bytes "0x${_b}")
      else()
        set(_bytes "${_bytes}, 0x${_b}")
      endif()
      set(_i ${_next})
    endwhile()
    string(TOUPPER "${_net}" _net_up)
    set(ARQMA_NET_ID_${_net_up}_BYTES "${_bytes}")
  endforeach()
  configure_file("${CMAKE_SOURCE_DIR}/src/network_id_generated.h.in"
                 "${CMAKE_BINARY_DIR}/network_id_generated.h" @ONLY)
endfunction ()
find_package(Git QUIET)
if ("$Format:$" STREQUAL "")
  # We're in a tarball; use hard-coded variables.
  write_static_version_header("release")
  write_static_network_id_header("release" 0)
elseif (GIT_FOUND OR Git_FOUND)
  message(STATUS "Found Git: ${GIT_EXECUTABLE}")
  add_custom_command(
    OUTPUT            "${CMAKE_BINARY_DIR}/version.cpp"
    COMMAND           "${CMAKE_COMMAND}"
                      "-D" "GIT=${GIT_EXECUTABLE}"
                      "-D" "TO=${CMAKE_BINARY_DIR}/version.cpp"
                      "-P" "cmake/GenVersion.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS           "${CMAKE_SOURCE_DIR}/src/version.cpp.in")

  # Always regenerate NETWORK_ID so dirty working trees cannot reuse a clean ID.
  add_custom_target(gennetworkid ALL
    COMMAND "${CMAKE_COMMAND}"
            "-D" "GIT=${GIT_EXECUTABLE}"
            "-D" "SRC_DIR=${CMAKE_SOURCE_DIR}"
            "-D" "TO=${CMAKE_BINARY_DIR}/network_id_generated.h"
            "-P" "${CMAKE_SOURCE_DIR}/cmake/GenNetworkId.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating NETWORK_ID header (fixed mainnet; commit-scoped test/stage)"
    SOURCES "${CMAKE_SOURCE_DIR}/src/network_id_generated.h.in"
            "${CMAKE_SOURCE_DIR}/cmake/GenNetworkId.cmake")
  # Configure-time seed so the first compilation finds the header.
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-D" "GIT=${GIT_EXECUTABLE}"
            "-D" "SRC_DIR=${CMAKE_SOURCE_DIR}"
            "-D" "TO=${CMAKE_BINARY_DIR}/network_id_generated.h"
            "-P" "${CMAKE_SOURCE_DIR}/cmake/GenNetworkId.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
else()
  message(STATUS "WARNING: Git was not found!")
  write_static_version_header("unknown")
  write_static_network_id_header("unknown" 0)
endif ()
add_custom_target(genversion ALL
  DEPENDS "${CMAKE_BINARY_DIR}/version.cpp")
if(TARGET gennetworkid)
  add_dependencies(genversion gennetworkid)
endif()