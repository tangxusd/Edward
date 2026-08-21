add_executable(test_resolve_connection
  fixture_rpc_server.cpp
  test_resolve_connection.cpp
)
target_link_libraries(test_resolve_connection PRIVATE edward_resolve Qt6::Core Qt6::Network)
add_test(NAME resolve.connection COMMAND test_resolve_connection)

add_executable(test_resolve_adapter
  fixture_rpc_server.cpp
  test_resolve_adapter.cpp
)
target_link_libraries(test_resolve_adapter PRIVATE edward_resolve Qt6::Core Qt6::Network)
add_test(NAME resolve.adapter COMMAND test_resolve_adapter)
