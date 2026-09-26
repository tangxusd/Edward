#include "edward/resources/device_identity.hpp"

#include <QRegularExpression>

namespace edward::resources {

DeviceIdentity normalizeDeviceIdentity(const QString& serial, const QString& mac) {
  DeviceIdentity identity;
  identity.serial = serial.trimmed().toUpper();
  identity.mac = mac.trimmed().toUpper().replace(QLatin1Char('-'), QLatin1Char(':'));
  static const QRegularExpression macPattern(QStringLiteral("^[0-9A-F]{2}(:[0-9A-F]{2}){5}$"));
  if (identity.serial.isEmpty() || identity.serial.size() > 256 || !macPattern.match(identity.mac).hasMatch()) {
    identity = {};
    identity.error = QStringLiteral("无法读取设备标识，请检查系统权限后重试。");
  }
  return identity;
}

#ifndef Q_OS_MACOS
DeviceIdentity collectDeviceIdentity() {
  return {.error = QStringLiteral("无法读取设备标识，请检查系统权限后重试。")} ;
}
#endif

}  // namespace edward::resources
