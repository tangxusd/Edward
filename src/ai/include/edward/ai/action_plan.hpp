#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace edward::ai {

struct ProjectSnapshot final {
  qint64 revision = 0;
  QStringList knownTargetIds;
  QStringList verifiedResourceIds;
  QStringList knownMediaIds;
  qint64 capabilitySetVersion = -1;
  QString capabilitySetHash;
  QString referenceSnapshotId;
};

struct ActionPlan final {
  QString schemaVersion;
  QString requestId;
  qint64 baseProjectRevision = -1;
  QString referenceSnapshotId;
  QJsonObject capabilitySet;
  QJsonArray operations;

  static std::optional<ActionPlan> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] bool validate(const ProjectSnapshot& project, QString* error = nullptr) const;
};

}  // namespace edward::ai
