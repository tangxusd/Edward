#include "edward/resolve/resolve_transport_config.hpp"

#include <QCoreApplication>
#include <QJsonObject>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QString error;
  const auto config = edward::resolve::ResolveBridgeConfig::fromJson(
      QJsonObject{{"host", "127.0.0.1"}, {"port", 49632}, {"token", "0123456789abcdef"}}, &error);
  assert(config.has_value());
  assert(config->port == 49632);
  assert(error.isEmpty());

  assert(!edward::resolve::ResolveBridgeConfig::fromJson(
      QJsonObject{{"host", "0.0.0.0"}, {"port", 49632}, {"token", "0123456789abcdef"}}, &error));
  assert(error == QStringLiteral("resolve_bridge_host_not_loopback"));
  assert(!edward::resolve::ResolveBridgeConfig::fromJson(
      QJsonObject{{"host", "127.0.0.1"}, {"port", 49632}, {"token", "short"}}, &error));
  assert(error == QStringLiteral("resolve_bridge_token_invalid"));
  return 0;
}
