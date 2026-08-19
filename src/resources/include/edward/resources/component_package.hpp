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
  QString thumbnail;
  std::vector<QString> assets;

  static std::optional<ComponentPackage> load(const std::filesystem::path& directory, QString* error = nullptr);
  bool validate(QString* error = nullptr) const;
  bool saveLocal(const std::filesystem::path& directory, QString* error = nullptr) const;
};

}  // namespace edward::resources
