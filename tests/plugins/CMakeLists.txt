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

add_executable(test_plugin_adapters test_plugin_adapters.cpp)
target_link_libraries(test_plugin_adapters PRIVATE edward_plugins Qt6::Core)
target_compile_definitions(test_plugin_adapters PRIVATE EDWARD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME plugins.plugin_adapters COMMAND test_plugin_adapters)

add_executable(test_plugin_runtime_resolver test_plugin_runtime_resolver.cpp)
target_link_libraries(test_plugin_runtime_resolver PRIVATE edward_plugins Qt6::Core)
add_test(NAME plugins.plugin_runtime_resolver COMMAND test_plugin_runtime_resolver)

add_executable(test_plugin_trust_store test_plugin_trust_store.cpp)
target_link_libraries(test_plugin_trust_store PRIVATE edward_plugins Qt6::Core)
add_test(NAME plugins.plugin_trust_store COMMAND test_plugin_trust_store)
