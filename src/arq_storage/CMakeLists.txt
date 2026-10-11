set(arq_storage_sources
  http_io.cpp
  storage_client.cpp
  storage_endpoint.cpp
  storage_server.cpp)

set(arq_storage_headers
  http_io.h
  storage_client.h
  storage_endpoint.h
  storage_server.h)

arqma_find_all_headers(arq_storage_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arq_storage
  ${arq_storage_private_headers})
arqma_add_library(arq_storage
  ${arq_storage_sources}
  ${arq_storage_headers}
  ${arq_storage_private_headers})

target_include_directories(arq_storage
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(arq_storage
  PUBLIC
    ${Boost_SYSTEM_LIBRARY}
  PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})

if(NOT IOS)
  arqma_add_executable(arqma_storage main.cpp)
  target_link_libraries(arqma_storage
    PRIVATE
      arq_storage
      epee
      ${Boost_PROGRAM_OPTIONS_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
      ${CMAKE_THREAD_LIBS_INIT}
      ${EXTRA_LIBRARIES})
  set_property(TARGET arqma_storage PROPERTY OUTPUT_NAME "arqma-storage")
  install(TARGETS arqma_storage DESTINATION bin)
endif()
