set(arq_router_sources
  router_http.cpp
  router_server.cpp
  router_service.cpp)

set(arq_router_headers
  router_config.h
  router_http.h
  router_server.h
  router_service.h)

arqma_find_all_headers(arq_router_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arq_router
  ${arq_router_private_headers})
arqma_add_library(arq_router
  ${arq_router_sources}
  ${arq_router_headers}
  ${arq_router_private_headers})

target_include_directories(arq_router
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(arq_router
  PUBLIC
    arq_messaging
    arq_storage
    ${Boost_SYSTEM_LIBRARY}
  PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})

if(NOT IOS)
  arqma_add_executable(arqma_router main.cpp)
  target_link_libraries(arqma_router
    PRIVATE
      arq_router
      arq_messaging
      arq_storage
      epee
      ${Boost_PROGRAM_OPTIONS_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
      ${CMAKE_THREAD_LIBS_INIT}
      ${EXTRA_LIBRARIES})
  set_property(TARGET arqma_router PROPERTY OUTPUT_NAME "arqma-router")
  install(TARGETS arqma_router DESTINATION bin)
endif()
