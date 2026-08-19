add_executable(test_dependency_manifest test_dependency_manifest.cpp)
add_test(
  NAME contract.dependency
  COMMAND test_dependency_manifest
    "${CMAKE_SOURCE_DIR}/third_party/shotcut/SOURCES.json"
)

add_executable(test_component_package_schema test_component_package_schema.cpp)
add_test(
  NAME contract.component_package_schema
  COMMAND test_component_package_schema
    "${CMAKE_SOURCE_DIR}/docs/contracts/component-package.schema.json"
)
