#include "edward/plugins/plugin_runtime_resolver.hpp"

#include <QStandardPaths>

namespace edward::plugins {
namespace {

QString executableName(const QString& runtime) {
#ifdef Q_OS_WIN
  return runtime + QStringLiteral(".exe");
#else
  return runtime;
#endif
}

bool regularFile(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::symlink_status(path, error)) && !error;
}

}  // namespace

std::optional<QString> resolvePluginRuntime(const PluginManifest& manifest,
                                            const PluginRuntimeResolution& resolution,
                                            QString* error) {
  if (manifest.runtime == QStringLiteral("native")) return QStringLiteral("native");
  if (manifest.runtime != QStringLiteral("node") && manifest.runtime != QStringLiteral("bun")) {
    if (error) *error = QStringLiteral("plugin runtime is not supported");
    return std::nullopt;
  }
  if (!resolution.bundledRoot.empty()) {
    const auto bundled = resolution.bundledRoot / executableName(manifest.runtime).toStdString();
    if (regularFile(bundled)) return QString::fromStdString(bundled.string());
    if (error) *error = QStringLiteral("bundled plugin runtime is unavailable");
    return std::nullopt;
  }
  if (!resolution.allowDevelopmentPath) {
    if (error) *error = QStringLiteral("bundled plugin runtime is required");
    return std::nullopt;
  }
  const auto path = QStandardPaths::findExecutable(manifest.runtime);
  if (path.isEmpty()) {
    if (error) *error = QStringLiteral("development plugin runtime is unavailable: %1").arg(manifest.runtime);
    return std::nullopt;
  }
  return path;
}

}  // namespace edward::plugins
