#include "edward/resources/font_installer.hpp"

#include <QCryptographicHash>

#include <cassert>

int main() {
  const QByteArray bytes("font-test");
  edward::resources::FontAsset asset{"orbit-test", "orbit-test.ttf", bytes, edward::resources::FontInstaller::sha256(bytes), "font/ttf"};
  const auto installed = edward::resources::FontInstaller::install(asset);
  assert(installed.ok);
  assert(!installed.installedPath.isEmpty());
  edward::resources::FontAsset invalid = asset;
  invalid.expectedSha256 = QByteArray(64, '0');
  assert(!edward::resources::FontInstaller::install(invalid).ok);
  assert(edward::resources::FontInstaller::removeIfUnreferenced(asset.expectedSha256));
  return 0;
}
