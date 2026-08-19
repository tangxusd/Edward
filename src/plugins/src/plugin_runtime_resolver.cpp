#include "edward/plugins/plugin_runtime_resolver.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
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
    const auto resolved = QString::fromStdString(bundled.string());
    if (resolution.requireVersion && resolution.expectedVersion.isEmpty()) {
      if (error) *error = QStringLiteral("bundled plugin runtime version is required");
      return std::nullopt;
    }
    if (!resolution.expectedVersion.isEmpty() &&
        !verifyPluginRuntimeVersion(resolved, resolution.expectedVersion, error)) return std::nullopt;
    return resolved;
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
  if (!resolution.expectedVersion.isEmpty() && !verifyPluginRuntimeVersion(path, resolution.expectedVersion, error))
    return std::nullopt;
  return path;
}

bool verifyPluginRuntimeVersion(const QString& executable, const QString& expectedVersion, QString* error) {
  if (executable.isEmpty() || expectedVersion.isEmpty()) {
    if (error) *error = QStringLiteral("plugin runtime version is required");
    return false;
  }
  QProcess process;
  process.setProgram(executable);
  process.setArguments({QStringLiteral("--version")});
  process.start();
  if (!process.waitForStarted(1000) || !process.waitForFinished(3000) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (error) *error = QStringLiteral("bundled plugin runtime version probe failed");
    return false;
  }
  if (QString::fromUtf8(process.readAllStandardOutput()).trimmed() != expectedVersion) {
    if (error) *error = QStringLiteral("bundled plugin runtime version mismatch");
    return false;
  }
  return true;
}

}  // namespace edward::plugins
