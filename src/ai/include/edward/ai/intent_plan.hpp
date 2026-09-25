#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace edward::ai {

// The model-facing capability view contains semantic names only.  It never
// carries an executor, project object ID, path, or revision.
struct CapabilityView final {
  QStringList capabilityIds;
  qsizetype maxIntents = 32;
  qsizetype maxPlanBytes = 256 * 1024;

  [[nodiscard]] bool contains(const QString& capability) const;
};

struct IntentPlan final {
  QString schemaVersion;
  QString requestId;
  QJsonArray intents;

  static std::optional<IntentPlan> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] bool validate(const CapabilityView& capabilities, QString* error = nullptr) const;
};

}  // namespace edward::ai
