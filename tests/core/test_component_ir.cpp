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
  const auto parsed = edward::core::ComponentIr::parse({{"version", "1"}, {"root", root}});
  assert(parsed);
  assert(parsed->validate());
  assert(parsed->root().children.size() == 1);
  assert(parsed->toJson().value("version").toString() == "1");

  assert(!edward::core::ComponentIr::parse({{"version", "2"}, {"root", root}}));
  const QJsonObject duplicate{{"version", "1"}, {"root", QJsonObject{
      {"id", "root"}, {"type", "container"}, {"children", QJsonArray{
          QJsonObject{{"id", "root"}, {"type", "text"}}
      }}
  }}};
  assert(!edward::core::ComponentIr::parse(duplicate));
  return 0;
}
