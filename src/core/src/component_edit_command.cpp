#include "edward/core/component_edit_command.hpp"

namespace edward::core {

bool ComponentEditCommand::apply(ComponentIr& component, const ComponentEditCommand& command,
                                 QString* error) {
  if (command.nodeId.isEmpty() || command.field.isEmpty()) {
    if (error) *error = QStringLiteral("component edit target is required");
    return false;
  }
  bool applied = false;
  switch (command.kind) {
    case ComponentEditKind::SetTransformNumber:
      applied = command.value.isDouble() &&
                component.setNodeTransformNumber(command.nodeId, command.field, command.value.toDouble());
      break;
    case ComponentEditKind::SetProperty:
      applied = component.setNodeProperty(command.nodeId, command.field, command.value);
      break;
    case ComponentEditKind::SetKeyframeValue:
      applied = component.setNodeKeyframeValue(command.nodeId, command.field, command.frame, command.value);
      break;
  }
  if (!applied && error) *error = QStringLiteral("component edit is invalid");
  return applied;
}

}  // namespace edward::core
