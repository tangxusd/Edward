add_library(edward_resolve STATIC
  include/edward/resolve/resolve_types.hpp
  include/edward/resolve/resolve_connection.hpp
  src/resolve_connection.cpp
)
target_include_directories(edward_resolve PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_resolve PUBLIC Qt6::Core Qt6::Network)
