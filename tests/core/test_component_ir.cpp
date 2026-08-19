#include <edward/core/component_ir.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <cassert>

int main() {
  const QJsonObject root{{"id", "root"}, {"type", "container"}, {"children", QJsonArray{
      QJsonObject{{"id", "title"}, {"type", "text"},
                   {"properties", QJsonObject{{"text", "Edward"}}},
                   {"keyframes", QJsonObject{{"opacity", QJsonArray{QJsonObject{{"frame", 0}, {"value", 0.0}}}}}}},
  }}};
  auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(parsed);
  assert(parsed->validate());
  assert(parsed->root().children.size() == 1);
  assert(parsed->toJson().value("version").toString() == "1");
  const auto dependent = edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", root},
       {"pluginDependency", QJsonObject{{"pluginId", "remotion"}, {"version", "1.0.0"}}}});
  assert(dependent);
  assert(dependent->pluginDependency()->pluginId == "remotion");
  assert(dependent->toJson().value("pluginDependency").toObject().value("version").toString() == "1.0.0");
  assert(!edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", root}, {"pluginDependency", QJsonObject{{"pluginId", "remotion"}}}}));
  assert(parsed->setNodeTransformNumber("title", "x", 12.0));
  assert(parsed->setNodeProperty("title", "text", "Updated"));
  assert(parsed->setNodeKeyframeNumber("title", "x", 30, 36.0));
  assert(parsed->setNodeKeyframeNumber("title", "x", 0, 12.0));
  assert(parsed->setNodeKeyframeNumber("title", "x", 30, 42.0));
  const auto updated = parsed->toJson().value("root").toObject().value("children").toArray().at(0).toObject();
  assert(updated.value("transform").toObject().value("x").toDouble() == 12.0);
  assert(updated.value("properties").toObject().value("text").toString() == "Updated");
  const auto xKeyframes = updated.value("keyframes").toObject().value("x").toArray();
  assert(xKeyframes.size() == 2);
  assert(xKeyframes.at(0).toObject().value("frame").toInt() == 0);
  assert(xKeyframes.at(1).toObject().value("frame").toInt() == 30);
  assert(xKeyframes.at(1).toObject().value("value").toDouble() == 42.0);
  assert(!parsed->setNodeTransformNumber("missing", "x", 1.0));
  assert(!parsed->setNodeKeyframeNumber("title", "x", -1, 1.0));
  assert(!parsed->setNodeKeyframeNumber("missing", "x", 1, 1.0));
  assert(!parsed->setNodeProperty("title", "", "ignored"));

  assert(!edward::core::ComponentIr::parse({{"version", "2"}, {"root", root}}));
  const QJsonObject duplicate{{"version", "1"}, {"root", QJsonObject{
      {"id", "root"}, {"type", "container"}, {"children", QJsonArray{
          QJsonObject{{"id", "root"}, {"type", "text"}}
      }}
  }}};
  assert(!edward::core::ComponentIr::parse(duplicate));
  return 0;
}
