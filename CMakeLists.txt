cmake_minimum_required(VERSION 3.15)
project(acid)
#add_subdirectory(example)
#add_subdirectory(tests)
if (CMAKE_BUILD_TYPE)
else()
    #set(CMAKE_BUILD_TYPE RELEASE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()
message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_COMMAND: ${CMAKE_COMMAND}")

option(ENABLE_DEBUGGER "enable debugger" ON)
if (ENABLE_DEBUGGER)
    set(ENABLE_DEBUGGER 1)
    add_definitions(-DACID_ENABLE_DEBUGGER=1)
    message ("  enable_debugger: yes")
else()
    set(ENABLE_DEBUGGER 0)
    add_definitions(-DACID_ENABLE_DEBUGGER=0)
    message ("  enable_debugger: no")
endif()

option(BUILD_DYNAMIC "build dynamic" OFF)
if (BUILD_DYNAMIC)
    message ("  build dynamic lib: yes")
else()
    message ("  build dynamic lib: no")
endif()

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("----------------------------------")

set(CMAKE_CXX_COMPILER "/usr/bin/g++-11")
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "-std=c++20 -D__const__=  -fPIC -fno-strict-aliasing -Wall ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG")

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

###################################################################################
# third party
# json
include_directories("${PROJECT_SOURCE_DIR}/third_party/json/single_include")
# libgo
include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo")
include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo")
include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo/linux")
link_directories(${PROJECT_SOURCE_DIR}/third_party/libgo/build)
# spdlog
include_directories("${PROJECT_SOURCE_DIR}/third_party/spdlog/include")
link_directories(${PROJECT_SOURCE_DIR}/third_party/spdlog/build)
# yaml-cpp
include_directories("${PROJECT_SOURCE_DIR}/third_party/yaml-cpp/include")
link_directories(${PROJECT_SOURCE_DIR}/third_party/yaml-cpp/build)
###################################################################################

include_directories("${PROJECT_SOURCE_DIR}")
#记得将目录加进来
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/common SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/http SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/http/servlets SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/net SRC_LIST)
#aux_source_directory(${PROJECT_SOURCE_DIR}/acid/raft SRC_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/acid/rpc SRC_LIST)

# 编译的库在 acid/lib 下生成
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
message("------------ LIBRARY_OUTPUT_PATH -------------")
message(${PROJECT_SOURCE_DIR}/lib)

set(LINK_ARGS acid libgo pthread dl yaml-cpp)
set(TARGET "acid")
set(STATIC_T "acid_static")
set(SHARED_T "acid_dynamic")

# 默认编译静态库
add_library("${STATIC_T}" STATIC ${SRC_LIST})
set_target_properties("${STATIC_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
target_link_libraries("${STATIC_T}" -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)

# 如果想编为译动态库记得把 libgo 也编译成动态库
if (BUILD_DYNAMIC)
    add_library("${SHARED_T}" SHARED ${SRC_LIST})
    set_target_properties("${SHARED_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
    target_link_libraries("${SHARED_T}" -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)
    install(TARGETS ${SHARED_T} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
endif()

aux_source_directory(${PROJECT_SOURCE_DIR}/acid/raft RAFT_LIST)

set(RAFT_TARGET "raft")
set(RAFT_STATIC_T "raft_static")

# 默认编译静态库
add_library("${RAFT_STATIC_T}" STATIC ${RAFT_LIST})
set_target_properties("${RAFT_STATIC_T}" PROPERTIES OUTPUT_NAME "${RAFT_TARGET}")
target_link_libraries("${RAFT_STATIC_T}" -Wl,--start-group ${LINK_ARGS} -Wl,--end-group)
