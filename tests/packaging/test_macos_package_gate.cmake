set(APP_BUNDLE "${BINARY_DIR}/bin/Edward.app")
set(OUTPUT_DIR "${BINARY_DIR}/macos-package")
set(PACKAGE_SCRIPT "${SOURCE_DIR}/packaging/macos/Package.cmake")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -DAPP_BUNDLE=${APP_BUNDLE}
    -DOUTPUT_DIR=${OUTPUT_DIR}
    -DLICENSE_DIR=${SOURCE_DIR}/licenses
    -DTHIRD_PARTY_NOTICES=${SOURCE_DIR}/licenses/THIRD_PARTY_NOTICES.md
    -DBASE_RESOURCES_DIR=${SOURCE_DIR}/resources/base
    -DMLT_MODULE_DIR=${SOURCE_DIR}/third_party/mlt/modules
    -DMLT_DATA_DIR=${SOURCE_DIR}/third_party/mlt/data
    -DFONT_DIR=${SOURCE_DIR}/assets/fonts
    -DFONT_MANIFEST=${SOURCE_DIR}/docs/contracts/font-manifest.json
    -P "${PACKAGE_SCRIPT}"
  RESULT_VARIABLE RESULT
  OUTPUT_VARIABLE OUTPUT
  ERROR_VARIABLE ERROR)
if(RESULT EQUAL 0)
  message(FATAL_ERROR "Packaging unexpectedly succeeded without release inputs")
endif()
if(NOT ERROR MATCHES "LICENSE_DIR is required|THIRD_PARTY_NOTICES is required|BASE_RESOURCES_DIR is required|MLT_MODULE_DIR is required|MLT_DATA_DIR is required|FONT_DIR is required|FONT_MANIFEST is required")
  message(FATAL_ERROR "Packaging failed for an unexpected reason: ${ERROR}")
endif()

set(EMPTY_ROOT "${BINARY_DIR}/macos-package-empty-inputs")
file(MAKE_DIRECTORY
  "${EMPTY_ROOT}/licenses"
  "${EMPTY_ROOT}/resources"
  "${EMPTY_ROOT}/mlt/modules"
  "${EMPTY_ROOT}/mlt/data"
  "${EMPTY_ROOT}/fonts")
file(WRITE "${EMPTY_ROOT}/THIRD_PARTY_NOTICES.md" "notice")
file(WRITE "${EMPTY_ROOT}/font-manifest.json" "{}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -DAPP_BUNDLE=${APP_BUNDLE}
    -DOUTPUT_DIR=${OUTPUT_DIR}
    -DLICENSE_DIR=${EMPTY_ROOT}/licenses
    -DTHIRD_PARTY_NOTICES=${EMPTY_ROOT}/THIRD_PARTY_NOTICES.md
    -DBASE_RESOURCES_DIR=${EMPTY_ROOT}/resources
    -DMLT_MODULE_DIR=${EMPTY_ROOT}/mlt/modules
    -DMLT_DATA_DIR=${EMPTY_ROOT}/mlt/data
    -DFONT_DIR=${EMPTY_ROOT}/fonts
    -DFONT_MANIFEST=${EMPTY_ROOT}/font-manifest.json
    -P "${PACKAGE_SCRIPT}"
  RESULT_VARIABLE EMPTY_RESULT
  ERROR_VARIABLE EMPTY_ERROR)
if(EMPTY_RESULT EQUAL 0)
  message(FATAL_ERROR "Packaging unexpectedly succeeded with empty release inputs")
endif()
if(NOT EMPTY_ERROR MATCHES "must contain real files")
  message(FATAL_ERROR "Empty input rejection failed: ${EMPTY_ERROR}")
endif()
