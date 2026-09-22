#include <QFile>

#include <cassert>

int main() {
  QFile source(QStringLiteral(EDWARD_SOURCE_DIR "/src/desktop/qml/Workbench.qml"));
  assert(source.open(QIODevice::ReadOnly));
  const auto qml = QString::fromUtf8(source.readAll());
  assert(qml.contains(QStringLiteral("url: fablecutServerUrl")));
  assert(qml.contains(QStringLiteral("!workbenchRuntime.nativeRuntimeSelected")));
  assert(qml.contains(QStringLiteral("WebChannel {")));
  assert(qml.contains(QStringLiteral("preferenceWebChannel.registerObjects")) ||
         qml.contains(QStringLiteral("registerObjects({")));
  QFile mainSource(QStringLiteral(EDWARD_SOURCE_DIR "/src/desktop/src/main.cpp"));
  assert(mainSource.open(QIODevice::ReadOnly));
  const auto sourceText = QString::fromUtf8(mainSource.readAll());
  assert(sourceText.contains(QStringLiteral("qrc:///qtwebchannel/qwebchannel.js")));
  assert(sourceText.contains(QStringLiteral("QWebEngineScript::DocumentCreation")));
  assert(sourceText.contains(QStringLiteral("QProcess::MergedChannels")));
  assert(sourceText.contains(QStringLiteral("recordFablecutDiagnostic")));
  assert(sourceText.contains(QStringLiteral("setPersistentStoragePath")));
  assert(sourceText.contains(QStringLiteral("ForcePersistentCookies")));
  assert(sourceText.contains(QStringLiteral("-tiTCP")));
  assert(sourceText.contains(QStringLiteral("portProbe.listen(QHostAddress::LocalHost, fablecutPort)")));
  assert(sourceText.contains(QStringLiteral("server_startup_")));
  return 0;
}
