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
  assert(image.pixelColor(34, 50).alpha() > 0);
  assert(image.pixelColor(5, 5).alpha() == 0);

  const QJsonObject nestedRoot{{"id", "root"}, {"type", "container"},
      {"transform", QJsonObject{{"x", 10}, {"y", 0}}},
      {"properties", QJsonObject{{"opacity", 0.5}}}, {"children", QJsonArray{
      QJsonObject{{"id", "child"}, {"type", "shape"},
          {"transform", QJsonObject{{"x", 5}, {"y", 0}, {"width", 10}, {"height", 10}}},
          {"properties", QJsonObject{{"fill", "#ff0000"}}}}
  }}};
  const auto nested = edward::core::ComponentIr::parse({{"version", "1"}, {"root", nestedRoot}});
  assert(nested);
  const auto nestedImage = edward::media::ComponentRenderer{}.render(*nested, 0, {100, 100});
  assert(nestedImage.pixelColor(35, 50).red() > 0);
  assert(nestedImage.pixelColor(35, 50).alpha() > 0 && nestedImage.pixelColor(35, 50).alpha() < 255);
  assert(nestedImage.pixelColor(45, 50).alpha() == 0);

  const QJsonObject scaledRoot{{"id", "root"}, {"type", "shape"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 10}, {"height", 10}, {"scaleX", 2}, {"scaleY", 2}}},
      {"properties", QJsonObject{{"fill", "#00ff00"}}}};
  const auto scaled = edward::core::ComponentIr::parse({{"version", "1"}, {"root", scaledRoot}});
  assert(scaled);
  const auto scaledImage = edward::media::ComponentRenderer{}.render(*scaled, 0, {100, 100});
  assert(scaledImage.pixelColor(40, 50).green() > 0);
  assert(scaledImage.pixelColor(39, 50).alpha() == 0);
  return 0;
}
