#include "edward/resolve/fusion_converter.hpp"

#include <QJsonArray>

namespace edward::resolve {
namespace {

void appendNode(const QJsonObject& node, QJsonArray& output,
                edward::core::ComponentConversionReport& report) {
  const auto id = node.value(QStringLiteral("id")).toString();
  const auto type = node.value(QStringLiteral("type")).toString();
  const auto properties = node.value(QStringLiteral("properties")).toObject();
  static const QStringList supportedProperties{
      QStringLiteral("text"), QStringLiteral("fontFamily"), QStringLiteral("fontSize"),
      QStringLiteral("color"), QStringLiteral("fill"), QStringLiteral("stroke"),
      QStringLiteral("strokeWidth"), QStringLiteral("opacity"), QStringLiteral("startFrame"),
      QStringLiteral("endFrame")};
  for (auto iterator = properties.constBegin(); iterator != properties.constEnd(); ++iterator) {
    if (!supportedProperties.contains(iterator.key())) {
      report.unsupported.push_back({id, iterator.key(), QStringLiteral("Fusion 标准转换器不支持该属性")});
    }
  }
  output.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), type},
                            {QStringLiteral("transform"), node.value(QStringLiteral("transform"))},
                            {QStringLiteral("properties"), properties},
                            {QStringLiteral("keyframes"), node.value(QStringLiteral("keyframes"))}});
  for (const auto& child : node.value(QStringLiteral("children")).toArray()) {
    if (child.isObject()) appendNode(child.toObject(), output, report);
  }
}

}  // namespace

FusionConversionResult convertComponentToFusion(const edward::core::ComponentIr& component,
                                                const ResolveCapabilities& capabilities) {
  FusionConversionResult result;
  if (!capabilities.fusion) {
    result.report.unsupported.push_back({QStringLiteral("root"), QStringLiteral("fusion"),
                                         QStringLiteral("Resolve 当前不具备 Fusion 能力")});
    result.transparentVideoFallbackAllowed = true;
    return result;
  }
  QJsonArray nodes;
  appendNode(component.toJson().value(QStringLiteral("root")).toObject(), nodes, result.report);
  result.fusionPayload = QJsonObject{{QStringLiteral("backend"), QStringLiteral("fusion")},
                                     {QStringLiteral("component"), component.toJson()},
                                     {QStringLiteral("nodes"), nodes}};
  result.transparentVideoFallbackAllowed = !result.report.complete();
  return result;
}

}  // namespace edward::resolve
