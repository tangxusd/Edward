cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED OUTPUT_PATH OR OUTPUT_PATH STREQUAL "")
  message(FATAL_ERROR "OUTPUT_PATH is required")
endif()
foreach(id IN ITEMS 1080P 4K VFR)
  if(NOT DEFINED FIXTURE_${id} OR NOT EXISTS "${FIXTURE_${id}}")
    message(FATAL_ERROR "FIXTURE_${id} must point to an existing real media file")
  endif()
endforeach()

file(SHA256 "${FIXTURE_1080P}" HASH_1080P)
file(SHA256 "${FIXTURE_4K}" HASH_4K)
file(SHA256 "${FIXTURE_VFR}" HASH_VFR)
file(WRITE "${OUTPUT_PATH}"
  "{\n"
  "  \"fixtures\": [\n"
  "    {\"id\": \"1080p\", \"path\": \"${FIXTURE_1080P}\", \"sha256\": \"${HASH_1080P}\"},\n"
  "    {\"id\": \"4k\", \"path\": \"${FIXTURE_4K}\", \"sha256\": \"${HASH_4K}\"},\n"
  "    {\"id\": \"vfr\", \"path\": \"${FIXTURE_VFR}\", \"sha256\": \"${HASH_VFR}\"}\n"
  "  ]\n"
  "}\n")
message(STATUS "Generated performance fixture manifest at ${OUTPUT_PATH}")
