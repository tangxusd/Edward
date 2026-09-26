#include <edward/ai/intent_resolver.hpp>

#include <QJsonDocument>
#include <QRegularExpression>
#include <QHash>
#include <QSet>

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
void appendUnique(QStringList& values, const QString& value) {
  if (!value.isEmpty() && !values.contains(value)) values.append(value);
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

bool validPolicy(const QString& name, const QString& value) {
  if (value.isEmpty() || value == QStringLiteral("ask")) return false;
  static const QHash<QString, QSet<QString>> values{
      {QStringLiteral("collisionPolicy"), {QStringLiteral("fail"), QStringLiteral("overwrite"), QStringLiteral("new_track"), QStringLiteral("auto")}},
      {QStringLiteral("trackPlacementPolicy"), {QStringLiteral("specified"), QStringLiteral("current"), QStringLiteral("auto"), QStringLiteral("new_track")}},
      {QStringLiteral("linkedMediaPolicy"), {QStringLiteral("preserve"), QStringLiteral("split"), QStringLiteral("ignore")}},
      {QStringLiteral("markerPolicy"), {QStringLiteral("preserve"), QStringLiteral("move"), QStringLiteral("drop"), QStringLiteral("rebase")}}};
  return values.value(name).contains(value);
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
    auto clips = selectClips(selector, ref, references);
    if (selector == QStringLiteral("playhead_clip") && clips.size() > 1) { fail(result, ResolutionErrorCode::TargetAmbiguous, QStringLiteral("playhead intersects multiple clips"), intentId, selector); return result; }
    if (selector == QStringLiteral("selected_component")) { clips.clear(); for (const auto& id : references.selectedComponentIds) if (const auto* clip = references.clip(id)) clips.push_back(clip); }
    if (selector == QStringLiteral("selected_audio")) { clips.clear(); for (const auto& id : references.selectedMediaIds) if (const auto* clip = references.clip(id)) clips.push_back(clip); }
    if (selector == QStringLiteral("selected_clip") && clips.size() != 1) { fail(result, clips.isEmpty() ? ResolutionErrorCode::TargetNotFound : ResolutionErrorCode::TargetAmbiguous, QStringLiteral("selection must resolve to one clip"), intentId, selector); return result; }
    if (selector == QStringLiteral("selected_clips") && clips.isEmpty()) { fail(result, ResolutionErrorCode::TargetNotFound, QStringLiteral("selection must resolve to at least one clip"), intentId, selector); return result; }
    QString targetId;
    QString targetKind = selector;
    if (!clips.isEmpty()) {
      targetId = clips.first()->id;
      targetKind = selector == QStringLiteral("selected_component") ? QStringLiteral("selected_component") : (selector == QStringLiteral("selected_audio") ? QStringLiteral("selected_audio") : QStringLiteral("selected_clip"));
      for (const auto* clip : clips) if (clip->locked) { fail(result, ResolutionErrorCode::TrackConflict, QStringLiteral("target is locked"), intentId, selector); return result; }
    }
    else if (selector == QStringLiteral("timeline") || selector == QStringLiteral("current_track")) { targetId = selector == QStringLiteral("timeline") ? QStringLiteral("timeline") : references.currentTrackId; targetKind = selector; }
    else if (selector.startsWith(QStringLiteral("global_marker")) || selector.startsWith(QStringLiteral("clip_marker"))) {
      const auto match = QRegularExpression(QStringLiteral("\\((\\d+)\\)")).match(selector);
      const int number = match.hasMatch() ? match.captured(1).toInt() : ref.value(QStringLiteral("label")).toInt(ref.value(QStringLiteral("index")).toInt(-1));
      const bool clipMarker = selector.startsWith(QStringLiteral("clip_marker"));
      int markerMatches = 0;
      for (const auto& marker : references.markers) if (marker.scope == (clipMarker ? QStringLiteral("clip") : QStringLiteral("timeline")) && marker.displayNumber == number) { targetId = marker.markerId; targetKind = QStringLiteral("marker"); markerMatches++; }
      if (markerMatches > 1) { fail(result, ResolutionErrorCode::TargetAmbiguous, QStringLiteral("marker number is ambiguous"), intentId, selector); return result; }
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
    const auto targetTrack = !clips.isEmpty() ? references.track(clips.first()->trackId) : nullptr;
    if (targetTrack && targetTrack->locked) { fail(result, ResolutionErrorCode::TrackConflict, QStringLiteral("track is locked"), intentId, selector); return result; }
    qint64 targetVersion = 0;
    if (targetTrack) targetVersion = targetTrack->version;
    for (const auto& marker : references.markers) if (marker.markerId == targetId) targetVersion = marker.version;
    QJsonObject policies;
    for (const auto& policy : {QStringLiteral("collisionPolicy"), QStringLiteral("trackPlacementPolicy"),
                               QStringLiteral("linkedMediaPolicy"), QStringLiteral("markerPolicy")}) {
      const auto requested = args.value(policy).toString();
      QString fallback = contract.value(policy).toString();
      if (fallback.isEmpty()) {
        fallback = policy == QStringLiteral("collisionPolicy") ? QStringLiteral("fail")
            : policy == QStringLiteral("trackPlacementPolicy") ? QStringLiteral("auto")
            : policy == QStringLiteral("linkedMediaPolicy") ? QStringLiteral("preserve")
            : QStringLiteral("preserve");
      }
      const auto value = requested.isEmpty() ? fallback : requested;
      if (!requested.isEmpty() && !validPolicy(policy, value)) {
        fail(result, ResolutionErrorCode::PolicyRequired, QStringLiteral("policy '%1' is missing or invalid").arg(policy), intentId, selector);
        return result;
      }
      policies.insert(policy, value);
    }
    const auto requestedTrackId = args.value(QStringLiteral("trackId")).toString(args.value(QStringLiteral("track")).toString());
    const auto operationTrack = requestedTrackId.isEmpty() ? targetTrack : references.track(requestedTrackId);
    if (!requestedTrackId.isEmpty() && !operationTrack) {
      fail(result, ResolutionErrorCode::TargetNotFound, QStringLiteral("requested track not found"), intentId, selector);
      return result;
    }
    if (operationTrack && operationTrack->locked) {
      fail(result, ResolutionErrorCode::TrackConflict, QStringLiteral("requested track is locked"), intentId, selector);
      return result;
    }
    if (operationTrack && policies.value(QStringLiteral("collisionPolicy")) == QStringLiteral("fail") && !clips.isEmpty()) {
      const auto start = args.value(QStringLiteral("startFrame")).toInteger(clips.first()->startFrame);
      const auto duration = args.value(QStringLiteral("durationFrames")).toInteger(clips.first()->durationFrames);
      const auto end = start + duration;
      for (const auto& candidate : references.clips) {
        if (candidate.id == clips.first()->id || candidate.trackId != operationTrack->id) continue;
        if (start < candidate.startFrame + candidate.durationFrames && candidate.startFrame < end) {
          fail(result, ResolutionErrorCode::TrackConflict, QStringLiteral("requested track has an overlapping clip"), intentId, selector);
          return result;
        }
      }
    }
    const auto evidence = QJsonObject{{QStringLiteral("referenceSnapshotId"), references.referenceSnapshotId}, {QStringLiteral("selector"), selector}, {QStringLiteral("projectRevision"), references.projectRevision}, {QStringLiteral("markerId"), targetKind == QStringLiteral("marker") ? targetId : QString()}};
    QJsonArray readSet;
    QJsonArray writeSet;
    QStringList resolvedIds;
    for (const auto* selected : clips) {
      appendUnique(resolvedIds, selected->id);
      const auto linkedPolicy = policies.value(QStringLiteral("linkedMediaPolicy"));
      if (linkedPolicy == QStringLiteral("preserve") || linkedPolicy == QStringLiteral("split")) {
        for (const auto& linked : selected->linkedClipIds) {
          if (!references.clip(linked)) {
            fail(result, ResolutionErrorCode::ReferenceStale, QStringLiteral("linked clip is missing"), intentId, selector);
            return result;
          }
          appendUnique(resolvedIds, linked);
        }
      }
    }
    for (const auto& id : resolvedIds) { readSet.append(id); writeSet.append(id); appendUnique(result.readSet, id); appendUnique(result.writeSet, id); }
    const auto appendOperation = [&](const ClipReference* selected, const QString& id, const QString& generatedOperationId) {
      QJsonArray targetReadSet;
      QJsonArray targetWriteSet;
      QStringList targetIds;
      if (selected) {
        appendUnique(targetIds, selected->id);
        const auto linkedPolicy = policies.value(QStringLiteral("linkedMediaPolicy"));
        if (linkedPolicy == QStringLiteral("preserve") || linkedPolicy == QStringLiteral("split")) {
          for (const auto& linked : selected->linkedClipIds) appendUnique(targetIds, linked);
        }
      } else {
        targetIds = resolvedIds;
      }
      for (const auto& target : targetIds) { targetReadSet.append(target); targetWriteSet.append(target); }
      const auto operationTarget = selected ? selected->id : id;
      auto operationEvidence = evidence;
      if (selected) operationEvidence.insert(QStringLiteral("targetId"), operationTarget);
      auto operationPrecondition = targetVersion;
      if (selected) {
        if (const auto track = references.track(selected->trackId)) operationPrecondition = track->version;
        operationPrecondition = selected->version;
      }
      const auto operationKind = selected ? (selector == QStringLiteral("selected_component") ? QStringLiteral("selected_component") : (selector == QStringLiteral("selected_audio") ? QStringLiteral("selected_audio") : QStringLiteral("selected_clip"))) : targetKind;
      const QJsonObject operationTargetObject{{QStringLiteral("kind"), operationKind}, {QStringLiteral("id"), operationTarget}, {QStringLiteral("resolvedFrom"), selector}};
      result.resolutionEvidence.append(operationEvidence);
      operations.append(QJsonObject{{QStringLiteral("operationId"), generatedOperationId}, {QStringLiteral("capability"), capability}, {QStringLiteral("target"), operationTargetObject}, {QStringLiteral("args"), args}, {QStringLiteral("policies"), policies}, {QStringLiteral("dependsOn"), QJsonArray{}}, {QStringLiteral("preconditions"), QJsonArray{QJsonObject{{QStringLiteral("projectRevision"), references.projectRevision}, {QStringLiteral("targetVersion"), operationPrecondition}}}}, {QStringLiteral("readSet"), targetReadSet}, {QStringLiteral("writeSet"), targetWriteSet}, {QStringLiteral("resolutionEvidence"), operationEvidence}});
    };
    if (selector == QStringLiteral("selected_clips")) {
      for (const auto* selected : clips) appendOperation(selected, selected->id, QStringLiteral("op-%1").arg(index++));
    } else {
      appendOperation(nullptr, targetId, operationId);
    }
  }
  ActionPlan plan; plan.schemaVersion = QStringLiteral("orbit.bound-action-plan.v2"); plan.requestId = intent.requestId; plan.baseProjectRevision = references.projectRevision; plan.referenceSnapshotId = references.referenceSnapshotId; plan.capabilitySet = {{QStringLiteral("version"), capabilities.version}, {QStringLiteral("hash"), capabilities.hash}}; plan.operations = operations; result.plan = plan; return result;
}
} // namespace edward::ai
