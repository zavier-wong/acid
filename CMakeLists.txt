
cmake_minimum_required(VERSION 3.16)
project(acid)

set (CMAKE_CXX_COMPILER "/usr/bin/g++-11")
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -std=c++20  -Wall -Werror -Wno-deprecated -Wno-unused-function ")

include(cmake/utils.cmake)
add_definitions(-Wno-builtin-macro-redefined)

#编译
add_subdirectory(source)
add_subdirectory(tests)
add_subdirectory(example)
