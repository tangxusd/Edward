#include <QFile>

#include <cassert>

int main() {
  QFile source(QStringLiteral(EDWARD_SOURCE_DIR "/src/desktop/src/main.cpp"));
  assert(source.open(QIODevice::ReadOnly));
  const auto mainSource = QString::fromUtf8(source.readAll());

  assert(mainSource.contains(QStringLiteral("QStringLiteral(\"cwd\")")));
  assert(mainSource.contains(QStringLiteral("QStringLiteral(\"-Fn\")")));
  assert(mainSource.contains(QStringLiteral("canonicalFilePath() == expectedCwd")));
  assert(!mainSource.contains(QStringLiteral("command.contains(fablecutEntry)")));
  assert(mainSource.contains(QStringLiteral("QTcpServer portProbe")));
  assert(mainSource.contains(QStringLiteral("QStringLiteral(\"PORT\")")));
  assert(mainSource.contains(QStringLiteral("fablecutServerUrl")));
  assert(mainSource.contains(QStringLiteral("fablecutServer.waitForFinished(300)")));
  return 0;
}
