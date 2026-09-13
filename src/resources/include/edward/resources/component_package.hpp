#pragma once

#include "edward/core/component_ir.hpp"

#include <QString>

#include <filesystem>
#include <optional>
#include <vector>

namespace edward::resources {

struct ComponentPackage final {
  QString resourceId;
  QString displayName;
  edward::core::ComponentIr component;
  QString pluginId;
  QString pluginVersion;
  QString thumbnail;
  std::vector<QString> assets;
  QString category = QStringLiteral("my");
  // Public library preferred delivery artifact. The Component IR remains the
  // authoring source, while this precompiled Fusion Composition is the direct
  // Resolve insertion path when present.
  QString fusionComp;
  QString fusionUrl;
  QString fusionSha256;
  QString fusionResolveVersion;
  // Stable invocation target, for example resolve.fusion or premiere.mogrt.
  QString target = QStringLiteral("resolve.fusion");

  static std::optional<ComponentPackage> load(const std::filesystem::path& directory, QString* error = nullptr);
  bool validate(QString* error = nullptr) const;
  bool saveLocal(const std::filesystem::path& directory, QString* error = nullptr) const;
};

}  // namespace edward::resources
