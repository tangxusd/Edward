#include "edward/resolve/resolve_adapter.hpp"

#include <QCoreApplication>
#include <QTcpServer>

#include <cassert>

namespace edward::resolve::test {
QTcpServer* createFixtureServer(QObject* parent);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  auto* server = edward::resolve::test::createFixtureServer(&app);
  assert(server->isListening());

  edward::resolve::ResolveConnection connection;
  QString error;
  assert(connection.connectToBridge(
      QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  edward::resolve::ResolveAdapter adapter(connection);
  assert(adapter.attach(&error));

  const auto capabilities = adapter.capabilities(&error);
  assert(capabilities.has_value());
  assert(capabilities->studioVersion == QStringLiteral("20.0.0"));
  assert(capabilities->timeline && capabilities->fusion && capabilities->render);

  const auto snapshot = adapter.timelineSnapshot(&error);
  assert(snapshot.has_value());
  assert(snapshot->projectName == QStringLiteral("Demo"));
  assert(snapshot->timelineName == QStringLiteral("Timeline 1"));
  assert(snapshot->playheadFrame == 42);
  assert(snapshot->fpsNumerator == 30 && snapshot->fpsDenominator == 1);
  assert(snapshot->tracks.size() == 2);
  assert(snapshot->tracks.at(0).video && !snapshot->tracks.at(0).audio);
  assert(!snapshot->tracks.at(1).video && snapshot->tracks.at(1).audio);

  assert(adapter.setPlayhead(100, &error));
  assert(!adapter.setPlayhead(-1, &error));
  assert(error == QStringLiteral("playhead_out_of_range"));

  edward::resolve::ResolveConnection freeConnection;
  server->setProperty("freeCapabilities", true);
  assert(freeConnection.connectToBridge(
      QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  edward::resolve::ResolveAdapter freeAdapter(freeConnection);
  const auto freeCapabilities = freeAdapter.capabilities(&error);
  assert(!freeCapabilities.has_value());
  assert(error == QStringLiteral("resolve_studio_required"));
  return 0;
}
