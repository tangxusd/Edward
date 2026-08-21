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

add_custom_target(prepare_macos_development_mlt_runtime
  COMMAND ${CMAKE_COMMAND}
    -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs
    -DMLT_MODULE_DIR=${EDWARD_MLT_MODULE_DIR}
    -DMLT_DATA_DIR=${EDWARD_MLT_DATA_DIR}
    -P ${CMAKE_SOURCE_DIR}/packaging/macos/PrepareDevelopmentMltRuntime.cmake
  DEPENDS prepare_macos_development_inputs
  COMMENT "Copy real MLT runtime into development-only package inputs")

add_custom_target(package_macos_development
  COMMAND ${CMAKE_COMMAND}
    -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:edward_app>
    -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/macos-development-package
    -DLICENSE_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs/licenses
    -DTHIRD_PARTY_NOTICES=${CMAKE_BINARY_DIR}/macos-development-inputs/THIRD_PARTY_NOTICES_PLACEHOLDER.md
    -DBASE_RESOURCES_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs/resources/base
    -DMLT_MODULE_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs/mlt/modules
    -DMLT_DATA_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs/mlt/data
    -DFONT_DIR=${CMAKE_BINARY_DIR}/macos-development-inputs/fonts
    -DFONT_MANIFEST=${CMAKE_BINARY_DIR}/macos-development-inputs/font-manifest-placeholder.json
    -DDEVELOPMENT_ONLY=ON
    -DDEVELOPMENT_MARKER=${CMAKE_BINARY_DIR}/macos-development-inputs/NOT_FOR_DISTRIBUTION.txt
    -P ${CMAKE_SOURCE_DIR}/packaging/macos/Package.cmake
  DEPENDS edward_app prepare_macos_development_inputs
  COMMENT "Package development-only macOS app (not for distribution)")

add_custom_target(audit_macos_development_bundle
  COMMAND ${CMAKE_COMMAND}
    -DBUNDLE_DIR=${CMAKE_BINARY_DIR}/macos-development-package/Edward.app
    -DREPORT_PATH=${CMAKE_BINARY_DIR}/macos-development-package/bundle-tree-dependency-audit.json
    -P ${CMAKE_CURRENT_SOURCE_DIR}/audit_macos_bundle_tree_dependencies.cmake
  DEPENDS package_macos_development
  COMMENT "Audit every bundled macOS dynamic library")
