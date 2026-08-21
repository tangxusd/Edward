add_executable(test_0_3_0_smoke test_0_3_0_smoke.cpp)
target_link_libraries(test_0_3_0_smoke PRIVATE edward_desktop edward_media)
add_dependencies(test_0_3_0_smoke media_fixture)
add_test(NAME e2e.edward_0_3_0_smoke
  COMMAND test_0_3_0_smoke ${CMAKE_BINARY_DIR}/tests/media/fixture.mp4 ${CMAKE_CURRENT_BINARY_DIR}/edward-0.3.0-smoke.mp4)

add_executable(test_real_media_import test_real_media_import.cpp)
target_link_libraries(test_real_media_import PRIVATE edward_desktop edward_media)

add_executable(test_real_media_export test_real_media_export.cpp)
target_link_libraries(test_real_media_export PRIVATE edward_media)

add_executable(test_resolve_studio_subtitle test_resolve_studio_subtitle.cpp)
target_link_libraries(test_resolve_studio_subtitle PRIVATE edward_resolve edward_core Qt6::Core Qt6::Network)
add_test(NAME e2e.resolve_studio_subtitle COMMAND test_resolve_studio_subtitle)
