cmake_minimum_required(VERSION 2.8)

enable_testing()

add_executable(test_http_client test_http_client.cpp)
TARGET_LINK_LIBRARIES(test_http_client fibio_http ${COMMON_LIBS} ${ZLIB_LIBRARIES})

add_executable(test_http_server test_http_server.cpp)
TARGET_LINK_LIBRARIES(test_http_server fibio_http ${COMMON_LIBS} ${ZLIB_LIBRARIES})

add_test(http_client test_http_client)
add_test(http_server test_http_server)
