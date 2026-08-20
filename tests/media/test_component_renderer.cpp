#include <edward/core/component_ir.hpp>
#include <edward/media/component_renderer.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QGuiApplication>
#include <cassert>

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
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

  const QJsonObject linearRoot{{"id", "linear-root"}, {"type", "shape"},
      {"transform", QJsonObject{{"y", 40}, {"width", 8}, {"height", 8}}},
      {"properties", QJsonObject{{"fill", "#ffffff"}}},
      {"keyframes", QJsonObject{{"x", QJsonArray{
          QJsonObject{{"frame", 0}, {"value", 10}, {"easing", "linear"}},
          QJsonObject{{"frame", 10}, {"value", 70}}
      }}}}};
  const QJsonObject bezierRoot{{"id", "bezier-root"}, {"type", "shape"},
      {"transform", QJsonObject{{"y", 40}, {"width", 8}, {"height", 8}}},
      {"properties", QJsonObject{{"fill", "#ffffff"}}},
      {"keyframes", QJsonObject{{"x", QJsonArray{
          QJsonObject{{"frame", 0}, {"value", 10}, {"easing", "bezier"}, {"controlOut", 10}},
          QJsonObject{{"frame", 10}, {"value", 70}, {"controlIn", 70}}
      }}}}};
  const auto linear = edward::core::ComponentIr::parse({{"version", "1"}, {"root", linearRoot}});
  const auto bezier = edward::core::ComponentIr::parse({{"version", "1"}, {"root", bezierRoot}});
  assert(linear && bezier);
  const auto linearImage = edward::media::ComponentRenderer{}.render(*linear, 2, {100, 100});
  const auto bezierImage = edward::media::ComponentRenderer{}.render(*bezier, 2, {100, 100});
  assert(linearImage.pixelColor(28, 10).alpha() > 0);
  assert(bezierImage.pixelColor(28, 10).alpha() == 0);

  // Keyframes are normalized by time before interpolation; control values must
  // stay attached to their sorted keyframe, even when input JSON is unordered.
  const QJsonObject unorderedRoot{{"id", "unordered-root"}, {"type", "shape"},
      {"transform", QJsonObject{{"y", 0}, {"width", 8}, {"height", 8}}},
      {"properties", QJsonObject{{"fill", "#ffffff"}}},
      {"keyframes", QJsonObject{{"x", QJsonArray{
          QJsonObject{{"frame", 10}, {"value", 70}, {"controlIn", 70}},
          QJsonObject{{"frame", 0}, {"value", 10}, {"easing", "bezier"}, {"controlOut", 10}}
      }}}}};
  const auto unordered = edward::core::ComponentIr::parse({{"version", "1"}, {"root", unorderedRoot}});
  assert(unordered);
  const auto unorderedImage = edward::media::ComponentRenderer{}.render(*unordered, 5, {100, 100});
  assert(unorderedImage.pixelColor(8, 50).alpha() > 0);

  // Duplicate timestamps must not create a zero-duration interpolation span.
  const QJsonObject duplicateRoot{{"id", "duplicate-root"}, {"type", "shape"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 8}, {"height", 8}}},
      {"properties", QJsonObject{{"fill", "#ffffff"}}},
      {"keyframes", QJsonObject{{"x", QJsonArray{
          QJsonObject{{"frame", 0}, {"value", 0}},
          QJsonObject{{"frame", 0}, {"value", 10}},
          QJsonObject{{"frame", 10}, {"value", 20}}
      }}}}};
  const auto duplicate = edward::core::ComponentIr::parse({{"version", "1"}, {"root", duplicateRoot}});
  assert(!duplicate);

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

  const QJsonObject borderedRoot{{"id", "bordered"}, {"type", "shape"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 40}, {"height", 40}}},
      {"properties", QJsonObject{{"fill", "#000000"}, {"borderColor", "#ff0000"}, {"borderWidth", 4}}}};
  const auto bordered = edward::core::ComponentIr::parse({{"version", "1"}, {"root", borderedRoot}});
  assert(bordered);
  const auto borderedImage = edward::media::ComponentRenderer{}.render(*bordered, 0, {100, 100});
  assert(borderedImage.pixelColor(30, 50).red() > 200);
  assert(borderedImage.pixelColor(30, 50).green() < 80);

  const QJsonObject scaledRoot{{"id", "root"}, {"type", "shape"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 10}, {"height", 10}, {"scaleX", 2}, {"scaleY", 2}}},
      {"properties", QJsonObject{{"fill", "#00ff00"}}}};
  const auto scaled = edward::core::ComponentIr::parse({{"version", "1"}, {"root", scaledRoot}});
  assert(scaled);
  const auto scaledImage = edward::media::ComponentRenderer{}.render(*scaled, 0, {100, 100});
  assert(scaledImage.pixelColor(40, 50).green() > 0);
  assert(scaledImage.pixelColor(39, 50).alpha() == 0);

  const QJsonObject textRoot{{"id", "root"}, {"type", "text"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 80}, {"height", 30}}},
      {"properties", QJsonObject{{"text", "Before"}, {"fontSize", 16}, {"color", "#ffffff"}}},
      {"keyframes", QJsonObject{{"text", QJsonArray{QJsonObject{{"frame", 10}, {"value", "After"}}}}}}};
  const auto text = edward::core::ComponentIr::parse({{"version", "1"}, {"root", textRoot}});
  assert(text);
  const auto before = edward::media::ComponentRenderer{}.render(*text, 0, {100, 100});
  const auto after = edward::media::ComponentRenderer{}.render(*text, 10, {100, 100});
  assert(before != after);

  const QJsonObject animatedColorRoot{{"id", "color"}, {"type", "shape"},
      {"transform", QJsonObject{{"x", 0}, {"y", 0}, {"width", 20}, {"height", 20}}},
      {"properties", QJsonObject{{"fill", "#ff0000"}}},
      {"keyframes", QJsonObject{{"fill", QJsonArray{
          QJsonObject{{"frame", 0}, {"value", "#ff0000"}},
          QJsonObject{{"frame", 10}, {"value", "#0000ff"}}}}}}};
  const auto animatedColorComponent = edward::core::ComponentIr::parse({{"version", "1"}, {"root", animatedColorRoot}});
  assert(animatedColorComponent);
  const auto colorFrame0 = edward::media::ComponentRenderer{}.render(*animatedColorComponent, 0, {100, 100});
  const auto colorFrame10 = edward::media::ComponentRenderer{}.render(*animatedColorComponent, 10, {100, 100});
  assert(colorFrame0.pixelColor(40, 50).red() > 200 && colorFrame0.pixelColor(40, 50).blue() < 80);
  assert(colorFrame10.pixelColor(40, 50).blue() > 200 && colorFrame10.pixelColor(40, 50).red() < 80);
  return 0;
}
