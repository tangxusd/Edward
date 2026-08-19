add_library(edward_plugins STATIC
  include/edward/plugins/plugin_manifest.hpp
  include/edward/plugins/plugin_host.hpp
  include/edward/plugins/installed_plugin.hpp
  include/edward/plugins/plugin_runtime_resolver.hpp
  include/edward/plugins/plugin_trust_store.hpp
  include/edward/plugins/plugin_sandbox_policy.hpp
  src/plugin_manifest.cpp
  src/plugin_host.cpp
  src/installed_plugin.cpp
  src/plugin_runtime_resolver.cpp
  src/plugin_trust_store.cpp
  src/plugin_sandbox_policy.cpp
)
target_include_directories(edward_plugins PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(edward_plugins PUBLIC edward_core Qt6::Core Qt6::Gui PkgConfig::EDWARD_SODIUM)
target_compile_definitions(edward_plugins PRIVATE $<$<CONFIG:Release>:EDWARD_RELEASE_BUILD>)
set(EDWARD_PLUGIN_NODE_SHA256 "" CACHE STRING "SHA-256 of the Node runtime bundled with Edward")
set(EDWARD_PLUGIN_TRUSTED_KEY_ID "" CACHE STRING "Trusted Ed25519 key ID for release plugin manifests")
set(EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64 "" CACHE STRING "Trusted Ed25519 public key (Base64) for release plugin manifests")
if(EDWARD_PLUGIN_NODE_SHA256)
  target_compile_definitions(edward_plugins PRIVATE EDWARD_PLUGIN_NODE_SHA256="${EDWARD_PLUGIN_NODE_SHA256}")
endif()
if(EDWARD_PLUGIN_TRUSTED_KEY_ID AND EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64)
  target_compile_definitions(edward_plugins PRIVATE
    EDWARD_PLUGIN_TRUSTED_KEY_ID="${EDWARD_PLUGIN_TRUSTED_KEY_ID}"
    EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64="${EDWARD_PLUGIN_TRUSTED_PUBLIC_KEY_B64}")
endif()
