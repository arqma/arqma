set(arqmq_sources
  arqmq.cpp
  backend_policy.cpp
  curve_zap.cpp
  mesh_bridge.cpp
  peer_table.cpp
  socket_stack.cpp)

set(arqmq_headers
  arqmq.h
  backend_policy.hpp
  curve_zap.hpp
  transport.hpp
  socket_stack.hpp
  peer_table.hpp
  mesh_bridge.hpp
  command_registry.hpp
  message_limits.hpp)

arqma_find_all_headers(arqmq_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arqmq
  ${arqmq_private_headers})
arqma_add_library(arqmq
  ${arqmq_sources}
  ${arqmq_headers}
  ${arqmq_private_headers})

target_include_directories(arqmq
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(arqmq
  PUBLIC
    libzmq
  PRIVATE
    ${EXTRA_LIBRARIES})
