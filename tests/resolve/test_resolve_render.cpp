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
  edward::resolve::ResolveConnection connection;
  QString error;
  assert(connection.connectToBridge(QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  edward::resolve::ResolveAdapter adapter(connection);
  assert(adapter.attach(&error));

  edward::resolve::ResolveRenderOptions options;
  options.outputPath = QStringLiteral("/tmp/edward-render.mp4");
  options.width = 1920;
  options.height = 1080;
  options.fps = 30;
  options.codec = QStringLiteral("h264");
  options.quality = 80;
  QString jobId;
  assert(adapter.queueAndStartRender(options, &jobId, &error));
  assert(jobId == QStringLiteral("render-1"));
  const auto completed = adapter.renderStatus(jobId, &error);
  assert(completed.state == edward::resolve::ResolveRenderState::Completed);
  assert(completed.progress == 100);
  assert(adapter.cancelRender(jobId, &error));

  options.width = 0;
  assert(!adapter.queueAndStartRender(options, &jobId, &error));
  assert(error == QStringLiteral("resolve_render_options_invalid"));
  options.width = 1920;
  options.outputPath = QStringLiteral("/tmp/reject.mp4");
  assert(!adapter.queueAndStartRender(options, &jobId, &error));
  assert(error == QStringLiteral("resolve_render_rejected"));

  const auto missing = adapter.renderStatus(QStringLiteral("missing"), &error);
  assert(missing.state == edward::resolve::ResolveRenderState::Failed);
  assert(error.isEmpty());
  return 0;
}
