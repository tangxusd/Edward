#include "edward/plugins/plugin_runtime_resolver.hpp"

#include <QCryptographicHash>
#include <QFile>
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

std::optional<QString> sha256(const std::filesystem::path& path) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    const auto bytes = file.read(64 * 1024);
    if (bytes.isEmpty() && file.error() != QFile::NoError) return std::nullopt;
    hash.addData(bytes);
  }
  return QString::fromLatin1(hash.result().toHex());
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
    if (!regularFile(bundled)) {
      if (error) *error = QStringLiteral("bundled plugin runtime is unavailable");
      return std::nullopt;
    }
    if (resolution.requireIntegrity && resolution.expectedSha256.isEmpty()) {
      if (error) *error = QStringLiteral("bundled plugin runtime checksum is required");
      return std::nullopt;
    }
    if (!resolution.expectedSha256.isEmpty()) {
      const auto actualSha256 = sha256(bundled);
      if (!actualSha256 || actualSha256->compare(resolution.expectedSha256, Qt::CaseInsensitive) != 0) {
        if (error) *error = QStringLiteral("bundled plugin runtime checksum mismatch");
        return std::nullopt;
      }
    }
    return QString::fromStdString(bundled.string());
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
