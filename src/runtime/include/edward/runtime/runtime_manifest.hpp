#pragma once

#include <QJsonObject>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace edward::runtime {

struct RuntimeManifest final {
  QString protocol;
  QString runtime;
  int width = 0;
  int height = 0;
  double fps = 0.0;
  std::int64_t durationInFrames = 0;
  QString previewEntry;
  QString renderEntry;
  QJsonObject propsSchema;
  QStringList editableProperties;

  static std::optional<RuntimeManifest> parse(const QJsonObject& object,
                                              QString* error = nullptr);
  [[nodiscard]] bool validate(QString* error = nullptr) const;
};

}  // namespace edward::runtime
