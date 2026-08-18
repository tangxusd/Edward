add_executable(test_timeline_controller test_timeline_controller.cpp)
target_link_libraries(test_timeline_controller PRIVATE edward_desktop)
add_test(NAME desktop.timeline_controller COMMAND test_timeline_controller ${CMAKE_BINARY_DIR}/tests/media/fixture.mp4)

add_executable(test_visual_routes test_visual_routes.cpp)
target_compile_definitions(test_visual_routes PRIVATE EDWARD_DESKTOP_QML_DIR="${CMAKE_SOURCE_DIR}/src/desktop/qml")
add_test(NAME desktop.visual_routes COMMAND test_visual_routes)
