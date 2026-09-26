#pragma once

#include <QString>

namespace edward::resources {

struct DeviceIdentity final {
  QString serial;
  QString mac;
  QString error;

  [[nodiscard]] bool valid() const { return !serial.isEmpty() && !mac.isEmpty() && error.isEmpty(); }
};

DeviceIdentity normalizeDeviceIdentity(const QString& serial, const QString& mac);
DeviceIdentity collectDeviceIdentity();

}  // namespace edward::resources
