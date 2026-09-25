#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace edward::ai {

// RuntimeFacts contains only facts about the currently available, local
// executors.  It deliberately does not contain project objects or paths.
struct RuntimeFacts final {
  QStringList executorIds;
  QStringList verificationAdapterIds;
  QStringList targetTypes;
  qint64 registryVersion = 1;
};

struct CapabilityContract final {
  QString id;
  QString version;
  QJsonObject inputSchema;
  QStringList targetTypes;
  QString permissionCategory;
  bool mutatesProject = true;
  QStringList externalSideEffects;
  QString selectionPolicy;
  QString unitPolicy;
  QString coalescingPolicy;
  QString lockPolicy;
  QString playbackPolicy;
  QJsonObject limits;
  QString undoScope;
  QString collisionPolicy;
  QString trackPlacementPolicy;
  QString linkedMediaPolicy;
  QJsonObject taskPolicy;
  QString executionMode;
  QString reversibility;
  QStringList allowedPolicies;
  QString markerPolicy;
  QString validate;
  QString execute;
  QStringList postconditions;
  QString preview;
  QString render;
  QString verificationAdapter;
  QString executor;
  QStringList replacementOf;

  [[nodiscard]] QJsonObject toJson() const;
};

struct CapabilitySnapshot final {
  QString schemaVersion = QStringLiteral("orbit.capability-snapshot.v1");
  qint64 version = 0;
  QString hash;
  QStringList contractIds;
  QString canonicalText;
  QJsonObject canonicalJson;
  QJsonArray modelCapabilities;
  QStringList enabledIds;
  bool valid = false;
  QString error;

  [[nodiscard]] QJsonObject toJson() const;
};

class CapabilityRegistry final {
 public:
  CapabilityRegistry();
  explicit CapabilityRegistry(QVector<CapabilityContract> contracts);

  [[nodiscard]] static CapabilityRegistry builtIn();
  [[nodiscard]] CapabilitySnapshot snapshot(const RuntimeFacts& facts) const;
  [[nodiscard]] QJsonArray modelView(const CapabilitySnapshot& snapshot) const;
  [[nodiscard]] std::optional<CapabilityContract> find(QStringView id, QStringView version) const;
  [[nodiscard]] bool isValid() const { return validationError_.isEmpty(); }
  [[nodiscard]] QString validationError() const { return validationError_; }

 private:
  QVector<CapabilityContract> contracts_;
  QString validationError_;

  void validate();
};

}  // namespace edward::ai
