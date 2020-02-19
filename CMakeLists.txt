cmake_minimum_required(VERSION 3.0)
project(fylee)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
# include (cmake/utils.cmake)

SET(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
SET(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(/usr/local/include)
INCLUDE_DIRECTORIES(/usr/include)

LINK_DIRECTORIES(/usr/local/lib)
LINK_DIRECTORIES(/usr/lib)
LINK_DIRECTORIES(${PROJECT_SOURCE_DIR}/lib)

option(BUILD_TEST "ON for complile test" OFF)

find_package(Boost REQUIRED)
if(Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
endif()

find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIR})
endif()

SET(LIB_SRC
    fylee/address.cc
    fylee/buffer.cc
    fylee/config.cc
    fylee/env.cc
    fylee/fdmanager.cc
    fylee/log.cc
    fylee/mutex.cc
    fylee/socket.cc
    fylee/timer.cc
    fylee/thread.cc
    fylee/util.cc
    fylee/stream.cc
    fylee/streams/socket_stream.cc
    fylee/http/http.cc
    fylee/http/http_parser.cc
    fylee/http/http11_parser.cc
    fylee/http/httpclient_parser.cc
    fylee/http/http_session.cc
    fylee/http/http_server.cc
    fylee/http/servlet.cc
    fylee/uri.cc
    fylee/eventloop.cc
    fylee/eventloopthread.cc
    fylee/eventloopthreadpool.cc
    fylee/channel.cc
    fylee/poller.cc
    fylee/timerqueue.cc
    fylee/tcp_server.cc
    fylee/connection.cc
)

add_library(fylee SHARED ${LIB_SRC})
# force_redefine_file_macro_for_sources(fylee)

find_library(PTHREAD pthread)

SET(LINKS
        fylee
        yaml-cpp
        jsoncpp
        pthread
        dl
        )

# if(BUILD_TEST)
# endif()

# add_executable(test_hook "tests/test_hook.cc")
# target_link_libraries(test_hook ${LINKS})

# add_executable(test_iomanager "tests/test_iomanager.cc")
# target_link_libraries(test_iomanager ${LINKS})

# add_executable(test_address "tests/test_address.cc")
# target_link_libraries(test_address ${LINKS})

# add_executable(test_socket "tests/test_socket.cc")
# target_link_libraries(test_socket ${LINKS})

# add_executable(test_buffer "tests/test_buffer.cc")
# target_link_libraries(test_buffer ${LINKS})

# add_executable(test_http "tests/test_http.cc")
# target_link_libraries(test_http ${LINKS})

# add_executable(test_http_parser "tests/test_http_parser.cc")
# target_link_libraries(test_http_parser ${LINKS})

# add_executable(test_tcp_server "tests/test_tcp_server.cc")
# target_link_libraries(test_tcp_server ${LINKS})

# add_executable(test_uri "tests/test_uri.cc")
# target_link_libraries(test_uri ${LINKS})

# add_executable(test_http_connection "tests/test_http_connection.cc")
# target_link_libraries(test_http_connection ${LINKS})


add_executable(echo_server "examples/echo_server.cc")
target_link_libraries(echo_server ${LINKS})

add_executable(test_http_server "examples/stress_test_server.cc")
target_link_libraries(test_http_server ${LINKS})

