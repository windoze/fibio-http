cmake_minimum_required(VERSION 2.8)
project(fibio_http)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ftemplate-depth=256")
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-deprecated-register")
    set(CMAKE_LINK_FLAGS "${CMAKE_LINK_FLAGS} -stdlib=libc++")
endif(APPLE)

find_package(Threads REQUIRED)
find_package(Boost 1.55.0 COMPONENTS system coroutine context iostreams REQUIRED)
find_package(ZLIB REQUIRED)

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/http-parser
    ${CMAKE_SOURCE_DIR}/fibio/include
    ${CMAKE_SOURCE_DIR}/include
)

set(FIBIO_LIBS)
list(APPEND FIBIO_LIBS fibio_http fibio ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

ADD_SUBDIRECTORY(fibio)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)