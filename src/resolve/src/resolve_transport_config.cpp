#include "edward/resolve/resolve_transport_config.hpp"

namespace edward::resolve {

std::optional<ResolveBridgeConfig> ResolveBridgeConfig::fromJson(const QJsonObject& object,
                                                                 QString* error) {
  const auto fail = [error](const QString& value) {
    if (error) *error = value;
    return std::optional<ResolveBridgeConfig>{};
  };
  const auto host = object.value(QStringLiteral("host")).toString().trimmed();
  const auto port = object.value(QStringLiteral("port"));
  const auto token = object.value(QStringLiteral("token")).toString();
  const bool loopback = host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost") ||
                        host == QStringLiteral("::1");
  if (!loopback) return fail(QStringLiteral("resolve_bridge_host_not_loopback"));
  if (!port.isDouble() || port.toInt() <= 0 || port.toInt() > 65535)
    return fail(QStringLiteral("resolve_bridge_port_invalid"));
  if (token.size() < 16) return fail(QStringLiteral("resolve_bridge_token_invalid"));

  ResolveBridgeConfig result;
  result.host = host;
  result.port = static_cast<quint16>(port.toInt());
  result.token = token;
  if (error) error->clear();
  return result;
}

}  // namespace edward::resolve
