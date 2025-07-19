set(arqnet_sources
  sn_network.cpp
  bt_serialize.cpp)

set(arqnet_headers
  )

arqma_find_all_headers(arqnet_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arqnet
  ${arqnet_private_headers})
arqma_add_library(arqnet
  ${arqnet_sources}
  ${arqnet_headers}
  ${arqnet_private_headers})

target_link_libraries(arqnet
  PUBLIC
    libzmq)