#include "edward/resolve/resolve_adapter.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTcpServer>

#include <cassert>

namespace edward::resolve::test {
QTcpServer* createFixtureServer(QObject* parent);
}

namespace {
edward::core::ComponentIr makeSubtitle() {
  const QJsonObject text{{"id", "cue-1"}, {"type", "text"},
                         {"properties", QJsonObject{{"text", "你好"}, {"startFrame", 0}, {"endFrame", 60}, {"fontFamily", "System"}, {"fontSize", 48}, {"color", "#ffffff"}}},
                         {"transform", QJsonObject{{"x", 12}, {"y", -24}, {"width", 600}, {"height", 80}}}};
  const auto root = QJsonObject{{"id", "subtitle-root"}, {"type", "container"}, {"children", QJsonArray{text}}};
  const auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(parsed.has_value());
  return *parsed;
}

edward::core::ComponentIr makeInvalidSubtitle() {
  const QJsonObject text{{"id", "cue-invalid"}, {"type", "text"},
                         {"properties", QJsonObject{{"text", ""}, {"startFrame", 20}, {"endFrame", 10}}}};
  const auto root = QJsonObject{{"id", "subtitle-root"}, {"type", "container"}, {"children", QJsonArray{text}}};
  const auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(parsed.has_value());
  return *parsed;
}
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  auto* server = edward::resolve::test::createFixtureServer(&app);
  edward::resolve::ResolveConnection connection;
  QString error;
  assert(connection.connectToBridge(QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  edward::resolve::ResolveAdapter adapter(connection);
  assert(adapter.attach(&error));
  const auto snapshot = adapter.timelineSnapshot(&error);
  assert(snapshot.has_value());
  auto subtitle = makeSubtitle();
  assert(adapter.insertSubtitleComponent(subtitle, *snapshot, &error));
  assert(adapter.lastInsertedComponentId() == QStringLiteral("subtitle-1"));
  auto invalidSubtitle = makeInvalidSubtitle();
  assert(!adapter.insertSubtitleComponent(invalidSubtitle, *snapshot, &error));
  assert(error == QStringLiteral("subtitle_component_invalid"));
  assert(adapter.setComponentKeyframe(QStringLiteral("subtitle-1"), QStringLiteral("cue-1"),
                                      QStringLiteral("x"), 42, -30.0, &error));
  return 0;
}
