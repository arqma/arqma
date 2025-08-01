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

add_library(arqma_bc_utils_std_libs INTERFACE)
target_link_libraries(arqma_bc_utils_std_libs INTERFACE
  cryptonote_core
  lmdb
  blockchain_db
  version
  epee
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_ASIO_LIBRARY}
  ${EXTRA_LIBRARIES})


set(blockchain_import_sources
  blockchain_import.cpp
  bootstrap_file.cpp
  blocksdat_file.cpp)

set(blockchain_import_private_headers
  bootstrap_file.h
  blocksdat_file.h
  bootstrap_serialization.h)

arqma_private_headers(blockchain_import
  ${blockchain_import_private_headers})

set(blockchain_export_sources
  blockchain_export.cpp
  bootstrap_file.cpp
  blocksdat_file.cpp)

set(blockchain_export_private_headers
  bootstrap_file.h
  blocksdat_file.h
  bootstrap_serialization.h)

arqma_private_headers(blockchain_export
  ${blockchain_export_private_headers})


set(blockchain_blackball_sources
  blockchain_blackball.cpp)

set(blockchain_blackball_private_headers
  bootstrap_file.h
  blocksdat_file.h
  bootstrap_serialization.h)

arqma_private_headers(blockchain_blackball
  ${blockchain_blackball_private_headers})

set(blockchain_usage_sources
  blockchain_usage.cpp)

set(blockchain_usage_private_headers)

arqma_private_headers(blockchain_usage
  ${blockchain_usage_private_headers})


#set(blockchain_prune_known_spent_data_sources
#  blockchain_prune_known_spent_data.cpp)

#set(blockchain_prune_known_spent_data_private_headers)

#arqma_private_headers(blockchain_prune_known_spent_data
#  ${blockchain_prune_known_spent_data_private_headers})

#set(blockchain_prune_sources
#  blockchain_prune.cpp)

#set(blockchain_prune_private_headers)

#arqma_private_headers(blockchain_prune
#  ${blockchain_prune_private_headers})

set(blockchain_ancestry_sources
  blockchain_ancestry.cpp)

set(blockchain_ancestry_private_headers)

arqma_private_headers(blockchain_ancestry
${blockchain_ancestry_private_headers})

set(blockchain_depth_sources
  blockchain_depth.cpp)

set(blockchain_depth_private_headers)

arqma_private_headers(blockchain_depth
  ${blockchain_depth_private_headers})

set(blockchain_stats_sources
  blockchain_stats.cpp)

set(blockchain_stats_private_headers)

arqma_private_headers(blockchain_stats
  ${blockchain_stats_private_headers})

arqma_add_executable(blockchain_import
  ${blockchain_import_sources}
  ${blockchain_import_private_headers})

target_link_libraries(blockchain_import
  PRIVATE
    arqma_bc_utils_std_libs
    cryptonote_protocol
    p2p
    arqnet
    blocks
    ${UPNP_LIBRARIES})

if(ARCH_WIDTH)
  target_compile_definitions(blockchain_import
    PUBLIC -DARCH_WIDTH=${ARCH_WIDTH})
endif()

set_property(TARGET blockchain_import PROPERTY OUTPUT_NAME "arqma-blockchain-import")
install(TARGETS blockchain_import DESTINATION bin)

arqma_add_executable(blockchain_export
  ${blockchain_export_sources}
  ${blockchain_export_private_headers})

target_link_libraries(blockchain_export
  PRIVATE
    arqma_bc_utils_std_libs)

set_property(TARGET blockchain_export PROPERTY OUTPUT_NAME "arqma-blockchain-export")
install(TARGETS blockchain_export DESTINATION bin)

arqma_add_executable(blockchain_blackball
  ${blockchain_blackball_sources}
  ${blockchain_blackball_private_headers})

target_link_libraries(blockchain_blackball
  PRIVATE
    wallet
    arqma_bc_utils_std_libs)

set_property(TARGET blockchain_blackball PROPERTY OUTPUT_NAME "arqma-blockchain-mark-spent-outputs")
install(TARGETS blockchain_blackball DESTINATION bin)

arqma_add_executable(blockchain_usage
  ${blockchain_usage_sources}
  ${blockchain_usage_private_headers})

target_link_libraries(blockchain_usage
  PRIVATE
    arqma_bc_utils_std_libs)

set_property(TARGET blockchain_usage PROPERTY OUTPUT_NAME "arqma-blockchain-usage")
install(TARGETS blockchain_usage DESTINATION bin)

arqma_add_executable(blockchain_ancestry
  ${blockchain_ancestry_sources}
  ${blockchain_ancestry_private_headers})

target_link_libraries(blockchain_ancestry
  PRIVATE
    arqma_bc_utils_std_libs)

set_property(TARGET blockchain_ancestry PROPERTY OUTPUT_NAME "arqma-blockchain-ancestry")
install(TARGETS blockchain_ancestry DESTINATION bin)

arqma_add_executable(blockchain_depth
  ${blockchain_depth_sources}
  ${blockchain_depth_private_headers})

target_link_libraries(blockchain_depth
  PRIVATE
    arqma_bc_utils_std_libs)

set_property(TARGET blockchain_depth PROPERTY OUTPUT_NAME "arqma-blockchain-depth")
install(TARGETS blockchain_depth DESTINATION bin)

#arqma_add_executable(blockchain_prune_known_spent_data
#  ${blockchain_prune_known_spent_data_sources}
#  ${blockchain_prune_known_spent_data_private_headers})

#target_link_libraries(blockchain_prune_known_spent_data
#  PRIVATE
#    p2p
#    arqma_bc_utils_std_libs)

#set_property(TARGET blockchain_prune_known_spent_data PROPERTY OUTPUT_NAME "arqma-blockchain-prune-known-spent-data")
#install(TARGETS blockchain_prune_known_spent_data DESTINATION bin)

#arqma_add_executable(blockchain_prune
#  ${blockchain_prune_sources}
#  ${blockchain_prune_private_headers})

#set_property(TARGET blockchain_prune PROPERTY OUTPUT_NAME "arqma-blockchain-prune")
#install(TARGETS blockchain_prune DESTINATION bin)

#target_link_libraries(blockchain_prune
#  PRIVATE
#    p2p
#    arqma_bc_utils_std_libs)

arqma_add_executable(blockchain_stats
  ${blockchain_stats_sources}
  ${blockchain_stats_private_headers})

set_property(TARGET blockchain_stats PROPERTY OUTPUT_NAME "arqma-blockchain-stats")
install(TARGETS blockchain_stats DESTINATION bin)

target_link_libraries(blockchain_stats
  PRIVATE
    arqma_bc_utils_std_libs)