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

add_executable(test_subtitle_insertion
  fixture_rpc_server.cpp
  test_subtitle_insertion.cpp
)
target_link_libraries(test_subtitle_insertion PRIVATE edward_resolve edward_core Qt6::Core Qt6::Network)
add_test(NAME resolve.subtitle_insertion COMMAND test_subtitle_insertion)
