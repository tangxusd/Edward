set(CMAKE_AUTOMOC ON)
add_library(edward_desktop STATIC
  include/edward/desktop/workbench_runtime.hpp
  src/timeline_controller.cpp
  src/workbench_runtime.cpp
)
target_include_directories(edward_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_desktop PUBLIC edward_core edward_media edward_plugins edward_resources Qt6::Core Qt6::Concurrent)

add_executable(edward_app src/main.cpp)
target_compile_definitions(edward_app PRIVATE EDWARD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(edward_app PRIVATE edward_desktop Qt6::Gui Qt6::Quick Qt6::QuickControls2)
set_target_properties(edward_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
