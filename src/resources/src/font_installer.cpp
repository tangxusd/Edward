#include "edward/resources/font_installer.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace edward::resources {

QString FontInstaller::userFontDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/fonts");
}

QByteArray FontInstaller::sha256(const QByteArray& bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

InstallResult FontInstaller::install(const FontAsset& asset) {
  InstallResult result;
  const auto extension = QFileInfo(asset.fileName).suffix().toLower();
  if (asset.fontId.isEmpty() || asset.bytes.isEmpty() || asset.expectedSha256.isEmpty()) {
    result.error = QStringLiteral("字体文件信息不完整");
    return result;
  }
  if (extension != QStringLiteral("ttf") && extension != QStringLiteral("otf") &&
      extension != QStringLiteral("woff") && extension != QStringLiteral("woff2")) {
    result.error = QStringLiteral("字体格式不受支持");
    return result;
  }
  if (asset.bytes.size() > 50 * 1024 * 1024 || sha256(asset.bytes) != asset.expectedSha256.toLower()) {
    result.error = QStringLiteral("字体文件校验失败");
    return result;
  }
  QDir directory(userFontDirectory());
  if (!directory.mkpath(QStringLiteral("."))) {
    result.error = QStringLiteral("无法创建用户字体目录");
    return result;
  }
  const auto safeId = asset.fontId;
  if (safeId.contains(QStringLiteral("..")) || safeId.contains(QDir::separator()) || safeId.contains(QLatin1Char('/'))) {
    result.error = QStringLiteral("字体标识包含非法路径");
    return result;
  }
  const auto path = directory.filePath(safeId + QStringLiteral(".") + extension);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    result.error = QStringLiteral("无法写入用户字体目录");
    return result;
  }
  if (file.write(asset.bytes) != asset.bytes.size() || !file.flush()) {
    file.close();
    result.error = QStringLiteral("字体写入不完整");
    return result;
  }
  file.close();
  result.ok = true;
  result.installedPath = path;
  return result;
}

bool FontInstaller::removeIfUnreferenced(const QString& contentHash) {
  if (contentHash.isEmpty()) return false;
  QDir directory(userFontDirectory());
  for (const auto& fileName : directory.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf"), QStringLiteral("*.woff"), QStringLiteral("*.woff2")}, QDir::Files)) {
    const auto path = directory.filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) continue;
    const auto hash = sha256(file.readAll());
    file.close();
    if (hash == contentHash.toUtf8().toLower()) return file.remove();
  }
  return false;
}

}  // namespace edward::resources
