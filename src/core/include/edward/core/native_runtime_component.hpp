#pragma once

#include <QJsonObject>
#include <QString>

namespace edward::core {

struct NativeRuntimeComponent final {
  QString packageRoot;
  QString manifestPath;
  QString runtime;
  QJsonObject props;

  [[nodiscard]] bool valid() const {
    return !packageRoot.isEmpty() && !manifestPath.isEmpty() &&
           (runtime == QStringLiteral("react") || runtime == QStringLiteral("html-css") ||
            runtime == QStringLiteral("svg") || runtime == QStringLiteral("gsap"));
  }
};

}  // namespace edward::core
