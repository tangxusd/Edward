add_test(NAME packaging.macos_app_structure
  COMMAND ${CMAKE_COMMAND}
    -DBUNDLE_DIR=$<TARGET_BUNDLE_DIR:edward_app>
    -DEXECUTABLE_NAME=Edward
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_macos_app_structure.cmake)
set_tests_properties(packaging.macos_app_structure PROPERTIES DEPENDS desktop.visual_routes)

add_test(NAME packaging.macos_dependency_audit
  COMMAND ${CMAKE_COMMAND}
    -DBUNDLE_EXECUTABLE=$<TARGET_FILE:edward_app>
    -DREPORT_PATH=${CMAKE_BINARY_DIR}/macos-dependency-audit.json
    -P ${CMAKE_CURRENT_SOURCE_DIR}/audit_macos_bundle_dependencies.cmake)
set_tests_properties(packaging.macos_dependency_audit PROPERTIES DEPENDS packaging.macos_app_structure)

add_test(NAME packaging.macos_package_gate
  COMMAND ${CMAKE_COMMAND}
    -DBINARY_DIR=${CMAKE_BINARY_DIR}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_macos_package_gate.cmake)
set_tests_properties(packaging.macos_package_gate PROPERTIES DEPENDS packaging.macos_app_structure)

add_custom_target(prepare_macos_development_inputs
  COMMAND ${CMAKE_COMMAND}
    -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs
    -P ${CMAKE_SOURCE_DIR}/packaging/macos/PrepareDevelopmentInputs.cmake
  COMMENT "Generate development-only macOS package inputs")
