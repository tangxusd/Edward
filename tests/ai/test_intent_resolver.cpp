#include "edward/ai/intent_resolver.hpp"

#include <cassert>
#include <iostream>

using namespace edward::ai;

namespace {
CapabilitySnapshot caps() {
  RuntimeFacts facts;
  facts.executorIds = {QStringLiteral("timeline.executor")};
  facts.verificationAdapterIds = {QStringLiteral("timeline.visible")};
  facts.targetTypes = {QStringLiteral("playhead"), QStringLiteral("selected_clip"), QStringLiteral("selected_component"),
                       QStringLiteral("selected_audio"), QStringLiteral("timeline"), QStringLiteral("selected_track"),
                       QStringLiteral("track"), QStringLiteral("marker")};
  return CapabilityRegistry::builtIn().snapshot(facts);
}

ReferenceSnapshot refs(const CapabilitySnapshot& capabilities) {
  ReferenceSnapshot snapshot;
  snapshot.referenceSnapshotId = QStringLiteral("ref-1");
  snapshot.projectRevision = 7;
  snapshot.capabilitySetVersion = 1;
  snapshot.capabilitySetHash = capabilities.hash;
  snapshot.fps = 30;
  snapshot.playheadFrame = 15;
  snapshot.selectedClipIds = {QStringLiteral("clip-a")};
  snapshot.clips = {ClipReference{QStringLiteral("clip-a"), QStringLiteral("v1"), 0, 90, 4, false, {QStringLiteral("audio-a")}}};
  snapshot.tracks = {TrackReference{QStringLiteral("v1"), 2, false}};
  snapshot.markers = {MarkerReference{QStringLiteral("marker-m3"), QStringLiteral("timeline"), {}, 56, 56, 3, QStringLiteral("blue"), 1},
                      MarkerReference{QStringLiteral("marker-c-m2"), QStringLiteral("clip"), QStringLiteral("clip-a"), 12, 12, 2, QStringLiteral("red"), 1}};
  return snapshot;
}

IntentPlan intent(const QString& capability, const QJsonObject& target, const QJsonObject& args) {
  IntentPlan result;
  result.schemaVersion = QStringLiteral("edward.intent-plan.v1");
  result.requestId = QStringLiteral("req-1");
  result.intents = {QJsonObject{{QStringLiteral("intentId"), QStringLiteral("i-1")},
                                {QStringLiteral("capability"), capability}, {QStringLiteral("targetRef"), target},
                                {QStringLiteral("arguments"), args}}};
  return result;
}
}  // namespace

int main() {
  const auto capabilitySnapshot = caps();
  assert(capabilitySnapshot.valid);
  IntentResolver resolver;

  {
    const auto result = resolver.resolve(intent(QStringLiteral("resize_clip"), {{QStringLiteral("selector"), QStringLiteral("selected_clip")}},
                                               {{QStringLiteral("durationSeconds"), 1.87},
                                                {QStringLiteral("trackPlacementPolicy"), QStringLiteral("specified")}}),
                                         refs(capabilitySnapshot), capabilitySnapshot);
    assert(result.succeeded());
    const auto operation = result.plan->operations.at(0).toObject();
    assert(operation.value(QStringLiteral("args")).toObject().value(QStringLiteral("durationFrames")).toInteger() == 56);
    assert(operation.value(QStringLiteral("target")).toObject().value(QStringLiteral("id")).toString() == QStringLiteral("clip-a"));
  }
  {
    auto snapshot = refs(capabilitySnapshot);
    snapshot.selectedClipIds = {QStringLiteral("clip-a"), QStringLiteral("clip-b")};
    snapshot.clips.push_back(ClipReference{QStringLiteral("clip-b"), QStringLiteral("v1"), 90, 90, 5, false, {}});
    const auto result = resolver.resolve(intent(QStringLiteral("resize_clip"), {{QStringLiteral("selector"), QStringLiteral("selected_clip")}},
                                               {{QStringLiteral("durationSeconds"), 1.0}, {QStringLiteral("trackPlacementPolicy"), QStringLiteral("specified")}}),
                                         snapshot, capabilitySnapshot);
    assert(result.error && result.error->code == ResolutionErrorCode::TargetAmbiguous);
  }
  {
    const auto result = resolver.resolve(intent(QStringLiteral("set_marker_color"),
                                               {{QStringLiteral("selector"), QStringLiteral("global_marker")}, {QStringLiteral("label"), 3}}, {}),
                                         refs(capabilitySnapshot), capabilitySnapshot);
    assert(result.succeeded());
  }
  {
    const auto result = resolver.resolve(intent(QStringLiteral("set_marker_color"), {{QStringLiteral("selector"), QStringLiteral("global_marker(3)")}},
                                               {{QStringLiteral("color"), QStringLiteral("red")}}),
                                         refs(capabilitySnapshot), capabilitySnapshot);
    assert(result.succeeded());
    assert(result.resolutionEvidence.at(0).toObject().value(QStringLiteral("markerId")).toString() == QStringLiteral("marker-m3"));
  }
  {
    auto snapshot = refs(capabilitySnapshot);
    snapshot.clips.front().locked = true;
    const auto result = resolver.resolve(intent(QStringLiteral("set_clip_props"), {{QStringLiteral("selector"), QStringLiteral("selected_clip")}}, {}),
                                         snapshot, capabilitySnapshot);
    assert(result.error && result.error->code == ResolutionErrorCode::TrackConflict);
  }
  {
    const auto result = resolver.resolve(intent(QStringLiteral("resize_clip"), {{QStringLiteral("selector"), QStringLiteral("selected_clip")}},
                                               {{QStringLiteral("durationSeconds"), -1.0}, {QStringLiteral("trackPlacementPolicy"), QStringLiteral("specified")}}),
                                         refs(capabilitySnapshot), capabilitySnapshot);
    assert(result.error && result.error->code == ResolutionErrorCode::FrameInvalid);
  }
  std::cout << "intent resolver tests passed\n";
}
