if(NOT DEFINED BUNDLE_DIR OR BUNDLE_DIR STREQUAL "")
  message(FATAL_ERROR "BUNDLE_DIR is required")
endif()

if(NOT EXISTS "${BUNDLE_DIR}")
  message(FATAL_ERROR "Expected macOS app bundle does not exist: ${BUNDLE_DIR}")
endif()

if(NOT EXISTS "${BUNDLE_DIR}/Contents/Info.plist")
  message(FATAL_ERROR "App bundle is missing Contents/Info.plist")
endif()

if(NOT EXISTS "${BUNDLE_DIR}/Contents/MacOS/${EXECUTABLE_NAME}")
  message(FATAL_ERROR "App bundle is missing Contents/MacOS/${EXECUTABLE_NAME}")
endif()
