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
