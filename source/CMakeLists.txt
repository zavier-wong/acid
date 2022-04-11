include_directories(${PROJECT_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic  -std=c++20  -Wall -Werror -Wno-deprecated -Wno-unused-function ")
set(LIB_SRC
        byte_array.cpp
        config.cpp
        fd_manager.cpp
        fiber.cpp
        file_monitor.cpp

        http/http.cpp
        http/http_connection.cpp
        http/http_server.cpp
        http/http_session.cpp
        http/parse.cpp
        http/servlet.cpp
        http/servlets/file_servlet.cpp

        hook.cpp
        io_manager.cpp
        lexical_cast.cpp
        log.cpp

        net/address.cpp
        net/socket.cpp
        net/socket_stream.cpp
        net/tcp_server.cpp
        net/uri.cpp

        rpc/rpc_client.cpp
        rpc/rpc_server.cpp
        rpc/rpc_service_register.cpp
        rpc/rpc_connection_pool.cpp
        rpc/rpc_session.cpp

        sync/mutex.cpp
        sync/co_condvar.cpp

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