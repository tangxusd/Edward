#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace edward::resolve {

struct ResolveBridgeConfig final {
  QString host;
  quint16 port = 0;
  QString token;

  static std::optional<ResolveBridgeConfig> fromJson(const QJsonObject& object,
                                                     QString* error = nullptr);
};

}  // namespace edward::resolve
