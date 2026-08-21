#include "edward/core/component_conversion_report.hpp"

#include <QJsonArray>

namespace edward::core {

QJsonObject ComponentConversionReport::toJson() const {
  QJsonArray entries;
  for (const auto& item : unsupported) {
    entries.append(QJsonObject{{QStringLiteral("nodeId"), item.nodeId},
                               {QStringLiteral("field"), item.field},
                               {QStringLiteral("reason"), item.reason}});
  }
  return QJsonObject{{QStringLiteral("complete"), complete()},
                     {QStringLiteral("unsupported"), entries}};
}

}  // namespace edward::core
