include_directories(${PROJECT_SOURCE_DIR})
link_directories(${PROJECT_SOURCE_DIR}/lib)

set(LIBS acid)

#set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/test)

set(TESTS_FILES "")
aux_source_directory("${CMAKE_SOURCE_DIR}/tests" TESTS_FILES)

foreach(TESTS_FILE_PATH ${TESTS_FILES})
    string(REGEX REPLACE ".+[/\]([^/\.]+)\\.cpp" "\\1" TESTS_FILE_NAME ${TESTS_FILE_PATH})
    message(STATUS "Generating test target: ${TESTS_FILE_NAME}")
    acid_add_executable(${TESTS_FILE_NAME} ${TESTS_FILE_NAME}.cpp acid ${LIBS})
endforeach(TESTS_FILE_PATH)

set(TESTS_RPC_FILES "")
aux_source_directory("${CMAKE_SOURCE_DIR}/tests/rpc" TESTS_RPC_FILES)

foreach(TESTS_RPC_FILE_PATH ${TESTS_RPC_FILES})
    string(REGEX REPLACE ".+[/\]([^/\.]+)\\.cpp" "\\1" TESTS_RPC_FILE_NAME ${TESTS_RPC_FILE_PATH})
    message(STATUS "Generating test target: ${TESTS_RPC_FILE_NAME} ")
    acid_add_executable(${TESTS_RPC_FILE_NAME} rpc/${TESTS_RPC_FILE_NAME}.cpp acid ${LIBS})
endforeach(TESTS_RPC_FILE_PATH)