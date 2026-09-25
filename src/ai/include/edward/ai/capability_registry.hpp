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
  QString undoScope;
  QString collisionPolicy;
  QString trackPlacementPolicy;
  QString linkedMediaPolicy;
  QString taskPolicy;
  QString verificationAdapter;
  QString executor;
  QStringList replacementOf;

  [[nodiscard]] QJsonObject toJson() const;
};

struct CapabilitySnapshot final {
  qint64 version = 0;
  QString hash;
  QJsonArray modelCapabilities;
  QStringList enabledIds;
  bool valid = false;
  QString error;
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
