#include <edward/core/component_edit_command_parser.hpp>

#include <QJsonArray>
#include <cassert>

int main() {
  QString error;
  const auto transform = edward::core::parseComponentEditCommand(
      {{"operation", "setTransformNumber"}, {"nodeId", "card"}, {"field", "x"}, {"value", 24}}, &error);
  assert(transform && transform->kind == edward::core::ComponentEditKind::SetTransformNumber);
  assert(transform->value.toDouble() == 24.0);
  const auto property = edward::core::parseComponentEditCommand(
      {{"operation", "setProperty"}, {"nodeId", "title"}, {"field", "text"}, {"value", "Edward"}}, &error);
  assert(property && property->value.toString() == "Edward");
  const auto keyframe = edward::core::parseComponentEditCommand(
      {{"operation", "setKeyframeValue"}, {"nodeId", "title"}, {"field", "text"},
       {"frame", 12}, {"value", "Animated"}}, &error);
  assert(keyframe && keyframe->frame == 12);
  assert(!edward::core::parseComponentEditCommand(
      {{"operation", "setTransformNumber"}, {"nodeId", "card"}, {"field", "x"}, {"value", "24"}}, &error));
  assert(!edward::core::parseComponentEditCommand(
      {{"operation", "setProperty"}, {"nodeId", "card"}, {"field", "style"}, {"value", QJsonArray{1}}}, &error));
  assert(!edward::core::parseComponentEditCommand(
      {{"operation", "setKeyframeValue"}, {"nodeId", "card"}, {"field", "x"}, {"frame", -1}, {"value", 1}}, &error));
  assert(!edward::core::parseComponentEditCommand(
      {{"operation", "eval"}, {"nodeId", "card"}, {"field", "x"}, {"value", "alert(1)"}}, &error));
  return 0;
}
