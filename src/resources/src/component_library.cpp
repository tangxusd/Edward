#include "edward/resources/component_library.hpp"

#include <algorithm>

namespace edward::resources {

namespace {
void setError(QString* error, const QString& value) {
  if (error) *error = value;
}
}

ComponentLibrary::ComponentLibrary(std::filesystem::path root) : root_(std::move(root)) {}

void ComponentLibrary::setRoot(std::filesystem::path root) { root_ = std::move(root); }

std::vector<ComponentLibraryItem> ComponentLibrary::list(QString* error) const {
  std::vector<ComponentLibraryItem> result;
  if (root_.empty()) return result;
  std::error_code ec;
  if (!std::filesystem::exists(root_, ec)) return result;
  if (ec || !std::filesystem::is_directory(root_, ec)) {
    setError(error, QStringLiteral("组件资源库目录不可用"));
    return result;
  }
  for (const auto& entry : std::filesystem::directory_iterator(root_, ec)) {
    if (ec) break;
    if (!entry.is_directory()) continue;
    QString packageError;
    const auto package = ComponentPackage::load(entry.path(), &packageError);
    if (!package) continue;
    result.push_back({package->resourceId, package->displayName, package->category, entry.path()});
  }
  if (ec) setError(error, QStringLiteral("组件资源库读取失败"));
  std::ranges::sort(result, [](const auto& left, const auto& right) {
    return left.displayName.localeAwareCompare(right.displayName) < 0;
  });
  return result;
}

bool ComponentLibrary::save(const ComponentPackage& package, QString* error) const {
  if (root_.empty()) {
    setError(error, QStringLiteral("组件资源库目录未配置"));
    return false;
  }
  if (!package.validate(error)) return false;
  return package.saveLocal(root_ / package.resourceId.toStdString(), error);
}

std::optional<ComponentPackage> ComponentLibrary::load(const QString& resourceId, QString* error) const {
  if (root_.empty() || resourceId.isEmpty()) {
    setError(error, QStringLiteral("组件资源库目录或资源 ID 无效"));
    return std::nullopt;
  }
  return ComponentPackage::load(root_ / resourceId.toStdString(), error);
}

}  // namespace edward::resources
