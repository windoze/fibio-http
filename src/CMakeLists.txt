cmake_minimum_required(VERSION 2.8)

file(GLOB_RECURSE HTTP_HDR "../include/fibio/*.hpp")
file(GLOB HTTP_SRC "http/*.[ch]pp")
add_library(fibio_http ${HTTP_HDR} ${HTTP_SRC} "${CMAKE_SOURCE_DIR}/http-parser/http_parser.c")
target_link_libraries(fibio_http ${FIBIO_LIBRARIES})
