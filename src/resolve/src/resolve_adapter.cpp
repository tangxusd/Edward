#include "edward/resolve/resolve_adapter.hpp"

#include <QJsonArray>

namespace edward::resolve {

namespace {
bool fail(QString* error, const QString& value) {
  if (error) *error = value;
  return false;
}

bool isStudioVersion(const QString& version) {
  const auto normalized = version.trimmed().toLower();
  return !normalized.isEmpty() && !normalized.contains(QStringLiteral("free")) &&
         !normalized.contains(QStringLiteral("unlicensed"));
}

bool hasValidSubtitleText(const QJsonObject& node) {
  if (node.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
    const auto properties = node.value(QStringLiteral("properties")).toObject();
    const auto text = properties.value(QStringLiteral("text")).toString();
    const auto start = properties.value(QStringLiteral("startFrame"));
    const auto end = properties.value(QStringLiteral("endFrame"));
    if (text.trimmed().isEmpty() || !start.isDouble() || !end.isDouble() ||
        start.toInt() < 0 || end.toInt() <= start.toInt()) return false;
    return true;
  }
  for (const auto& child : node.value(QStringLiteral("children")).toArray()) {
    if (child.isObject() && hasValidSubtitleText(child.toObject())) return true;
  }
  return false;
}

ResolveRenderState renderStateFromString(const QString& value) {
  if (value == QStringLiteral("queued")) return ResolveRenderState::Queued;
  if (value == QStringLiteral("rendering")) return ResolveRenderState::Rendering;
  if (value == QStringLiteral("completed")) return ResolveRenderState::Completed;
  if (value == QStringLiteral("failed")) return ResolveRenderState::Failed;
  if (value == QStringLiteral("canceled")) return ResolveRenderState::Canceled;
  return ResolveRenderState::Unknown;
}
}  // namespace

bool ResolveAdapter::attach(QString* error) {
  const auto detected = capabilities(error);
  if (!detected) return false;
  if (!isStudioVersion(detected->studioVersion)) return fail(error, QStringLiteral("resolve_studio_required"));
  if (!detected->timeline || !detected->fusion || !detected->render) {
    return fail(error, QStringLiteral("resolve_capability_missing"));
  }
  return true;
}

std::optional<ResolveCapabilities> ResolveAdapter::capabilities(QString* error) {
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("capabilities"), {}, &rpcError);
  if (!response) {
    if (error) *error = rpcError.code;
    return std::nullopt;
  }
  const auto result = response->value(QStringLiteral("result")).toObject();
  ResolveCapabilities value;
  value.studioVersion = result.value(QStringLiteral("studioVersion")).toString();
  value.timeline = result.value(QStringLiteral("timeline")).toBool();
  value.fusion = result.value(QStringLiteral("fusion")).toBool();
  value.render = result.value(QStringLiteral("render")).toBool();
  if (!isStudioVersion(value.studioVersion)) {
    if (error) *error = QStringLiteral("resolve_studio_required");
    return std::nullopt;
  }
  capabilities_ = value;
  if (error) error->clear();
  return value;
}

std::optional<ResolveTimelineSnapshot> ResolveAdapter::timelineSnapshot(QString* error) {
  if (!capabilities_ && !attach(error)) return std::nullopt;
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("timeline.snapshot"), {}, &rpcError);
  if (!response) {
    if (error) *error = rpcError.code;
    return std::nullopt;
  }
  const auto result = response->value(QStringLiteral("result")).toObject();
  ResolveTimelineSnapshot snapshot;
  snapshot.projectName = result.value(QStringLiteral("projectName")).toString();
  snapshot.timelineName = result.value(QStringLiteral("timelineName")).toString();
  snapshot.playheadFrame = result.value(QStringLiteral("playheadFrame")).toInt();
  snapshot.timelineStartFrame = result.value(QStringLiteral("timelineStartFrame")).toInt();
  snapshot.fpsNumerator = result.value(QStringLiteral("fpsNumerator")).toInt();
  snapshot.fpsDenominator = result.value(QStringLiteral("fpsDenominator")).toInt(1);
  if (snapshot.projectName.isEmpty() || snapshot.timelineName.isEmpty() || snapshot.fpsNumerator <= 0 ||
      snapshot.fpsDenominator <= 0 || snapshot.playheadFrame < snapshot.timelineStartFrame) {
    if (error) *error = QStringLiteral("timeline_snapshot_invalid");
    return std::nullopt;
  }
  for (const auto& raw : result.value(QStringLiteral("tracks")).toArray()) {
    const auto object = raw.toObject();
    ResolveTrack track;
    track.id = object.value(QStringLiteral("id")).toString();
    track.name = object.value(QStringLiteral("name")).toString();
    track.video = object.value(QStringLiteral("video")).toBool();
    track.audio = object.value(QStringLiteral("audio")).toBool();
    if (track.id.isEmpty() || (!track.video && !track.audio)) {
      if (error) *error = QStringLiteral("timeline_track_invalid");
      return std::nullopt;
    }
    snapshot.tracks.push_back(track);
  }
  if (error) error->clear();
  return snapshot;
}

bool ResolveAdapter::setPlayhead(int frame, QString* error) {
  if (frame < 0) return fail(error, QStringLiteral("playhead_out_of_range"));
  if (!capabilities_ && !attach(error)) return false;
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("timeline.setPlayhead"),
                                         QJsonObject{{QStringLiteral("frame"), frame}}, &rpcError);
  if (!response) return fail(error, rpcError.code);
  if (!response->value(QStringLiteral("result")).toObject().value(QStringLiteral("accepted")).toBool()) {
    return fail(error, QStringLiteral("playhead_rejected"));
  }
  if (error) error->clear();
  return true;
}

bool ResolveAdapter::insertSubtitleComponent(const edward::core::ComponentIr& component,
                                             const ResolveTimelineSnapshot& snapshot,
                                             QString* error) {
  if (!component.validate(error)) return false;
  if (!hasValidSubtitleText(component.toJson().value(QStringLiteral("root")).toObject())) {
    return fail(error, QStringLiteral("subtitle_component_invalid"));
  }
  ResolveError rpcError;
  const auto response = connection_.call(
      QStringLiteral("subtitle.insert"),
      QJsonObject{{QStringLiteral("playheadFrame"), snapshot.playheadFrame},
                  {QStringLiteral("fpsNumerator"), snapshot.fpsNumerator},
                  {QStringLiteral("fpsDenominator"), snapshot.fpsDenominator},
                  {QStringLiteral("component"), component.toJson()}},
      &rpcError);
  if (!response) return fail(error, rpcError.code);
  if (!response->value(QStringLiteral("result")).toObject().value(QStringLiteral("accepted")).toBool()) {
    return fail(error, QStringLiteral("subtitle_insert_rejected"));
  }
  lastInsertedComponentId_ = response->value(QStringLiteral("result")).toObject().value(QStringLiteral("componentId")).toString();
  if (lastInsertedComponentId_.isEmpty()) return fail(error, QStringLiteral("subtitle_component_id_missing"));
  if (error) error->clear();
  return true;
}

bool ResolveAdapter::setComponentKeyframe(const QString& componentId, const QString& nodeId,
                                          const QString& field, int frame, double value,
                                          QString* error) {
  if (componentId.trimmed().isEmpty() || nodeId.trimmed().isEmpty() || field.trimmed().isEmpty() || frame < 0)
    return fail(error, QStringLiteral("component_keyframe_invalid"));
  ResolveError rpcError;
  const auto response = connection_.call(
      QStringLiteral("component.keyframe"),
      QJsonObject{{QStringLiteral("componentId"), componentId},
                  {QStringLiteral("nodeId"), nodeId},
                  {QStringLiteral("field"), field},
                  {QStringLiteral("frame"), frame},
                  {QStringLiteral("value"), value}},
      &rpcError);
  if (!response) return fail(error, rpcError.code);
  if (!response->value(QStringLiteral("result")).toObject().value(QStringLiteral("accepted")).toBool())
    return fail(error, QStringLiteral("component_keyframe_rejected"));
  if (error) error->clear();
  return true;
}

bool ResolveAdapter::queueAndStartRender(const ResolveRenderOptions& options, QString* jobId,
                                         QString* error) {
  if (options.outputPath.trimmed().isEmpty() || options.width <= 0 || options.height <= 0 ||
      options.fps <= 0 || options.codec.trimmed().isEmpty()) {
    return fail(error, QStringLiteral("resolve_render_options_invalid"));
  }
  if (!capabilities_ && !attach(error)) return false;
  ResolveError rpcError;
  const auto response = connection_.call(
      QStringLiteral("render.start"),
      QJsonObject{{QStringLiteral("outputPath"), options.outputPath},
                  {QStringLiteral("width"), options.width},
                  {QStringLiteral("height"), options.height},
                  {QStringLiteral("fps"), options.fps},
                  {QStringLiteral("codec"), options.codec},
                  {QStringLiteral("quality"), options.quality}},
      &rpcError);
  if (!response) return fail(error, rpcError.code);
  const auto result = response->value(QStringLiteral("result")).toObject();
  if (!result.value(QStringLiteral("accepted")).toBool()) return fail(error, QStringLiteral("resolve_render_rejected"));
  const auto id = result.value(QStringLiteral("jobId")).toString();
  if (id.isEmpty()) return fail(error, QStringLiteral("resolve_render_job_id_missing"));
  if (jobId) *jobId = id;
  if (error) error->clear();
  return true;
}

ResolveRenderStatus ResolveAdapter::renderStatus(const QString& jobId, QString* error) {
  ResolveRenderStatus status;
  status.jobId = jobId;
  if (jobId.trimmed().isEmpty()) {
    if (error) *error = QStringLiteral("resolve_render_job_id_invalid");
    status.state = ResolveRenderState::Failed;
    status.error = QStringLiteral("resolve_render_job_id_invalid");
    return status;
  }
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("render.status"),
                                         QJsonObject{{QStringLiteral("jobId"), jobId}}, &rpcError);
  if (!response) {
    if (error) *error = rpcError.code;
    status.state = ResolveRenderState::Failed;
    status.error = rpcError.code;
    return status;
  }
  const auto result = response->value(QStringLiteral("result")).toObject();
  status.state = renderStateFromString(result.value(QStringLiteral("state")).toString());
  status.progress = qBound(0, result.value(QStringLiteral("progress")).toInt(), 100);
  status.error = result.value(QStringLiteral("error")).toString();
  if (status.state == ResolveRenderState::Unknown) {
    if (error) *error = QStringLiteral("resolve_render_status_invalid");
    status.state = ResolveRenderState::Failed;
    status.error = QStringLiteral("resolve_render_status_invalid");
    return status;
  }
  if (error) error->clear();
  return status;
}

bool ResolveAdapter::cancelRender(const QString& jobId, QString* error) {
  if (jobId.trimmed().isEmpty()) return fail(error, QStringLiteral("resolve_render_job_id_invalid"));
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("render.cancel"),
                                         QJsonObject{{QStringLiteral("jobId"), jobId}}, &rpcError);
  if (!response) return fail(error, rpcError.code);
  if (!response->value(QStringLiteral("result")).toObject().value(QStringLiteral("accepted")).toBool())
    return fail(error, QStringLiteral("resolve_render_cancel_rejected"));
  if (error) error->clear();
  return true;
}

bool ResolveAdapter::openDeliverPage(QString* error) {
  ResolveError rpcError;
  const auto response = connection_.call(QStringLiteral("ui.openDeliver"), {}, &rpcError);
  if (!response) return fail(error, rpcError.code);
  if (!response->value(QStringLiteral("result")).toObject().value(QStringLiteral("accepted")).toBool())
    return fail(error, QStringLiteral("resolve_deliver_open_rejected"));
  if (error) error->clear();
  return true;
}

}  // namespace edward::resolve
