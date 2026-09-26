#include "edward/desktop/artifact_intake.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace edward::desktop {
namespace {
void fail(QString* error, const QString& value) { if (error) *error = value; }
QString kindFor(const QString& path) {
  const auto ext = QFileInfo(path).suffix().toLower();
  if (ext == "svg") return "svg";
  if (ext == "html" || ext == "css") return "html-css";
  if (ext == "jsx" || ext == "tsx") return "react";
  if (ext == "js" || ext == "mjs") return "gsap-or-js";
  return "asset";
}
bool unsafeText(const QByteArray& data) {
  const auto value = QString::fromUtf8(data).toLower();
  return value.contains("http://") || value.contains("https://") || value.contains("//cdn.") ||
         value.contains("api_key") || value.contains("authorization:") || value.contains("bearer ");
}
}

ArtifactPackage ArtifactIntake::inspect(const QString& root, QString* error) {
  ArtifactPackage result;
  const QFileInfo rootInfo(root);
  if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()) { fail(error, "artifact root must be a real directory"); return {}; }
  result.root = rootInfo.canonicalFilePath();
  QFile manifest(QDir(result.root).filePath("artifact.json"));
  if (!manifest.open(QIODevice::ReadOnly)) { fail(error, "artifact.json is required"); return {}; }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(manifest.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) { fail(error, "artifact.json is invalid"); return {}; }
  const auto entry = document.object().value("entry").toString();
  if (entry.isEmpty() || QDir::isAbsolutePath(entry) || entry.contains("..")) { fail(error, "artifact entry is unsafe"); return {}; }
  const QFileInfo entryInfo(QDir(result.root).filePath(entry));
  if (!entryInfo.exists() || !entryInfo.isFile() || entryInfo.isSymLink() || entryInfo.canonicalFilePath().startsWith(result.root + "/") == false) { fail(error, "artifact entry is missing or outside root"); return {}; }
  result.entry = entry;
  constexpr qint64 kMaxFileBytes = 32 * 1024 * 1024;
  constexpr qint64 kMaxPackageBytes = 256 * 1024 * 1024;
  qint64 totalBytes = 0;
  QDirIterator it(result.root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QFileInfo info(it.next());
    if (info.isSymLink()) { fail(error, "artifact contains a symlink"); return {}; }
    if (info.size() > kMaxFileBytes || (totalBytes += info.size()) > kMaxPackageBytes) { fail(error, "artifact exceeds size limits"); return {}; }
    QFile file(info.filePath());
    if (!file.open(QIODevice::ReadOnly)) { fail(error, "artifact file cannot be read"); return {}; }
    const auto data = file.readAll();
    if (unsafeText(data)) { fail(error, "artifact contains network or credential access"); return {}; }
    result.files.push_back({QDir(result.root).relativeFilePath(info.filePath()), kindFor(info.filePath()), QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())});
  }
  return result;
}
}  // namespace edward::desktop
