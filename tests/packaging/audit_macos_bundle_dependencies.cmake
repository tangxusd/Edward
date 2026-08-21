if(NOT DEFINED BUNDLE_EXECUTABLE OR NOT EXISTS "${BUNDLE_EXECUTABLE}")
  message(FATAL_ERROR "BUNDLE_EXECUTABLE must point to an existing executable")
endif()
if(NOT DEFINED REPORT_PATH OR REPORT_PATH STREQUAL "")
  message(FATAL_ERROR "REPORT_PATH is required")
endif()

find_program(OTOOL_EXECUTABLE otool REQUIRED)
execute_process(
  COMMAND "${OTOOL_EXECUTABLE}" -L "${BUNDLE_EXECUTABLE}"
  RESULT_VARIABLE OTOOL_RESULT
  OUTPUT_VARIABLE OTOOL_OUTPUT
  ERROR_VARIABLE OTOOL_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT OTOOL_RESULT EQUAL 0)
  message(FATAL_ERROR "otool failed: ${OTOOL_ERROR}")
endif()

set(STATUS "ready_for_dependency_packaging")
set(REASON "all dynamic dependencies are bundle-relative or system frameworks")
if(OTOOL_OUTPUT MATCHES "/opt/homebrew/" OR OTOOL_OUTPUT MATCHES "/usr/local/")
  set(STATUS "developer_machine_dependencies")
  set(REASON "the executable still references Homebrew or local prefix paths")
endif()

string(REPLACE "\\" "\\\\" JSON_OUTPUT "${OTOOL_OUTPUT}")
string(REPLACE "\"" "\\\"" JSON_OUTPUT "${JSON_OUTPUT}")
string(REPLACE "\n" "\\n" JSON_OUTPUT "${JSON_OUTPUT}")
file(WRITE "${REPORT_PATH}"
  "{\n"
  "  \"status\": \"${STATUS}\",\n"
  "  \"reason\": \"${REASON}\",\n"
  "  \"executable\": \"${BUNDLE_EXECUTABLE}\",\n"
  "  \"otool_l\": \"${JSON_OUTPUT}\"\n"
  "}\n")

if(STATUS STREQUAL "developer_machine_dependencies")
  message(STATUS "macOS dependency audit: ${STATUS}; ${REASON}")
else()
  message(STATUS "macOS dependency audit: ${STATUS}")
endif()
