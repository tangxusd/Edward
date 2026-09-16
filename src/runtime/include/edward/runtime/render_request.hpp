#pragma once

#include <QJsonObject>
#include <QString>

#include <cstdint>

namespace edward::runtime {

struct RenderRequest final {
  std::int64_t frame = 0;
  QString outputPath;
  QJsonObject props;
};

}  // namespace edward::runtime
