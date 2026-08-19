add_executable(test_plugin_manifest test_plugin_manifest.cpp)
target_link_libraries(test_plugin_manifest PRIVATE edward_plugins Qt6::Core)
add_test(NAME plugins.plugin_manifest COMMAND test_plugin_manifest)
add_executable(test_plugin_host test_plugin_host.cpp)
target_link_libraries(test_plugin_host PRIVATE edward_plugins Qt6::Core)
add_test(NAME plugins.plugin_host COMMAND test_plugin_host)

add_executable(test_rpc_plugin rpc_fixture.cpp)
target_link_libraries(test_rpc_plugin PRIVATE Qt6::Core Qt6::Gui)

add_executable(test_plugin_process test_plugin_process.cpp)
target_link_libraries(test_plugin_process PRIVATE edward_plugins Qt6::Core Qt6::Gui)
add_dependencies(test_plugin_process test_rpc_plugin)
add_test(NAME plugins.plugin_process COMMAND test_plugin_process $<TARGET_FILE:test_rpc_plugin>)
