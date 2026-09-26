#pragma once

#include "edward/ai/action_plan.hpp"
#include "edward/ai/capability_registry.hpp"
#include "edward/ai/intent_plan.hpp"

#include <QJsonArray>
#include <QJsonObject>

#include <optional>

namespace edward::ai {

enum class ResolutionErrorCode {
  None,
  TargetNotFound,
  TargetAmbiguous,
  ReferenceStale,
  PolicyRequired,
  FrameInvalid,
  CapabilityUnavailable,
  IntentInvalid,
  TrackConflict,
};

struct ResolveError final {
  ResolutionErrorCode code = ResolutionErrorCode::None;
  QString detail;
  QString intentId;
  QString selector;
  QJsonArray candidates;

  [[nodiscard]] QString codeString() const;
  [[nodiscard]] QJsonObject toJson() const;
};

struct ResolveResult final {
  std::optional<ActionPlan> plan;
  QJsonArray resolutionEvidence;
  QStringList readSet;
  QStringList writeSet;
  std::optional<ResolveError> error;

  [[nodiscard]] bool succeeded() const { return plan.has_value() && !error.has_value(); }
};

class IntentResolver final {
 public:
  [[nodiscard]] ResolveResult resolve(const IntentPlan& intent,
                                      const ReferenceSnapshot& references,
                                      const CapabilitySnapshot& capabilities) const;
};

}  // namespace edward::ai
