#pragma once

#include "edward/resources/component_package.hpp"

#include <QString>

#include <filesystem>
#include <optional>
#include <vector>

namespace edward::resources {

struct ComponentLibraryItem final {
  QString resourceId;
  QString displayName;
  QString category;
  std::filesystem::path directory;
};

class ComponentLibrary final {
 public:
  explicit ComponentLibrary(std::filesystem::path root = {});
  void setRoot(std::filesystem::path root);
  [[nodiscard]] const std::filesystem::path& root() const { return root_; }
  [[nodiscard]] std::vector<ComponentLibraryItem> list(QString* error = nullptr) const;
  bool save(const ComponentPackage& package, QString* error = nullptr) const;
  [[nodiscard]] std::optional<ComponentPackage> load(const QString& resourceId,
                                                     QString* error = nullptr) const;

 private:
  std::filesystem::path root_;
};

}  // namespace edward::resources
