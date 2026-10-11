set(arq_messaging_sources
  identity.cpp
  sealed_box.cpp
  onion_layer.cpp)

set(arq_messaging_headers
  contacts.hpp
  identity.hpp
  message_envelope.hpp
  onion_forward.hpp
  onion_layer.hpp
  onion_request.hpp
  sealed_sender.hpp
  stack_env.hpp
  swarm_map.hpp)

arqma_find_all_headers(arq_messaging_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arq_messaging
  ${arq_messaging_private_headers})
arqma_add_library(arq_messaging
  ${arq_messaging_sources}
  ${arq_messaging_headers}
  ${arq_messaging_private_headers})

target_include_directories(arq_messaging
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(arq_messaging
  PUBLIC
    sodium
  PRIVATE
    ${EXTRA_LIBRARIES})

if(NOT IOS)
  arqma_add_executable(arqma_msg main.cpp)
  target_link_libraries(arqma_msg
    PRIVATE
      arq_messaging
      arq_storage
      epee
      ${Boost_PROGRAM_OPTIONS_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
      ${CMAKE_THREAD_LIBS_INIT}
      ${EXTRA_LIBRARIES})
  set_property(TARGET arqma_msg PROPERTY OUTPUT_NAME "arqma-msg")
  install(TARGETS arqma_msg DESTINATION bin)
endif()
