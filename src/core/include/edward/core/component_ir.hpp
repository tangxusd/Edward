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

class ComponentIr final {
 public:
  static std::optional<ComponentIr> parse(const QJsonObject& object);

  [[nodiscard]] bool validate(QString* error = nullptr) const;
  [[nodiscard]] QJsonObject toJson() const;
  [[nodiscard]] const QString& version() const { return version_; }
  [[nodiscard]] const ComponentNode& root() const { return root_; }

 private:
  ComponentIr(QString version, ComponentNode root) : version_(std::move(version)), root_(std::move(root)) {}

  QString version_;
  ComponentNode root_;
};

}  // namespace edward::core
