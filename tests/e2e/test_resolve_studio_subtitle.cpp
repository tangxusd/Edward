#include "edward/resolve/resolve_adapter.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QThread>
#include <QUrl>

#include <cassert>

namespace {
edward::core::ComponentIr makeSubtitle() {
  const QJsonObject text{{"id", "cue-e2e"}, {"type", "text"},
                         {"properties", QJsonObject{{"text", "Edward E2E"}, {"startFrame", 0}, {"endFrame", 60}, {"fontFamily", "System"}, {"fontSize", 48}, {"color", "#ffffff"}}}};
  const auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", QJsonObject{{"id", "subtitle-root"}, {"type", "container"}, {"children", QJsonArray{text}}}}});
  assert(parsed.has_value());
  return *parsed;
}
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (qEnvironmentVariable("EDWARD_RESOLVE_E2E") != QStringLiteral("1")) {
    qInfo("SKIP: set EDWARD_RESOLVE_E2E=1 to run against Resolve Studio");
    return 0;
  }
  const auto bridgeUrl = qEnvironmentVariable("EDWARD_RESOLVE_BRIDGE_URL");
  if (bridgeUrl.isEmpty()) {
    qInfo("SKIP: EDWARD_RESOLVE_BRIDGE_URL is not configured");
    return 0;
  }

  edward::resolve::ResolveConnection connection(2000);
  QString error;
  if (!connection.connectToBridge(QUrl(bridgeUrl), &error)) {
    qInfo().noquote() << "SKIP: Resolve Studio bridge unavailable:" << error;
    return 0;
  }
  edward::resolve::ResolveAdapter adapter(connection);
  assert(adapter.attach(&error));
  const auto snapshot = adapter.timelineSnapshot(&error);
  assert(snapshot.has_value());
  const auto subtitle = makeSubtitle();
  assert(adapter.insertSubtitleComponent(subtitle, *snapshot, &error));
  assert(adapter.setComponentKeyframe(adapter.lastInsertedComponentId(), QStringLiteral("cue-e2e"),
                                      QStringLiteral("x"), snapshot->playheadFrame, 0.0, &error));

  edward::resolve::ResolveRenderOptions options;
  options.outputPath = qEnvironmentVariable("EDWARD_RESOLVE_E2E_OUTPUT", QStringLiteral("resolve-e2e.mp4"));
  options.width = 1920;
  options.height = 1080;
  options.fps = snapshot->fpsNumerator / snapshot->fpsDenominator;
  options.codec = QStringLiteral("h264");
  options.quality = 80;
  QString jobId;
  assert(adapter.queueAndStartRender(options, &jobId, &error));
  for (int attempt = 0; attempt < 120; ++attempt) {
    const auto status = adapter.renderStatus(jobId, &error);
    if (status.state == edward::resolve::ResolveRenderState::Completed) return 0;
    assert(status.state != edward::resolve::ResolveRenderState::Failed);
    QThread::msleep(250);
    QCoreApplication::processEvents();
  }
  assert(false && "Resolve render did not complete within 30 seconds");
}
