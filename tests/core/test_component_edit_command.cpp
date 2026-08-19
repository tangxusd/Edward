#include <edward/core/component_edit_command.hpp>

#include <QJsonObject>
#include <QJsonArray>
#include <cassert>

int main() {
  const QJsonObject root{{"id", "root"}, {"type", "text"},
                         {"properties", QJsonObject{{"text", "Before"}}}};
  auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(component);
  QString error;
  assert(edward::core::ComponentEditCommand::apply(
      *component, {edward::core::ComponentEditKind::SetProperty, "root", "text", 0, "After"}, &error));
  assert(component->root().properties.value("text").toString() == "After");
  assert(component->root().keyframes.value("text").toArray().isEmpty());
  assert(edward::core::ComponentEditCommand::apply(
      *component, {edward::core::ComponentEditKind::SetKeyframeValue, "root", "text", 12, "Animated"}, &error));
  assert(!edward::core::ComponentEditCommand::apply(
      *component, {edward::core::ComponentEditKind::SetTransformNumber, "root", "x", 0, "not-a-number"}, &error));
  assert(!error.isEmpty());
  assert(!edward::core::ComponentEditCommand::apply(
      *component, {edward::core::ComponentEditKind::SetProperty, "missing", "text", 0, "bad"}, &error));
  return 0;
}
