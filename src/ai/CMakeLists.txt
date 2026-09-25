add_library(edward_ai STATIC
  src/action_plan.cpp
  src/intent_plan.cpp
  src/ai_transaction.cpp
  src/rule_file_loader.cpp
  src/memory_store.cpp
  src/resource_matcher.cpp
  src/ai_orchestrator.cpp
  src/capability_registry.cpp
  src/intent_resolver.cpp
)
target_include_directories(edward_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_ai PUBLIC Qt6::Core Qt6::Sql)
