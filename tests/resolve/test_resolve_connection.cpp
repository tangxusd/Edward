#include "edward/resolve/resolve_connection.hpp"

#include <QCoreApplication>
#include <QJsonObject>
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
  assert(connection.state() == edward::resolve::ConnectionState::Connected);

  edward::resolve::ResolveError rpcError;
  const auto response = connection.call(QStringLiteral("echo"),
                                        QJsonObject{{QStringLiteral("value"), 42}}, &rpcError);
  assert(response.has_value());
  assert(response->value(QStringLiteral("result")).toObject().value(QStringLiteral("value")) == 42);
  assert(rpcError.code.isEmpty());

  const auto malformed = connection.call(QStringLiteral("malformed"), {}, &rpcError);
  assert(!malformed.has_value());
  assert(rpcError.code == QStringLiteral("bridge_response_invalid"));

  assert(connection.connectToBridge(
      QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  const auto mismatch = connection.call(QStringLiteral("mismatch"), {}, &rpcError);
  assert(!mismatch.has_value());
  assert(rpcError.code == QStringLiteral("bridge_response_id_mismatch"));

  assert(connection.connectToBridge(
      QUrl(QStringLiteral("tcp://127.0.0.1:%1").arg(server->serverPort())), &error));
  const auto timeout = connection.call(QStringLiteral("timeout"), {}, &rpcError);
  assert(!timeout.has_value());
  assert(rpcError.code == QStringLiteral("bridge_response_timeout"));

  connection.disconnect();
  assert(connection.state() == edward::resolve::ConnectionState::Disconnected);
  return 0;
}
