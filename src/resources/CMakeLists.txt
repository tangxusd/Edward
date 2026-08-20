set(CMAKE_AUTOMOC ON)

add_library(edward_resources STATIC
  include/edward/resources/component_upload.hpp
  include/edward/resources/auth_session_store.hpp
  include/edward/resources/supabase_auth_client.hpp
  include/edward/resources/model_chat_client.hpp
  include/edward/resources/component_upload_queue.hpp
  include/edward/resources/component_upload_scheduler.hpp
  include/edward/resources/component_upload_dispatcher.hpp
  include/edward/resources/component_library.hpp
  src/component_package.cpp
  src/component_upload.cpp
  src/auth_session_store.cpp
  src/supabase_auth_client.cpp
  src/model_chat_client.cpp
  src/component_upload_queue.cpp
  src/component_upload_scheduler.cpp
  src/component_upload_dispatcher.cpp
  src/component_library.cpp
)
target_include_directories(edward_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_resources PUBLIC edward_core Qt6::Core Qt6::Network)
