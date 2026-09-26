#pragma once

#include <QString>
#include <QVector>

namespace edward::desktop {

struct ArtifactFile final { QString relativePath; QString kind; QString sha256; };
struct ArtifactPackage final { QString root; QString entry; QVector<ArtifactFile> files; };
using ArtifactReceipt = ArtifactPackage;

class ArtifactIntake final {
 public:
  [[nodiscard]] static ArtifactPackage inspect(const QString& root, QString* error = nullptr);
  [[nodiscard]] static ArtifactReceipt validate(const QString& root, QString* error = nullptr) { return inspect(root, error); }
};

}  // namespace edward::desktop
