cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

set(LICENSE_DIR "${OUTPUT_DIR}/licenses")
set(BASE_RESOURCES_DIR "${OUTPUT_DIR}/resources/base")
set(MLT_MODULE_DIR "${OUTPUT_DIR}/mlt/modules")
set(MLT_DATA_DIR "${OUTPUT_DIR}/mlt/data")
set(FONT_DIR "${OUTPUT_DIR}/fonts")
file(MAKE_DIRECTORY
  "${LICENSE_DIR}"
  "${BASE_RESOURCES_DIR}"
  "${MLT_MODULE_DIR}"
  "${MLT_DATA_DIR}"
  "${FONT_DIR}")

file(WRITE "${OUTPUT_DIR}/NOT_FOR_DISTRIBUTION.txt"
  "Edward development package inputs only. Replace every placeholder before release.\n")
file(WRITE "${LICENSE_DIR}/LICENSE_PLACEHOLDER.txt"
  "PLACEHOLDER: add the real Edward license before distribution.\n")
file(WRITE "${OUTPUT_DIR}/THIRD_PARTY_NOTICES_PLACEHOLDER.md"
  "# PLACEHOLDER\n\nAdd complete third-party notices before distribution.\n")
file(WRITE "${BASE_RESOURCES_DIR}/BASE_RESOURCES_PLACEHOLDER.txt"
  "PLACEHOLDER: add real base resources before distribution.\n")
file(WRITE "${MLT_MODULE_DIR}/MLT_MODULES_PLACEHOLDER.txt"
  "PLACEHOLDER: copy redistributable MLT modules before distribution.\n")
file(WRITE "${MLT_DATA_DIR}/MLT_DATA_PLACEHOLDER.txt"
  "PLACEHOLDER: copy redistributable MLT data before distribution.\n")
file(WRITE "${FONT_DIR}/FONT_PLACEHOLDER.txt"
  "PLACEHOLDER: add fonts with redistribution rights before distribution.\n")
file(WRITE "${OUTPUT_DIR}/font-manifest-placeholder.json"
  "{\"status\":\"placeholder\",\"distributionReady\":false}\n")

message(STATUS "Generated development-only package inputs at ${OUTPUT_DIR}")
message(WARNING "These inputs are NOT_FOR_DISTRIBUTION and must not be used for release")
