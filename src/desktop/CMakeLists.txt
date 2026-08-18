add_library(edward_desktop STATIC src/timeline_controller.cpp)
target_include_directories(edward_desktop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_desktop PUBLIC edward_core edward_media Qt6::Core)
