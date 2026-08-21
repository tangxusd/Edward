add_library(edward_resolve STATIC
  include/edward/resolve/resolve_types.hpp
  include/edward/resolve/resolve_connection.hpp
  include/edward/resolve/resolve_adapter.hpp
  src/resolve_connection.cpp
  src/resolve_adapter.cpp
)
target_include_directories(edward_resolve PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_resolve PUBLIC edward_core Qt6::Core Qt6::Network)
