
cmake_minimum_required(VERSION 3.16)
project(acid)

include(cmake/utils.cmake)
add_definitions(-Wno-builtin-macro-redefined)
#redefine_file_macro(acid)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -std=c++20  -Wall -Werror -Wno-deprecated -Wno-unused-function ")
set(BOOST_ROOT "/mnt/f/SourceCode/boost_1_73_0")

#编译
add_subdirectory(source)
add_subdirectory(tests)
add_subdirectory(example)
#include_directories(include)
#link_directories(lib)
#include_directories(/mnt/f/SourceCode/boost_1_73_0/)
#link_directories(/mnt/f/SourceCode/boost_1_73_0/libs)
