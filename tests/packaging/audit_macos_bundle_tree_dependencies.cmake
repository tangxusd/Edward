if(NOT DEFINED BUNDLE_DIR OR NOT IS_DIRECTORY "${BUNDLE_DIR}")
  message(FATAL_ERROR "BUNDLE_DIR must point to an existing .app directory")
endif()
if(NOT DEFINED REPORT_PATH OR REPORT_PATH STREQUAL "")
  message(FATAL_ERROR "REPORT_PATH is required")
endif()

find_program(FILE_EXECUTABLE file REQUIRED)
find_program(OTOOL_EXECUTABLE otool REQUIRED)
file(GLOB_RECURSE bundle_files LIST_DIRECTORIES false
  "${BUNDLE_DIR}/Contents/Frameworks/*"
  "${BUNDLE_DIR}/Contents/Resources/mlt/modules/*")

set(macho_count 0)
set(developer_references "")
foreach(bundle_file IN LISTS bundle_files)
  execute_process(
    COMMAND "${FILE_EXECUTABLE}" -b "${bundle_file}"
    OUTPUT_VARIABLE file_type
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT file_type MATCHES "Mach-O")
    continue()
  endif()
  math(EXPR macho_count "${macho_count} + 1")
  execute_process(
    COMMAND "${OTOOL_EXECUTABLE}" -L "${bundle_file}"
    RESULT_VARIABLE otool_result
    OUTPUT_VARIABLE otool_output
    ERROR_VARIABLE otool_error)
  if(NOT otool_result EQUAL 0)
    message(FATAL_ERROR "otool failed for ${bundle_file}: ${otool_error}")
  endif()
  string(REGEX REPLACE "^[^\n]*\n[ \t]*[^\n]*\n" "" linked_dependencies "${otool_output}")
  if(linked_dependencies MATCHES "/opt/homebrew/" OR linked_dependencies MATCHES "/usr/local/")
    string(APPEND developer_references "${bundle_file}\n${otool_output}\n")
  endif()
endforeach()

set(status "ready_for_dependency_packaging")
set(reason "all bundled Mach-O files use bundle-relative or system dependencies")
if(developer_references)
  set(status "developer_machine_dependencies")
  set(reason "bundled dynamic libraries still reference a developer prefix")
endif()

string(REPLACE "\\" "\\\\" JSON_REFERENCES "${developer_references}")
string(REPLACE "\"" "\\\"" JSON_REFERENCES "${JSON_REFERENCES}")
string(REPLACE "\n" "\\n" JSON_REFERENCES "${JSON_REFERENCES}")
file(WRITE "${REPORT_PATH}"
  "{\n"
  "  \"status\": \"${status}\",\n"
  "  \"reason\": \"${reason}\",\n"
  "  \"bundle\": \"${BUNDLE_DIR}\",\n"
  "  \"machOFileCount\": ${macho_count},\n"
  "  \"developerReferences\": \"${JSON_REFERENCES}\"\n"
  "}\n")

message(STATUS "macOS bundle tree dependency audit: ${status}; Mach-O files: ${macho_count}")
if(NOT status STREQUAL "ready_for_dependency_packaging")
  message(FATAL_ERROR "${reason}")
endif()
