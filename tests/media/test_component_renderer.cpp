#include <edward/core/component_ir.hpp>
#include <edward/media/component_renderer.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <cassert>

int main() {
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "box"}, {"type", "shape"},
                   {"transform", QJsonObject{{"x", 10}, {"y", 10}, {"width", 40}, {"height", 40}}},
                   {"properties", QJsonObject{{"fill", "#00ffff"}}},
                   {"keyframes", QJsonObject{{"x", QJsonArray{QJsonObject{{"frame", 0}, {"value", 10}, {"easing", "bezier"}, {"controlOut", 0}}, QJsonObject{{"frame", 10}, {"value", 30}, {"controlIn", 30}}}}}}},
  }}};
  const auto component = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(component);
  const auto image = edward::media::ComponentRenderer{}.render(*component, 5, {100, 100});
  assert(!image.isNull());
  assert(image.pixelColor(16, 20).alpha() > 0);
  assert(image.pixelColor(5, 5).alpha() == 0);
  return 0;
}
