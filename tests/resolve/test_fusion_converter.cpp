#include "edward/resolve/fusion_converter.hpp"

#include <QJsonArray>

#include <cassert>

namespace {
edward::core::ComponentIr makeComponent(bool unsupported) {
  QJsonObject properties{{"text", "Edward"}, {"fontSize", 32}, {"color", "#ffffff"}};
  if (unsupported) properties.insert("backdropFilter", "blur(4px)");
  const QJsonObject text{{"id", "title"}, {"type", "text"}, {"properties", properties}};
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{text}}};
  const auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(parsed.has_value());
  return *parsed;
}
}

int main() {
  const edward::resolve::ResolveCapabilities capabilities{true, true, true, QStringLiteral("20.0.0")};
  const auto supported = edward::resolve::convertComponentToFusion(makeComponent(false), capabilities);
  assert(supported.report.complete());
  assert(!supported.transparentVideoFallbackAllowed);
  assert(supported.fusionPayload.value(QStringLiteral("backend")).toString() == QStringLiteral("fusion"));

  const auto unsupported = edward::resolve::convertComponentToFusion(makeComponent(true), capabilities);
  assert(!unsupported.report.complete());
  assert(unsupported.transparentVideoFallbackAllowed);
  assert(unsupported.report.unsupported.at(0).field == QStringLiteral("backdropFilter"));

  const edward::resolve::ResolveCapabilities noFusion{true, false, true, QStringLiteral("20.0.0")};
  assert(edward::resolve::convertComponentToFusion(makeComponent(false), noFusion).transparentVideoFallbackAllowed);
  return 0;
}
