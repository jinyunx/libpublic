cmake_minimum_required(VERSION 2.6)

project(HttpServerTest)

set(CMAKE_C_FLAGS
    "-Wall -Wextra -Wno-unused-parameter")

set(CMAKE_C_FLAGS_DEBUG
    "-g")

set(CMAKE_C_FLAGS_RELEASE
    "-O3 -g -DRELEASE -DNDEBUG")

set(CMAKE_CXX_FLAGS
    "-Wall -Wextra -Wno-unused-parameter")

set(CMAKE_CXX_FLAGS_DEBUG
    "-g")

set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -g -DRELEASE -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,-rpath=.:..:lib")


set(CMAKE_USE_RELATIVE_PATHS TRUE)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")


include_directories("/root/onexie/cppDev/boost/include")

add_library(http
    HttpDispatch.cpp
    HttpParser.cpp
    HttpServer.cpp
    ../3rd/http-parser/http_parser.c
    )

add_executable(http_server_test
    HttpServerTest.cpp
    )

target_link_libraries(http_server_test
    http
    )
