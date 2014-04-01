cmake_minimum_required(VERSION 2.8)

add_executable(test_http_client test_http_client.cpp)
TARGET_LINK_LIBRARIES(test_http_client ${FIBIO_LIBS} ${ZLIB_LIBRARIES} fibio_http)

add_executable(test_http_server test_http_server.cpp)
TARGET_LINK_LIBRARIES(test_http_server ${FIBIO_LIBS} ${ZLIB_LIBRARIES} fibio_http)
