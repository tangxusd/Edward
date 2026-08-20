#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>
#include <vector>

namespace edward::core {

enum class ComponentNodeType { Container, Text, Shape, Image, Svg };

struct ComponentNode {
  QString id;
  ComponentNodeType type = ComponentNodeType::Container;
  QJsonObject properties;
  QJsonObject transform;
  QJsonObject keyframes;
  std::vector<ComponentNode> children;
};

struct PluginDependency final {
  QString pluginId;
  QString version;
};

class ComponentIr final {
 public:
  static std::optional<ComponentIr> parse(const QJsonObject& object);

  [[nodiscard]] bool validate(QString* error = nullptr) const;
  [[nodiscard]] QJsonObject toJson() const;
  [[nodiscard]] const QString& version() const { return version_; }
  [[nodiscard]] const ComponentNode& root() const { return root_; }
  [[nodiscard]] const std::optional<PluginDependency>& pluginDependency() const { return pluginDependency_; }
  bool setNodeTransformNumber(const QString& nodeId, const QString& field, double value);
  bool setNodeKeyframeNumber(const QString& nodeId, const QString& field, int frame, double value);
  bool setNodeKeyframeValue(const QString& nodeId, const QString& field, int frame, const QJsonValue& value);
  bool setNodeKeyframeEasing(const QString& nodeId, const QString& field, int frame,
                             const QString& easing);
  bool removeNodeKeyframe(const QString& nodeId, const QString& field, int frame);
  bool setNodeProperty(const QString& nodeId, const QString& field, const QJsonValue& value);

 private:
  ComponentIr(QString version, ComponentNode root, std::optional<PluginDependency> dependency)
      : version_(std::move(version)), root_(std::move(root)), pluginDependency_(std::move(dependency)) {}

  QString version_;
  ComponentNode root_;
  std::optional<PluginDependency> pluginDependency_;
};

}  // namespace edward::core
