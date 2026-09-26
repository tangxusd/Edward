#pragma once

#include "edward/core/native_runtime_component.hpp"
#include "edward/runtime/runtime_manifest.hpp"

#include <QString>

#include <filesystem>
#include <optional>
#include <vector>

namespace edward::resources {

struct ComponentPackage final {
  QString resourceId;
  QString displayName;
  edward::core::NativeRuntimeComponent nativeRuntime;
  edward::runtime::RuntimeManifest runtimeManifest;
  QString thumbnail;
  std::vector<QString> assets;
  QString category = QStringLiteral("my");

  static std::optional<ComponentPackage> load(const std::filesystem::path& directory, QString* error = nullptr);
  bool validate(QString* error = nullptr) const;
  bool saveLocal(const std::filesystem::path& directory, QString* error = nullptr) const;
};

}  // namespace edward::resources
