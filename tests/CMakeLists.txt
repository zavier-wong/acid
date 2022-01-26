include_directories(${PROJECT_SOURCE_DIR})
link_directories(${PROJECT_SOURCE_DIR}/lib)

set(LIBS acid)

#test_log
acid_add_executable(test_log test_log.cpp acid ${LIBS})

#test_config
acid_add_executable(test_config  test_config.cpp acid ${LIBS})

#test_log_yaml
acid_add_executable(test_log_yaml  test_log_yaml.cpp acid ${LIBS})

#test_thread
acid_add_executable(test_thread  test_thread.cpp acid ${LIBS})

#test_util
acid_add_executable(test_util  test_util.cpp  acid ${LIBS})

#test_fiber
acid_add_executable(test_fiber  test_fiber.cpp  acid ${LIBS})

#test_scheduler
acid_add_executable(test_scheduler test_scheduler.cpp  acid ${LIBS})

#test_iomanager
acid_add_executable(test_iomanager test_iomanager.cpp  acid ${LIBS})

#test_timer
acid_add_executable(test_timer test_timer.cpp  acid ${LIBS})

#test_hook
acid_add_executable(test_hook test_hook.cpp  acid ${LIBS})

#test_socket
acid_add_executable(test_socket test_socket.cpp  acid ${LIBS})

#test_address
acid_add_executable(test_address test_address.cpp acid ${LIBS})

#test_bytearray
acid_add_executable(test_bytearray test_bytearray.cpp acid ${LIBS})

#test_http_parse
acid_add_executable(test_http_parse test_http_parse.cpp acid ${LIBS})

#test_http
acid_add_executable(test_http test_http.cpp acid ${LIBS})

#test_tcp_server
acid_add_executable(test_tcp_server test_tcp_server.cpp acid ${LIBS})

#test_http_server
acid_add_executable(test_http_server test_http_server.cpp acid ${LIBS})

#test_http_connection
acid_add_executable(test_http_connection test_http_connection.cpp acid ${LIBS})

#test_uri
acid_add_executable(test_uri test_uri.cpp acid ${LIBS})

#test_test_serializer
acid_add_executable(test_serializer rpc/test_serializer.cpp acid ${LIBS})

#test_rpc_server
acid_add_executable(test_rpc_server rpc/test_rpc_server.cpp acid ${LIBS})

#test_rpc_client
acid_add_executable(test_rpc_client rpc/test_rpc_client.cpp acid ${LIBS})

#test_route_strategy
acid_add_executable(test_route_strategy rpc/test_route_strategy.cpp acid ${LIBS})

#test_rpc_registry
acid_add_executable(test_rpc_registry rpc/test_rpc_registry.cpp acid ${LIBS})

#test_rpc_connection_poll
acid_add_executable(test_rpc_connection_poll rpc/test_rpc_connection_poll.cpp acid ${LIBS})

#set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)