add_library(edward_core STATIC
  src/media_project.cpp
  src/timeline.cpp
  src/timeline_commands.cpp
)
target_include_directories(edward_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
