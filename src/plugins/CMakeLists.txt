add_library(edward_plugins STATIC
  include/edward/plugins/plugin_manifest.hpp
  include/edward/plugins/plugin_host.hpp
  src/plugin_manifest.cpp
  src/plugin_host.cpp
)
target_include_directories(edward_plugins PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_plugins PUBLIC Qt6::Core)
