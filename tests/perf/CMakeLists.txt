add_executable(edward_benchmark run_benchmark.cpp)
target_link_libraries(edward_benchmark PRIVATE edward_media Qt6::Core)
if(WIN32)
  target_link_libraries(edward_benchmark PRIVATE Psapi)
endif()

add_executable(test_performance_contract test_performance_contract.cpp)
target_link_libraries(test_performance_contract PRIVATE Qt6::Core)
add_dependencies(test_performance_contract edward_benchmark)
add_test(NAME performance.metrics COMMAND test_performance_contract $<TARGET_FILE:edward_benchmark>)

add_executable(test_performance_collection test_performance_collection.cpp)
target_link_libraries(test_performance_collection PRIVATE Qt6::Core)
add_dependencies(test_performance_collection edward_benchmark media_fixture)
add_test(NAME performance.collection COMMAND test_performance_collection
  $<TARGET_FILE:edward_benchmark> ${CMAKE_BINARY_DIR}/tests/media/fixture.mp4)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME performance.vfr_fixture_generator
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_generate_vfr_fixture.py
    --tool ${CMAKE_SOURCE_DIR}/tools/generate_vfr_fixture.py
    --workdir ${CMAKE_CURRENT_BINARY_DIR}/vfr-fixture-generator)
