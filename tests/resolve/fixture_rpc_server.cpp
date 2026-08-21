#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace edward::resolve::test {

QTcpServer* createFixtureServer(QObject* parent) {
  auto* server = new QTcpServer(parent);
  server->listen(QHostAddress::LocalHost, 0);
  QObject::connect(server, &QTcpServer::newConnection, server, [server] {
    auto* socket = server->nextPendingConnection();
    QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, server, pending = QByteArray{}]() mutable {
      pending += socket->readAll();
      const auto newline = pending.indexOf('\n');
      if (newline < 0) return;
      const auto request = QJsonDocument::fromJson(pending.left(newline)).object();
      pending.remove(0, newline + 1);
      const auto method = request.value(QStringLiteral("method")).toString();
      const auto id = request.value(QStringLiteral("id"));
      if (method == QStringLiteral("malformed")) {
        socket->write("{not-json}\n");
        socket->flush();
        return;
      }
      if (method == QStringLiteral("mismatch")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id.toInt() + 1},
                                                 {QStringLiteral("result"), QJsonObject{}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("timeout")) return;
      if (method == QStringLiteral("capabilities")) {
        const auto version = server->property("freeCapabilities").toBool() ? QStringLiteral("free") : QStringLiteral("20.0.0");
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{
                                                   {QStringLiteral("studioVersion"), version == QStringLiteral("free") ? QStringLiteral("free") : QStringLiteral("20.0.0")},
                                                   {QStringLiteral("timeline"), true},
                                                   {QStringLiteral("fusion"), true},
                                                   {QStringLiteral("render"), true}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("timeline.snapshot")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{
                                                   {QStringLiteral("projectName"), QStringLiteral("Demo")},
                                                   {QStringLiteral("timelineName"), QStringLiteral("Timeline 1")},
                                                   {QStringLiteral("playheadFrame"), 42},
                                                   {QStringLiteral("timelineStartFrame"), 0},
                                                   {QStringLiteral("fpsNumerator"), 30},
                                                   {QStringLiteral("fpsDenominator"), 1},
                                                   {QStringLiteral("tracks"), QJsonArray{
                                                     QJsonObject{{QStringLiteral("id"), QStringLiteral("v1")}, {QStringLiteral("name"), QStringLiteral("V1")}, {QStringLiteral("video"), true}, {QStringLiteral("audio"), false}},
                                                     QJsonObject{{QStringLiteral("id"), QStringLiteral("a1")}, {QStringLiteral("name"), QStringLiteral("A1")}, {QStringLiteral("video"), false}, {QStringLiteral("audio"), true}}
                                                   }}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("timeline.setPlayhead")) {
        const auto frame = request.value(QStringLiteral("params")).toObject().value(QStringLiteral("frame")).toInt();
        const auto result = QJsonObject{{QStringLiteral("accepted"), frame >= 0 && frame <= 900}};
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("result"), result}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("subtitle.insert")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("accepted"), true}, {QStringLiteral("componentId"), QStringLiteral("subtitle-1")}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("component.keyframe")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("accepted"), true}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("render.start")) {
        const auto outputPath = request.value(QStringLiteral("params")).toObject().value(QStringLiteral("outputPath")).toString();
        const bool accepted = !outputPath.contains(QStringLiteral("reject"));
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("accepted"), accepted}, {QStringLiteral("jobId"), accepted ? QStringLiteral("render-1") : QString()}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("render.status")) {
        const auto jobId = request.value(QStringLiteral("params")).toObject().value(QStringLiteral("jobId")).toString();
        const bool known = jobId == QStringLiteral("render-1");
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("state"), known ? QStringLiteral("completed") : QStringLiteral("failed")}, {QStringLiteral("progress"), known ? 100 : 0}, {QStringLiteral("error"), known ? QString() : QStringLiteral("render_job_not_found")}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("render.cancel")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("accepted"), true}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("ui.openDeliver")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), QJsonObject{{QStringLiteral("accepted"), true}}}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
        return;
      }
      if (method == QStringLiteral("echo")) {
        socket->write(QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("result"), request.value(QStringLiteral("params"))}})
                          .toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
      }
    });
  });
  return server;
}

}  // namespace edward::resolve::test
