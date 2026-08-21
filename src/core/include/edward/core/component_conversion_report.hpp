#pragma once

#include <QJsonObject>
#include <QVector>
#include <QString>

namespace edward::core {

struct UnsupportedProperty final {
  QString nodeId;
  QString field;
  QString reason;
};

struct ComponentConversionReport final {
  QVector<UnsupportedProperty> unsupported;
  [[nodiscard]] bool complete() const { return unsupported.isEmpty(); }
  [[nodiscard]] QJsonObject toJson() const;
};

}  // namespace edward::core
