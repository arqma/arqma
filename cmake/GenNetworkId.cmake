# Copyright (c) 2018 - 2026, The Arqma Network
#
# Build-time NETWORK_ID (dirty detection patterned on hyle-team/zano version.cmake):
#   - mainnet   -> ALWAYS the historical fixed UUID (live P2P continuity)
#   - testnet / stagenet:
#       clean tree -> ID derived only from `git rev-parse HEAD` (valid)
#       dirty tree -> commit + "-dirty-" + content hash (local edit isolates)
#
# Expected -D vars: GIT, TO (output header path), SRC_DIR (repo root)

if(NOT GIT)
  message(FATAL_ERROR "GenNetworkId.cmake: GIT is not set")
endif()
if(NOT TO)
  message(FATAL_ERROR "GenNetworkId.cmake: TO is not set")
endif()
if(NOT SRC_DIR)
  set(SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

function(arqma_hex_to_c_bytes hex_str out_var)
  string(TOLOWER "${hex_str}" _hex)
  string(REGEX REPLACE "[^0-9a-f]" "" _hex "${_hex}")
  string(LENGTH "${_hex}" _len)
  if(_len LESS 32)
    message(FATAL_ERROR "GenNetworkId.cmake: need >=16 bytes of hex, got '${hex_str}'")
  endif()
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
  set(${out_var} "${_bytes}" PARENT_SCOPE)
endfunction()

function(arqma_uuid_bytes_for_net seed net_name out_var)
  string(SHA256 _digest "arqma-network-id|${net_name}|${seed}")
  arqma_hex_to_c_bytes("${_digest}" _bytes)
  set(${out_var} "${_bytes}" PARENT_SCOPE)
endfunction()

set(ARQMA_NET_ID_COMMIT "unknown")
set(ARQMA_NET_ID_DIRTY 0)
set(ARQMA_NET_ID_DIRTY_HASH "")
set(ARQMA_NET_ID_SEED "unknown")

execute_process(
  COMMAND "${GIT}" rev-parse HEAD
  WORKING_DIRECTORY "${SRC_DIR}"
  RESULT_VARIABLE _commit_ret
  OUTPUT_VARIABLE _commit
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT _commit_ret EQUAL 0 OR _commit STREQUAL "")
  message(WARNING "GenNetworkId: cannot resolve git HEAD; using seed 'unknown'")
  set(ARQMA_NET_ID_SEED "unknown")
else()
  set(ARQMA_NET_ID_COMMIT "${_commit}")
  set(ARQMA_NET_ID_SEED "${_commit}")

  # Zano-style dirty detection (no tags required).
  execute_process(
    COMMAND "${GIT}" update-index -q --refresh
    WORKING_DIRECTORY "${SRC_DIR}"
    ERROR_QUIET)

  execute_process(
    COMMAND "${GIT}" diff-index --quiet HEAD --
    WORKING_DIRECTORY "${SRC_DIR}"
    RESULT_VARIABLE _dirty_ret
    ERROR_QUIET)

  set(_is_dirty FALSE)
  if(_dirty_ret EQUAL 1)
    set(_is_dirty TRUE)
  elseif(NOT _dirty_ret EQUAL 0)
    message(WARNING "GenNetworkId: cannot determine dirty state (diff-index=${_dirty_ret})")
  endif()

  # Untracked files also count as dirty (commit alone is not a valid ID then).
  execute_process(
    COMMAND "${GIT}" ls-files --others --exclude-standard
    WORKING_DIRECTORY "${SRC_DIR}"
    RESULT_VARIABLE _untracked_ret
    OUTPUT_VARIABLE _untracked
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_untracked_ret EQUAL 0 AND NOT _untracked STREQUAL "")
    set(_is_dirty TRUE)
  endif()

  if(_is_dirty)
    set(ARQMA_NET_ID_DIRTY 1)
    execute_process(
      COMMAND "${GIT}" diff HEAD
      WORKING_DIRECTORY "${SRC_DIR}"
      RESULT_VARIABLE _diff_ret
      OUTPUT_VARIABLE _diff_out
      ERROR_QUIET)
    if(NOT _diff_ret EQUAL 0)
      set(_diff_out "")
    endif()
    string(SHA256 ARQMA_NET_ID_DIRTY_HASH "${_diff_out}\n${_untracked}")
    set(ARQMA_NET_ID_SEED "${_commit}-dirty-${ARQMA_NET_ID_DIRTY_HASH}")
  endif()
endif()

# Historical mainnet UUID from master — never tip/tarball scoped.
set(ARQMA_NET_ID_MAINNET_BYTES "0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x1A")
arqma_uuid_bytes_for_net("${ARQMA_NET_ID_SEED}" "testnet" ARQMA_NET_ID_TESTNET_BYTES)
arqma_uuid_bytes_for_net("${ARQMA_NET_ID_SEED}" "stagenet" ARQMA_NET_ID_STAGENET_BYTES)

set(_template "${SRC_DIR}/src/network_id_generated.h.in")
if(NOT EXISTS "${_template}")
  message(FATAL_ERROR "GenNetworkId.cmake: missing template ${_template}")
endif()

configure_file("${_template}" "${TO}" @ONLY)

message(STATUS "NETWORK_ID seed (testnet/stagenet): ${ARQMA_NET_ID_SEED}")
message(STATUS "NETWORK_ID mainnet: fixed historical UUID")
if(ARQMA_NET_ID_DIRTY)
  message(STATUS "NETWORK_ID dirty: yes (isolates testnet/stagenet builds only)")
else()
  message(STATUS "NETWORK_ID dirty: no (commit-scoped testnet/stagenet ID)")
endif()
