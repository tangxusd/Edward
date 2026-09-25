#include <edward/ai/intent_resolver.hpp>

#include <QJsonDocument>
#include <QRegularExpression>

namespace edward::ai {
namespace {
QString selectorOf(const QJsonObject& ref) { return ref.value(QStringLiteral("selector")).toString(); }
QString errorCode(ResolutionErrorCode code) {
  switch (code) {
    case ResolutionErrorCode::TargetNotFound: return QStringLiteral("TARGET_NOT_FOUND");
    case ResolutionErrorCode::TargetAmbiguous: return QStringLiteral("TARGET_AMBIGUOUS");
    case ResolutionErrorCode::ReferenceStale: return QStringLiteral("REFERENCE_STALE");
    case ResolutionErrorCode::PolicyRequired: return QStringLiteral("POLICY_REQUIRED");
    case ResolutionErrorCode::FrameInvalid: return QStringLiteral("FRAME_INVALID");
    case ResolutionErrorCode::CapabilityUnavailable: return QStringLiteral("CAPABILITY_UNAVAILABLE");
    case ResolutionErrorCode::IntentInvalid: return QStringLiteral("INTENT_INVALID");
    case ResolutionErrorCode::TrackConflict: return QStringLiteral("TRACK_CONFLICT");
    default: return QStringLiteral("NONE");
  }
}
void fail(ResolveResult& result, ResolutionErrorCode code, const QString& detail, const QString& intentId = {}, const QString& selector = {}) {
  result.error = ResolveError{code, detail, intentId, selector, {}};
}
QList<const ClipReference*> selectClips(const QString& selector, const QJsonObject& ref, const ReferenceSnapshot& snapshot) {
  QList<const ClipReference*> result;
  if (selector == QStringLiteral("selected_clip") || selector == QStringLiteral("selected_clips")) {
    for (const auto& id : snapshot.selectedClipIds) if (const auto* clip = snapshot.clip(id)) result.push_back(clip);
  } else if (selector == QStringLiteral("playhead_clip")) {
    for (const auto& clip : snapshot.clips) if (snapshot.playheadFrame >= clip.startFrame && snapshot.playheadFrame < clip.startFrame + clip.durationFrames) result.push_back(&clip);
  } else if (selector == QStringLiteral("execution_selection")) {
    for (const auto& id : snapshot.selectedClipIds) if (const auto* clip = snapshot.clip(id)) result.push_back(clip);
  }
  Q_UNUSED(ref);
  return result;
}
} // namespace

QString ResolveError::codeString() const { return errorCode(code); }
QJsonObject ResolveError::toJson() const { return {{QStringLiteral("code"), codeString()}, {QStringLiteral("detail"), detail}, {QStringLiteral("intentId"), intentId}, {QStringLiteral("selector"), selector}, {QStringLiteral("candidates"), candidates}}; }

ResolveResult IntentResolver::resolve(const IntentPlan& intent, const ReferenceSnapshot& references, const CapabilitySnapshot& capabilities) const {
  ResolveResult result;
  if (!capabilities.valid || references.referenceSnapshotId.isEmpty() || references.fps <= 0 || references.projectRevision < 0) {
    fail(result, ResolutionErrorCode::ReferenceStale, QStringLiteral("reference or capability snapshot is invalid")); return result;
  }
  if (references.capabilitySetVersion != capabilities.version || references.capabilitySetHash != capabilities.hash) {
    fail(result, ResolutionErrorCode::ReferenceStale, QStringLiteral("capability snapshot changed")); return result;
  }
  QJsonArray operations;
  int index = 0;
  for (const auto& value : intent.intents) {
    const auto item = value.toObject();
    const auto intentId = item.value(QStringLiteral("intentId")).toString().isEmpty() ? QStringLiteral("intent-%1").arg(index) : item.value(QStringLiteral("intentId")).toString();
    const auto capability = item.value(QStringLiteral("capability")).toString();
    const auto ref = item.value(QStringLiteral("targetRef")).toObject();
    const auto selector = selectorOf(ref);
    const auto contracts = capabilities.modelCapabilities;
    QJsonObject contract;
    for (const auto& candidate : contracts) if (candidate.toObject().value(QStringLiteral("id")).toString() == capability) { contract = candidate.toObject(); break; }
    if (contract.isEmpty()) { fail(result, ResolutionErrorCode::CapabilityUnavailable, QStringLiteral("capability is unavailable"), intentId, selector); return result; }
    const auto clips = selectClips(selector, ref, references);
    if ((selector == QStringLiteral("selected_clip") || selector == QStringLiteral("selected_clips")) && clips.size() != 1) { fail(result, clips.isEmpty() ? ResolutionErrorCode::TargetNotFound : ResolutionErrorCode::TargetAmbiguous, QStringLiteral("selection must resolve to one clip"), intentId, selector); return result; }
    QString targetId;
    QString targetKind = selector;
    if (!clips.isEmpty()) { targetId = clips.first()->id; targetKind = QStringLiteral("selected_clip"); if (clips.first()->locked) { fail(result, ResolutionErrorCode::TrackConflict, QStringLiteral("target is locked"), intentId, selector); return result; } }
    else if (selector == QStringLiteral("timeline") || selector == QStringLiteral("current_track")) { targetId = selector == QStringLiteral("timeline") ? QStringLiteral("timeline") : references.currentTrackId; targetKind = selector; }
    else if (selector.startsWith(QStringLiteral("global_marker"))) {
      const auto match = QRegularExpression(QStringLiteral("\\((\\d+)\\)")).match(selector);
      const int number = match.hasMatch() ? match.captured(1).toInt() : ref.value(QStringLiteral("label")).toInt(ref.value(QStringLiteral("index")).toInt(-1));
      for (const auto& marker : references.markers) if (marker.scope == QStringLiteral("timeline") && marker.displayNumber == number) { targetId = marker.markerId; targetKind = QStringLiteral("marker"); break; }
      if (targetId.isEmpty()) { fail(result, ResolutionErrorCode::TargetNotFound, QStringLiteral("marker not found"), intentId, selector); return result; }
    }
    if (targetId.isEmpty()) { fail(result, ResolutionErrorCode::TargetNotFound, QStringLiteral("symbolic target not found"), intentId, selector); return result; }
    const auto operationId = QStringLiteral("op-%1").arg(index++);
    auto args = item.value(QStringLiteral("arguments")).toObject();
    if (args.contains(QStringLiteral("durationSeconds"))) {
      const auto seconds = args.value(QStringLiteral("durationSeconds")).toDouble(-1);
      if (!(seconds > 0) || !qIsFinite(seconds)) { fail(result, ResolutionErrorCode::FrameInvalid, QStringLiteral("duration must be positive"), intentId, selector); return result; }
      args.remove(QStringLiteral("durationSeconds")); args.insert(QStringLiteral("durationFrames"), qRound64(seconds * references.fps));
    }
    const auto evidence = QJsonObject{{QStringLiteral("referenceSnapshotId"), references.referenceSnapshotId}, {QStringLiteral("selector"), selector}, {QStringLiteral("projectRevision"), references.projectRevision}, {QStringLiteral("markerId"), targetKind == QStringLiteral("marker") ? targetId : QString()}};
    result.resolutionEvidence.append(evidence);
    operations.append(QJsonObject{{QStringLiteral("operationId"), operationId}, {QStringLiteral("capability"), capability}, {QStringLiteral("target"), QJsonObject{{QStringLiteral("kind"), targetKind}, {QStringLiteral("id"), targetId}, {QStringLiteral("resolvedFrom"), selector}}}, {QStringLiteral("args"), args}, {QStringLiteral("policies"), QJsonObject{{QStringLiteral("collisionPolicy"), contract.value(QStringLiteral("collisionPolicy"))}, {QStringLiteral("trackPlacementPolicy"), contract.value(QStringLiteral("trackPlacementPolicy"))}, {QStringLiteral("linkedMediaPolicy"), contract.value(QStringLiteral("linkedMediaPolicy"))}, {QStringLiteral("markerPolicy"), contract.value(QStringLiteral("markerPolicy"))}}}, {QStringLiteral("dependsOn"), QJsonArray{}}, {QStringLiteral("preconditions"), QJsonArray{QJsonObject{{QStringLiteral("projectRevision"), references.projectRevision}}}}, {QStringLiteral("readSet"), QJsonArray{targetId}}, {QStringLiteral("writeSet"), QJsonArray{targetId}}, {QStringLiteral("resolutionEvidence"), evidence}});
  }
  ActionPlan plan; plan.schemaVersion = QStringLiteral("orbit.bound-action-plan.v2"); plan.requestId = intent.requestId; plan.baseProjectRevision = references.projectRevision; plan.referenceSnapshotId = references.referenceSnapshotId; plan.capabilitySet = {{QStringLiteral("version"), capabilities.version}, {QStringLiteral("hash"), capabilities.hash}}; plan.operations = operations; result.plan = plan; return result;
}
} // namespace edward::ai
