set(arq_blink_sources
  blink.cpp)

set(arq_blink_headers
  blink.h)

arqma_find_all_headers(arq_blink_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")

arqma_private_headers(arq_blink
  ${arq_blink_private_headers})
arqma_add_library(arq_blink
  ${arq_blink_sources}
  ${arq_blink_headers}
  ${arq_blink_private_headers})

target_include_directories(arq_blink
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(arq_blink
  PUBLIC
    cncrypto
  PRIVATE
    ${EXTRA_LIBRARIES})
