add_test(NAME packaging.macos_app_structure
  COMMAND ${CMAKE_COMMAND}
    -DBUNDLE_DIR=$<TARGET_BUNDLE_DIR:edward_app>
    -DEXECUTABLE_NAME=Edward
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_macos_app_structure.cmake)
set_tests_properties(packaging.macos_app_structure PROPERTIES DEPENDS desktop.visual_routes)
