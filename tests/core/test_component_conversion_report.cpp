#include "edward/core/component_conversion_report.hpp"

#include <QJsonArray>

#include <cassert>

int main() {
  edward::core::ComponentConversionReport report;
  assert(report.complete());
  report.unsupported.push_back({QStringLiteral("title"), QStringLiteral("backdropFilter"), QStringLiteral("unsupported")});
  assert(!report.complete());
  assert(report.toJson().value(QStringLiteral("unsupported")).toArray().size() == 1);
  return 0;
}
