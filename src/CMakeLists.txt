cmake_minimum_required(VERSION 2.8)

file(GLOB HTTP_SRC "http/*.[ch]pp")
add_library(fibio_http  ${HTTP_SRC} "${CMAKE_SOURCE_DIR}/http-parser/http_parser.c")
target_link_libraries(fibio_http ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} fibio)