add_library(edward_core STATIC
  src/media_project.cpp
  src/timeline.cpp
  src/timeline_commands.cpp
  src/component_ir.cpp
  src/component_edit_command.cpp
  src/component_edit_command_parser.cpp
  src/project_identity.cpp
)
target_include_directories(edward_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_core PUBLIC Qt6::Core)
