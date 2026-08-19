set(CMAKE_AUTOMOC ON)

add_library(edward_resources STATIC
  include/edward/resources/component_upload.hpp
  src/component_package.cpp
  src/component_upload.cpp
)
target_include_directories(edward_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_resources PUBLIC edward_core Qt6::Core Qt6::Network)
