set(CMAKE_AUTOMOC ON)

add_library(edward_resources STATIC
  include/edward/resources/component_upload.hpp
  include/edward/resources/auth_session_store.hpp
  src/component_package.cpp
  src/component_upload.cpp
  src/auth_session_store.cpp
)
target_include_directories(edward_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_resources PUBLIC edward_core Qt6::Core Qt6::Network)
