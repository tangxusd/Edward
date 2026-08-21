cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED APP_BUNDLE OR NOT IS_DIRECTORY "${APP_BUNDLE}")
  message(FATAL_ERROR "APP_BUNDLE must point to an existing .app directory")
endif()
if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required")
endif()
if(NOT DEFINED LICENSE_DIR OR NOT IS_DIRECTORY "${LICENSE_DIR}")
  message(FATAL_ERROR "LICENSE_DIR is required; refusing to package without real license files")
endif()
if(NOT DEFINED THIRD_PARTY_NOTICES OR NOT EXISTS "${THIRD_PARTY_NOTICES}")
  message(FATAL_ERROR "THIRD_PARTY_NOTICES is required; refusing to package without notices")
endif()
if(NOT DEFINED BASE_RESOURCES_DIR OR NOT IS_DIRECTORY "${BASE_RESOURCES_DIR}")
  message(FATAL_ERROR "BASE_RESOURCES_DIR is required; refusing to package without base resources")
endif()

find_program(MACDEPLOYQT_EXECUTABLE macdeployqt REQUIRED)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
get_filename_component(APP_NAME "${APP_BUNDLE}" NAME)
set(OUTPUT_BUNDLE "${OUTPUT_DIR}/${APP_NAME}")
file(REMOVE_RECURSE "${OUTPUT_BUNDLE}")
file(COPY "${APP_BUNDLE}" DESTINATION "${OUTPUT_DIR}")

execute_process(
  COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${OUTPUT_BUNDLE}" -always-overwrite
  RESULT_VARIABLE MACDEPLOYQT_RESULT
  OUTPUT_VARIABLE MACDEPLOYQT_OUTPUT
  ERROR_VARIABLE MACDEPLOYQT_ERROR)
if(NOT MACDEPLOYQT_RESULT EQUAL 0)
  message(FATAL_ERROR "macdeployqt failed: ${MACDEPLOYQT_ERROR}")
endif()

file(COPY "${LICENSE_DIR}" DESTINATION "${OUTPUT_BUNDLE}/Contents/Resources")
file(COPY "${THIRD_PARTY_NOTICES}" DESTINATION "${OUTPUT_BUNDLE}/Contents/Resources")
file(COPY "${BASE_RESOURCES_DIR}/" DESTINATION "${OUTPUT_BUNDLE}/Contents/Resources")
message(STATUS "Packaged ${OUTPUT_BUNDLE}")
