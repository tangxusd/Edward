add_library(edward_ai STATIC
  src/action_plan.cpp
  src/ai_transaction.cpp
)
target_include_directories(edward_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_ai PUBLIC Qt6::Core)
