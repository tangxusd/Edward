add_executable(test_timeline_controller test_timeline_controller.cpp)
target_link_libraries(test_timeline_controller PRIVATE edward_desktop)
add_test(NAME desktop.timeline_controller COMMAND test_timeline_controller ${CMAKE_BINARY_DIR}/tests/media/fixture.mp4)

add_executable(test_visual_routes test_visual_routes.cpp)
target_compile_definitions(test_visual_routes PRIVATE EDWARD_DESKTOP_QML_DIR="${CMAKE_SOURCE_DIR}/src/desktop/qml")
target_compile_definitions(test_visual_routes PRIVATE EDWARD_DESKTOP_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/desktop/src")
add_test(NAME desktop.visual_routes COMMAND test_visual_routes)

add_executable(test_workbench_plugins test_workbench_plugins.cpp)
target_link_libraries(test_workbench_plugins PRIVATE edward_desktop edward_resources Qt6::Core)
add_dependencies(test_workbench_plugins test_rpc_plugin media_fixture media_audio_fixture)
add_test(NAME desktop.workbench_plugins COMMAND test_workbench_plugins
  $<TARGET_FILE:test_rpc_plugin> ${CMAKE_BINARY_DIR}/tests/media/fixture.mp4
  ${CMAKE_BINARY_DIR}/tests/media/audio-fixture.mp4)

add_executable(test_resolve_workbench
  test_resolve_workbench.cpp
  ../resolve/fixture_rpc_server.cpp
)
target_link_libraries(test_resolve_workbench PRIVATE edward_desktop Qt6::Network)
add_test(NAME desktop.resolve_workbench COMMAND test_resolve_workbench)

add_executable(test_resolve_keyframe_edit test_resolve_keyframe_edit.cpp)
target_link_libraries(test_resolve_keyframe_edit PRIVATE edward_desktop)
add_test(NAME desktop.resolve_keyframe_edit COMMAND test_resolve_keyframe_edit)

add_executable(test_export_channels test_export_channels.cpp)
target_link_libraries(test_export_channels PRIVATE edward_desktop)
add_test(NAME desktop.export_channels COMMAND test_export_channels)

add_executable(test_diagnostics_reporter test_diagnostics_reporter.cpp)
target_link_libraries(test_diagnostics_reporter PRIVATE edward_desktop)
add_test(NAME desktop.diagnostics_reporter COMMAND test_diagnostics_reporter)

add_executable(test_preference_store test_preference_store.cpp)
target_link_libraries(test_preference_store PRIVATE edward_desktop Qt6::Sql)
add_test(NAME desktop.preference_store COMMAND test_preference_store)
add_executable(test_style_snapshot test_style_snapshot.cpp)
target_link_libraries(test_style_snapshot PRIVATE edward_desktop Qt6::Core Qt6::Sql)
add_test(NAME desktop.style_snapshot COMMAND test_style_snapshot)

add_executable(test_preference_bridge test_preference_bridge.cpp)
target_link_libraries(test_preference_bridge PRIVATE edward_desktop Qt6::WebChannel)
add_test(NAME desktop.preference_bridge COMMAND test_preference_bridge)

add_executable(test_native_runtime_workbench test_native_runtime_workbench.cpp)
target_link_libraries(test_native_runtime_workbench PRIVATE edward_desktop Qt6::Core)
add_test(NAME desktop.native_runtime_workbench COMMAND test_native_runtime_workbench)
