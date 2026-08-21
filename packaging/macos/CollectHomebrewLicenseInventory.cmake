cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required")
endif()
find_program(BREW_EXECUTABLE brew REQUIRED)

execute_process(
  COMMAND "${BREW_EXECUTABLE}" deps --formula mlt
  RESULT_VARIABLE DEPS_RESULT
  OUTPUT_VARIABLE DEPS_OUTPUT
  ERROR_VARIABLE DEPS_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT DEPS_RESULT EQUAL 0)
  message(FATAL_ERROR "brew deps failed: ${DEPS_ERROR}")
endif()
string(REPLACE "\n" ";" formulas "${DEPS_OUTPUT}")
list(APPEND formulas mlt ffmpeg)
list(REMOVE_DUPLICATES formulas)

set(INVENTORY_DIR "${OUTPUT_DIR}/license-inventory")
file(REMOVE_RECURSE "${INVENTORY_DIR}")
file(MAKE_DIRECTORY "${INVENTORY_DIR}")
file(WRITE "${INVENTORY_DIR}/inventory.tsv" "formula\tbrew_prefix\tlicense_files\n")

foreach(formula IN LISTS formulas)
  if(formula STREQUAL "")
    continue()
  endif()
  execute_process(
    COMMAND "${BREW_EXECUTABLE}" --prefix "${formula}"
    RESULT_VARIABLE PREFIX_RESULT
    OUTPUT_VARIABLE PREFIX
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT PREFIX_RESULT EQUAL 0)
    file(APPEND "${INVENTORY_DIR}/inventory.tsv" "${formula}\tNOT_INSTALLED\t0\n")
    continue()
  endif()
  file(GLOB license_files
    "${PREFIX}/LICENSE*" "${PREFIX}/COPYING*" "${PREFIX}/NOTICE*"
    "${PREFIX}/COPYRIGHT*")
  list(LENGTH license_files license_count)
  file(APPEND "${INVENTORY_DIR}/inventory.tsv" "${formula}\t${PREFIX}\t${license_count}\n")
  if(license_files)
    file(MAKE_DIRECTORY "${INVENTORY_DIR}/${formula}")
    file(COPY ${license_files} DESTINATION "${INVENTORY_DIR}/${formula}")
  endif()
endforeach()

message(STATUS "Generated Homebrew license inventory at ${INVENTORY_DIR}")
