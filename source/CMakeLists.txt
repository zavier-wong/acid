
include_directories(${PROJECT_SOURCE_DIR})
include_directories(/usr/local/include/yaml-cpp/)
include_directories(/mnt/f/SourceCode/boost_1_73_0/)
link_directories(/mnt/f/SourceCode/boost_1_73_0/libs)
link_directories(/usr/local/bin)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -std=c++20  -Wall -Werror -Wno-deprecated -Wno-unused-function ")
set(LIB_SRC
        byte_array.cpp
        config.cpp
        fd_manager.cpp
        fiber.cpp

        http/http.cpp
        http/http_connection.cpp
        http/http_server.cpp
        http/http_session.cpp
        http/parse.cpp
        http/servlet.cpp
        http/servlets/file_servlet.cpp

        hook.cpp
        io_manager.cpp
        log.cpp
        mutex.cpp

        net/address.cpp
        net/socket.cpp
        net/socket_stream.cpp
        net/tcp_server.cpp
        net/uri.cpp

        rpc/rpc_server.cpp
        rpc/rpc_service_register.cpp

        scheduler.cpp
        stream.cpp
        timer.cpp
        thread.cpp
        util.cpp
        )
set(LIB_LIB pthread yaml-cpp dl)

add_library(acid STATIC ${LIB_SRC})
target_link_libraries(acid ${LIB_LIB})
redefine_file_macro(acid)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)