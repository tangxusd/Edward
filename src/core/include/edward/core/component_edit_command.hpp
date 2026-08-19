#pragma once

#include "edward/core/component_ir.hpp"

namespace edward::core {

enum class ComponentEditKind { SetTransformNumber, SetProperty, SetKeyframeValue };

struct ComponentEditCommand final {
  ComponentEditKind kind = ComponentEditKind::SetProperty;
  QString nodeId;
  QString field;
  int frame = 0;
  QJsonValue value;

  static bool apply(ComponentIr& component, const ComponentEditCommand& command,
                    QString* error = nullptr);
};

}  // namespace edward::core
