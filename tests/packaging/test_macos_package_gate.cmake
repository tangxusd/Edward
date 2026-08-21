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
    -P "${PACKAGE_SCRIPT}"
  RESULT_VARIABLE RESULT
  OUTPUT_VARIABLE OUTPUT
  ERROR_VARIABLE ERROR)
if(RESULT EQUAL 0)
  message(FATAL_ERROR "Packaging unexpectedly succeeded without release inputs")
endif()
if(NOT ERROR MATCHES "LICENSE_DIR is required|THIRD_PARTY_NOTICES is required|BASE_RESOURCES_DIR is required|MLT_MODULE_DIR is required|MLT_DATA_DIR is required")
  message(FATAL_ERROR "Packaging failed for an unexpected reason: ${ERROR}")
endif()
