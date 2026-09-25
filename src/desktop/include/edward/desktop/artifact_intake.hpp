#pragma once

#include <QString>
#include <QVector>

namespace edward::desktop {

struct ArtifactFile final { QString relativePath; QString kind; QString sha256; };
struct ArtifactPackage final { QString root; QString entry; QVector<ArtifactFile> files; };

class ArtifactIntake final {
 public:
  [[nodiscard]] static ArtifactPackage inspect(const QString& root, QString* error = nullptr);
};

}  // namespace edward::desktop
