#include "edward/core/component_ir.hpp"

#include <QJsonArray>
#include <QJsonValue>

#include <utility>

namespace edward::core {
namespace {

std::optional<ComponentNodeType> parseType(const QString& value) {
  if (value == "container") return ComponentNodeType::Container;
  if (value == "text") return ComponentNodeType::Text;
  if (value == "shape") return ComponentNodeType::Shape;
  if (value == "image") return ComponentNodeType::Image;
  if (value == "svg") return ComponentNodeType::Svg;
  return std::nullopt;
}

QString typeName(ComponentNodeType type) {
  switch (type) {
    case ComponentNodeType::Container: return "container";
    case ComponentNodeType::Text: return "text";
    case ComponentNodeType::Shape: return "shape";
    case ComponentNodeType::Image: return "image";
    case ComponentNodeType::Svg: return "svg";
  }
  return {};
}

std::optional<QJsonObject> normalizeKeyframes(const QJsonValue& value) {
  if (value.isUndefined()) return QJsonObject{};
  if (!value.isObject()) return std::nullopt;
  QJsonObject normalized;
  const auto source = value.toObject();
  for (auto iterator = source.constBegin(); iterator != source.constEnd(); ++iterator) {
    if (!iterator.value().isArray()) return std::nullopt;
    std::vector<QJsonValue> points;
    for (const auto& pointValue : iterator.value().toArray()) {
      if (!pointValue.isObject()) return std::nullopt;
      const auto point = pointValue.toObject();
      if (!point.value("frame").isDouble() ||
          (!point.value("value").isDouble() && !point.value("value").isString() &&
           !point.value("value").isBool()) ||
          point.value("frame").toInt() < 0) return std::nullopt;
      const auto easing = point.value("easing").toString();
      if (!easing.isEmpty() && easing != "linear" && easing != "bezier") return std::nullopt;
      if ((!point.value("controlIn").isUndefined() && !point.value("controlIn").isDouble()) ||
          (!point.value("controlOut").isUndefined() && !point.value("controlOut").isDouble())) return std::nullopt;
      points.push_back(point);
    }
    std::sort(points.begin(), points.end(), [](const QJsonValue& left, const QJsonValue& right) {
      return left.toObject().value("frame").toInt() < right.toObject().value("frame").toInt();
    });
    QJsonArray sorted;
    int previousFrame = -1;
    for (const auto& point : points) {
      const int frame = point.toObject().value("frame").toInt();
      if (frame == previousFrame) return std::nullopt;
      sorted.append(point);
      previousFrame = frame;
    }
    normalized.insert(iterator.key(), sorted);
  }
  return normalized;
}

std::optional<ComponentNode> parseNode(const QJsonObject& object) {
  const auto id = object.value("id").toString();
  const auto type = parseType(object.value("type").toString());
  if (id.isEmpty() || !type) return std::nullopt;

  ComponentNode node;
  node.id = id;
  node.type = *type;
  node.properties = object.value("properties").toObject();
  node.transform = object.value("transform").toObject();
  const auto keyframes = normalizeKeyframes(object.value("keyframes"));
  if (!keyframes) return std::nullopt;
  node.keyframes = *keyframes;

  const auto children = object.value("children");
  if (!children.isUndefined() && !children.isArray()) return std::nullopt;
  for (const auto& child : children.toArray()) {
    const auto parsed = parseNode(child.toObject());
    if (!parsed) return std::nullopt;
    node.children.push_back(*parsed);
  }
  return node;
}

std::optional<PluginDependency> parseDependency(const QJsonValue& value) {
  if (value.isUndefined()) return std::nullopt;
  if (!value.isObject()) return PluginDependency{};
  const auto object = value.toObject();
  const auto pluginId = object.value("pluginId").toString();
  const auto version = object.value("version").toString();
  if (pluginId.isEmpty() || version.isEmpty()) return PluginDependency{};
  return PluginDependency{pluginId, version};
}

QJsonObject nodeToJson(const ComponentNode& node) {
  QJsonArray children;
  for (const auto& child : node.children) children.append(nodeToJson(child));
  return {{"id", node.id}, {"type", typeName(node.type)}, {"properties", node.properties},
          {"transform", node.transform}, {"keyframes", node.keyframes}, {"children", children}};
}

bool validateNode(const ComponentNode& node, QString* error, std::vector<QString>& ids) {
  if (node.id.isEmpty()) {
    if (error) *error = "node id is required";
    return false;
  }
  if (std::find(ids.begin(), ids.end(), node.id) != ids.end()) {
    if (error) *error = "node id must be unique";
    return false;
  }
  ids.push_back(node.id);
  if (!node.transform.isEmpty() && !node.transform.value("position").isUndefined() &&
      !node.transform.value("position").isObject()) {
    if (error) *error = "transform.position must be an object";
    return false;
  }
  for (const auto& child : node.children)
    if (!validateNode(child, error, ids)) return false;
  return true;
}

ComponentNode* findNode(ComponentNode& node, const QString& nodeId) {
  if (node.id == nodeId) return &node;
  for (auto& child : node.children) {
    if (auto* found = findNode(child, nodeId)) return found;
  }
  return nullptr;
}

}  // namespace

std::optional<ComponentIr> ComponentIr::parse(const QJsonObject& object) {
  const auto version = object.value("version").toString();
  const auto root = parseNode(object.value("root").toObject());
  if (version.isEmpty() || !root) return std::nullopt;
  const auto dependency = parseDependency(object.value("pluginDependency"));
  if (!object.value("pluginDependency").isUndefined() &&
      (!dependency || dependency->pluginId.isEmpty())) return std::nullopt;
  ComponentIr ir(version, *root, dependency);
  return ir.validate() ? std::optional<ComponentIr>(std::move(ir)) : std::nullopt;
}

bool ComponentIr::validate(QString* error) const {
  if (version_ != "1") {
    if (error) *error = "unsupported component ir version";
    return false;
  }
  std::vector<QString> ids;
  return validateNode(root_, error, ids);
}

QJsonObject ComponentIr::toJson() const {
  QJsonObject object{{"version", version_}, {"root", nodeToJson(root_)}};
  if (pluginDependency_) object.insert("pluginDependency", QJsonObject{{"pluginId", pluginDependency_->pluginId}, {"version", pluginDependency_->version}});
  return object;
}

bool ComponentIr::setNodeTransformNumber(const QString& nodeId, const QString& field, double value) {
  auto* node = findNode(root_, nodeId);
  if (!node || field.isEmpty()) return false;
  node->transform.insert(field, value);
  return true;
}

bool ComponentIr::setNodeKeyframeNumber(const QString& nodeId, const QString& field, int frame, double value) {
  return setNodeKeyframeValue(nodeId, field, frame, value);
}

bool ComponentIr::setNodeKeyframeValue(const QString& nodeId, const QString& field, int frame, const QJsonValue& value) {
  auto* node = findNode(root_, nodeId);
  if (!node || field.isEmpty() || frame < 0 ||
      (!value.isDouble() && !value.isString() && !value.isBool())) return false;

  auto keyframes = node->keyframes.value(field).toArray();
  bool replaced = false;
  for (auto index = 0; index < keyframes.size(); ++index) {
    const auto point = keyframes.at(index).toObject();
    if (point.value("frame").toInt(-1) == frame) {
      auto updated = point;
      updated.insert("frame", frame);
      updated.insert("value", value);
      keyframes.replace(index, updated);
      replaced = true;
      break;
    }
  }
  if (!replaced) keyframes.append(QJsonObject{{"frame", frame}, {"value", value}});
  std::vector<QJsonValue> ordered;
  ordered.reserve(keyframes.size());
  for (const auto& keyframe : keyframes) ordered.push_back(keyframe);
  std::sort(ordered.begin(), ordered.end(), [](const QJsonValue& left, const QJsonValue& right) {
    return left.toObject().value("frame").toInt() < right.toObject().value("frame").toInt();
  });
  QJsonArray sorted;
  for (const auto& keyframe : ordered) sorted.append(keyframe);
  node->keyframes.insert(field, sorted);
  return true;
}

bool ComponentIr::setNodeProperty(const QString& nodeId, const QString& field, const QJsonValue& value) {
  auto* node = findNode(root_, nodeId);
  if (!node || field.isEmpty()) return false;
  node->properties.insert(field, value);
  return true;
}

}  // namespace edward::core
