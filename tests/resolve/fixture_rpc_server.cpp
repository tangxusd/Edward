#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace edward::resolve::test {

QTcpServer* createFixtureServer(QObject* parent) {
  auto* server = new QTcpServer(parent);
  server->listen(QHostAddress::LocalHost, 0);
  QObject::connect(server, &QTcpServer::newConnection, server, [server] {
    auto* socket = server->nextPendingConnection();
    QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
      const auto request = QJsonDocument::fromJson(socket->readLine()).object();
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
