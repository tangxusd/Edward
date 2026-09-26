#include <QFile>

#include <cassert>

namespace {
QString readSource(const QString& relativePath) {
  QFile source(QStringLiteral(EDWARD_SOURCE_DIR) + QLatin1Char('/') + relativePath);
  assert(source.open(QIODevice::ReadOnly));
  return QString::fromUtf8(source.readAll());
}
}  // namespace

int main() {
  const auto qml = readSource(QStringLiteral("src/desktop/qml/Workbench.qml"));
  assert(!qml.contains(QStringLiteral("ToolBar {")));
  assert(qml.contains(QStringLiteral("function handleNativeTitlebarAction(action)")));
  for (const auto& selector : {"btnAuth", "btnLayoutReset", "btnSettings", "timelineExportPanel", "data-track-size"}) {
    assert(qml.contains(QLatin1String(selector)));
  }
  assert(qml.contains(QStringLiteral("language-en")));
  assert(qml.contains(QStringLiteral("language-zh")));

  const auto titlebar = readSource(QStringLiteral("src/desktop/src/native_titlebar_macos.mm"));
  assert(titlebar.contains(QStringLiteral("language-en")));
  assert(titlebar.contains(QStringLiteral("language-zh")));
  return 0;
}
