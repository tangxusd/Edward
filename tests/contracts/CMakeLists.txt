add_executable(test_dependency_manifest test_dependency_manifest.cpp)
add_test(
  NAME contract.dependency
  COMMAND test_dependency_manifest
    "${CMAKE_SOURCE_DIR}/third_party/shotcut/SOURCES.json"
)
