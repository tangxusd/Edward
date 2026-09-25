#pragma once

#include <QByteArray>
#include <QString>

namespace edward::resources {

struct FontAsset {
  QString fontId;
  QString fileName;
  QByteArray bytes;
  QByteArray expectedSha256;
  QString mimeType;
};

struct InstallResult {
  bool ok = false;
  QString installedPath;
  QString error;
};

class FontInstaller final {
 public:
  static QString userFontDirectory();
  static InstallResult install(const FontAsset& asset);
  static bool removeIfUnreferenced(const QString& contentHash);
  static QByteArray sha256(const QByteArray& bytes);
};

}  // namespace edward::resources
