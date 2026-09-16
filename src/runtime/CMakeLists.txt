add_library(edward_runtime STATIC
  src/runtime_manifest.cpp
  src/web_runtime_host.cpp
)
target_include_directories(edward_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_runtime PUBLIC Qt6::Core)
