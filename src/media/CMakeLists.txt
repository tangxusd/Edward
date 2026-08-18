add_library(edward_media STATIC src/media_probe.cpp)
target_include_directories(edward_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_media PUBLIC edward_core edward_media_runtime)
