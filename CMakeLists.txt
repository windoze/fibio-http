cmake_minimum_required(VERSION 2.8)
project(fibio_http)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ftemplate-depth=256")
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-deprecated-register")
endif(APPLE)

find_package(ZLIB REQUIRED)
find_package(OpenSSL)
find_package(fibio REQUIRED)
find_package(Boost 1.56.0 COMPONENTS iostreams REQUIRED)

INCLUDE_DIRECTORIES(
    ${FIBIO_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/http-parser
    ${CMAKE_SOURCE_DIR}/include
)

set(COMMON_LIBS)
list(APPEND COMMON_LIBS fibio_http ${Boost_LIBRARIES} ${FIBIO_LIBS} ${OPENSSL_LIBRARIES})

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)

enable_testing()
