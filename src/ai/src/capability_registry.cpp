#include <edward/ai/capability_registry.hpp>

#include <algorithm>
#include <functional>
#include <utility>

namespace edward::ai {
namespace {

constexpr auto kExecutor = "timeline.executor";
constexpr auto kVerifier = "timeline.visible";

CapabilityContract makeContract(const QString& id, const QStringList& targets, const QStringList& fields,
                                QString permission = QStringLiteral("project.write"),
                                QJsonObject task = QJsonObject{{QStringLiteral("mode"), QStringLiteral("synchronous")},
                                                                 {QStringLiteral("resourceClass"), QStringLiteral("cpu")},
                                                                 {QStringLiteral("priority"), 50},
                                                                 {QStringLiteral("cancellableUntil"), QStringLiteral("commit")},
                                                                 {QStringLiteral("resumable"), false},
                                                                 {QStringLiteral("maxConcurrency"), 1},
                                                                 {QStringLiteral("diskReservation"), 0},
                                                                 {QStringLiteral("progressAdapter"), QStringLiteral("none")}}) {
  QJsonArray properties;
  Q_UNUSED(properties);
  QJsonObject schema;
  schema.insert(QStringLiteral("type"), QStringLiteral("object"));
  QJsonObject fieldSchema;
  for (const auto& field : fields) fieldSchema.insert(field, QJsonObject{{QStringLiteral("type"), QStringLiteral("any")}});
  schema.insert(QStringLiteral("properties"), fieldSchema);
  return {id,
          QStringLiteral("1"),
          schema,
          targets,
          std::move(permission),
          true,
          {},
          QStringLiteral("explicit_or_resolved"),
          QStringLiteral("frames_at_project_fps"),
          QStringLiteral("one_transaction"),
          QStringLiteral("reject_locked"),
          QStringLiteral("preserve_playhead"),
          QJsonObject{{QStringLiteral("maxOperations"), 64}, {QStringLiteral("maxTargets"), 64}},
          QStringLiteral("project_transaction"),
          QStringLiteral("fail"),
          QStringLiteral("specified_or_auto"),
          QStringLiteral("preserve_linked"),
          std::move(task),
          QStringLiteral("local_transaction"),
          QStringLiteral("undoable"),
          {QStringLiteral("collision"), QStringLiteral("trackPlacement"), QStringLiteral("linkedMedia"), QStringLiteral("marker")},
          QStringLiteral("preserve"),
          QStringLiteral("schema_and_target"),
          QStringLiteral("timeline_executor"),
          {QStringLiteral("project_hash_changed"), QStringLiteral("visible_state_verified")},
          QStringLiteral("timeline_preview"),
          QStringLiteral("timeline_render"),
          QString::fromLatin1(kVerifier),
          QString::fromLatin1(kExecutor),
          {}};
}

QStringList sorted(QStringList values) {
  values.removeDuplicates();
  std::sort(values.begin(), values.end());
  return values;
}

QString fnv1a64(const QByteArray& value) {
  quint64 hash = 0xcbf29ce484222325ULL;
  for (const auto byte : value) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 0x100000001b3ULL;
  }
  return QString::number(hash, 16).rightJustified(16, QLatin1Char('0'));
}

}  // namespace

QJsonObject CapabilityContract::toJson() const {
  return {{QStringLiteral("id"), id},
          {QStringLiteral("version"), version},
          {QStringLiteral("inputSchema"), inputSchema},
          {QStringLiteral("targetTypes"), QJsonArray::fromStringList(targetTypes)},
          {QStringLiteral("permissionCategory"), permissionCategory},
          {QStringLiteral("mutatesProject"), mutatesProject},
          {QStringLiteral("externalSideEffects"), QJsonArray::fromStringList(externalSideEffects)},
          {QStringLiteral("selectionPolicy"), selectionPolicy},
          {QStringLiteral("unitPolicy"), unitPolicy},
          {QStringLiteral("coalescingPolicy"), coalescingPolicy},
          {QStringLiteral("lockPolicy"), lockPolicy},
          {QStringLiteral("playbackPolicy"), playbackPolicy},
          {QStringLiteral("limits"), limits},
          {QStringLiteral("undoScope"), undoScope},
          {QStringLiteral("collisionPolicy"), collisionPolicy},
          {QStringLiteral("trackPlacementPolicy"), trackPlacementPolicy},
          {QStringLiteral("linkedMediaPolicy"), linkedMediaPolicy},
          {QStringLiteral("taskPolicy"), taskPolicy},
          {QStringLiteral("executionMode"), executionMode},
          {QStringLiteral("reversibility"), reversibility},
          {QStringLiteral("allowedPolicies"), QJsonArray::fromStringList(allowedPolicies)},
          {QStringLiteral("markerPolicy"), markerPolicy},
          {QStringLiteral("validate"), validate},
          {QStringLiteral("execute"), execute},
          {QStringLiteral("postconditions"), QJsonArray::fromStringList(postconditions)},
          {QStringLiteral("preview"), preview},
          {QStringLiteral("render"), render},
          {QStringLiteral("verificationAdapter"), verificationAdapter}};
}

QJsonObject CapabilitySnapshot::toJson() const {
  return {{QStringLiteral("schemaVersion"), schemaVersion},
          {QStringLiteral("version"), version},
          {QStringLiteral("contractIds"), QJsonArray::fromStringList(contractIds)},
          {QStringLiteral("capabilities"), modelCapabilities},
          {QStringLiteral("canonical"), canonicalJson},
          {QStringLiteral("hash"), hash}};
}

CapabilityRegistry::CapabilityRegistry() : CapabilityRegistry(builtIn().contracts_) {}

CapabilityRegistry::CapabilityRegistry(QVector<CapabilityContract> contracts) : contracts_(std::move(contracts)) {
  validate();
}

CapabilityRegistry CapabilityRegistry::builtIn() {
  QVector<CapabilityContract> contracts;
  contracts.reserve(20);
  contracts.push_back(makeContract(QStringLiteral("insert_native_component"), {QStringLiteral("playhead")}, {QStringLiteral("resourceId")}));
  contracts.push_back(makeContract(QStringLiteral("insert_media"), {QStringLiteral("playhead")}, {QStringLiteral("mediaId"), QStringLiteral("kind"), QStringLiteral("timelineStart"), QStringLiteral("durationFrames"), QStringLiteral("track")}));
  contracts.push_back(makeContract(QStringLiteral("insert_text"), {QStringLiteral("playhead")}, {QStringLiteral("text"), QStringLiteral("props"), QStringLiteral("timelineStart"), QStringLiteral("durationFrames"), QStringLiteral("track")}));
  contracts.push_back(makeContract(QStringLiteral("insert_subtitle"), {QStringLiteral("playhead")}, {QStringLiteral("text"), QStringLiteral("props"), QStringLiteral("timelineStart"), QStringLiteral("durationFrames"), QStringLiteral("track")}));
  contracts.push_back(makeContract(QStringLiteral("set_component_props"), {QStringLiteral("selected_component")}, {QStringLiteral("targetId"), QStringLiteral("props"), QStringLiteral("property"), QStringLiteral("value")}));
  contracts.push_back(makeContract(QStringLiteral("set_clip_props"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("props")}));
  contracts.push_back(makeContract(QStringLiteral("set_audio_props"), {QStringLiteral("selected_audio")}, {QStringLiteral("targetId"), QStringLiteral("props")}));
  contracts.push_back(makeContract(QStringLiteral("move_clip"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("timelineStart"), QStringLiteral("track")}));
  contracts.push_back(makeContract(QStringLiteral("resize_clip"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("durationFrames")}));
  contracts.push_back(makeContract(QStringLiteral("trim_head"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("atFrame")}));
  contracts.push_back(makeContract(QStringLiteral("trim_tail"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("atFrame")}));
  contracts.push_back(makeContract(QStringLiteral("split_clip"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("atFrame")}));
  contracts.push_back(makeContract(QStringLiteral("set_speed"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("speed")}));
  contracts.push_back(makeContract(QStringLiteral("duplicate_clip"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("timelineStart"), QStringLiteral("track")}));
  contracts.push_back(makeContract(QStringLiteral("replace_source"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId"), QStringLiteral("mediaId")}, QStringLiteral("media.write")));
  contracts.push_back(makeContract(QStringLiteral("remove_clip"), {QStringLiteral("selected_clip")}, {QStringLiteral("targetId")}));
  contracts.push_back(makeContract(QStringLiteral("create_marker"), {QStringLiteral("timeline"), QStringLiteral("selected_clip")}, {QStringLiteral("timelineFrame"), QStringLiteral("label"), QStringLiteral("color"), QStringLiteral("clipId"), QStringLiteral("localFrame")}, QStringLiteral("marker.write")));
  contracts.push_back(makeContract(QStringLiteral("delete_marker"), {QStringLiteral("marker")}, {QStringLiteral("markerId")}, QStringLiteral("marker.write")));
  contracts.push_back(makeContract(QStringLiteral("set_marker_color"), {QStringLiteral("marker")}, {QStringLiteral("markerId"), QStringLiteral("color")}, QStringLiteral("marker.write")));
  contracts.push_back(makeContract(QStringLiteral("close_gap"), {QStringLiteral("track")}, {QStringLiteral("track"), QStringLiteral("startFrame"), QStringLiteral("endFrame")}));
  return CapabilityRegistry(std::move(contracts));
}

void CapabilityRegistry::validate() {
  validationError_.clear();
  QHash<QString, QString> seen;
  for (const auto& contract : contracts_) {
    if (contract.id.isEmpty() || contract.version.isEmpty()) {
      validationError_ = QStringLiteral("capability id and version are required");
      return;
    }
    if (seen.contains(contract.id)) {
      validationError_ = QStringLiteral("duplicate capability id: %1").arg(contract.id);
      return;
    }
    seen.insert(contract.id, contract.version);
    if (contract.executor.isEmpty()) {
      validationError_ = QStringLiteral("capability %1 has no executor").arg(contract.id);
      return;
    }
    if (contract.verificationAdapter.isEmpty()) {
      validationError_ = QStringLiteral("capability %1 has no verification adapter").arg(contract.id);
      return;
    }
    if (contract.inputSchema.isEmpty() || contract.targetTypes.isEmpty()) {
      validationError_ = QStringLiteral("capability %1 has incomplete contract").arg(contract.id);
      return;
    }
    const auto& task = contract.taskPolicy;
    const auto has = [&](const QString& key) { return task.contains(key); };
    if (!has(QStringLiteral("mode")) || !task.value(QStringLiteral("mode")).isString() ||
        !has(QStringLiteral("resourceClass")) || !task.value(QStringLiteral("resourceClass")).isString() ||
        !has(QStringLiteral("priority")) || !task.value(QStringLiteral("priority")).isDouble() ||
        task.value(QStringLiteral("priority")).toInt(-1) < 0 || task.value(QStringLiteral("priority")).toInt() > 100 ||
        !has(QStringLiteral("cancellableUntil")) || !task.value(QStringLiteral("cancellableUntil")).isString() ||
        !has(QStringLiteral("resumable")) || !task.value(QStringLiteral("resumable")).isBool() ||
        !has(QStringLiteral("maxConcurrency")) || !task.value(QStringLiteral("maxConcurrency")).isDouble() || task.value(QStringLiteral("maxConcurrency")).toInt(0) < 1 ||
        !has(QStringLiteral("diskReservation")) || !task.value(QStringLiteral("diskReservation")).isDouble() || task.value(QStringLiteral("diskReservation")).toDouble(-1) < 0 ||
        !has(QStringLiteral("progressAdapter")) || !task.value(QStringLiteral("progressAdapter")).isString()) {
      validationError_ = QStringLiteral("capability %1 has invalid task policy").arg(contract.id);
      return;
    }
  }
  for (const auto& contract : contracts_) {
    for (const auto& replacement : contract.replacementOf) {
      if (!seen.contains(replacement)) {
        validationError_ = QStringLiteral("capability %1 replaces unknown capability %2").arg(contract.id, replacement);
        return;
      }
    }
  }
  QHash<QString, int> state;
  std::function<bool(const QString&)> visit = [&](const QString& id) {
    if (state.value(id) == 1) return false;
    if (state.value(id) == 2) return true;
    state.insert(id, 1);
    const auto it = std::find_if(contracts_.cbegin(), contracts_.cend(), [&](const auto& c) { return c.id == id; });
    if (it != contracts_.cend()) {
      for (const auto& dependency : it->replacementOf) if (!visit(dependency)) return false;
    }
    state.insert(id, 2);
    return true;
  };
  for (const auto& contract : contracts_) {
    if (!visit(contract.id)) {
      validationError_ = QStringLiteral("replacement cycle detected");
      return;
    }
  }
}

CapabilitySnapshot CapabilityRegistry::snapshot(const RuntimeFacts& facts) const {
  CapabilitySnapshot snapshot;
  snapshot.version = facts.registryVersion;
  if (facts.registryVersion != 1) {
    snapshot.error = QStringLiteral("unsupported capability registry version: %1").arg(facts.registryVersion);
    return snapshot;
  }
  if (!isValid()) {
    snapshot.error = validationError_;
    return snapshot;
  }
  const auto executors = sorted(facts.executorIds);
  const auto adapters = sorted(facts.verificationAdapterIds);
  const auto targetTypes = sorted(facts.targetTypes);
  if (targetTypes.isEmpty()) {
    snapshot.error = QStringLiteral("runtime has no target types");
    return snapshot;
  }
  QJsonArray view;
  QStringList ids;
  auto contracts = contracts_;
  std::sort(contracts.begin(), contracts.end(), [](const auto& left, const auto& right) {
    return left.id == right.id ? left.version < right.version : left.id < right.id;
  });
  for (const auto& contract : contracts) {
    if (!executors.contains(contract.executor)) {
      snapshot.error = QStringLiteral("missing executor: %1").arg(contract.executor);
      return snapshot;
    }
    if (!adapters.contains(contract.verificationAdapter)) {
      snapshot.error = QStringLiteral("missing verification adapter: %1").arg(contract.verificationAdapter);
      return snapshot;
    }
    for (const auto& target : contract.targetTypes) {
      if (!targetTypes.contains(target)) {
        snapshot.error = QStringLiteral("missing target type: %1").arg(target);
        return snapshot;
      }
    }
    view.append(contract.toJson());
    ids.push_back(contract.id + QStringLiteral("@") + contract.version);
  }
  snapshot.modelCapabilities = view;
  snapshot.contractIds = sorted(ids);
  snapshot.enabledIds = snapshot.contractIds;
  const auto canonicalText = QStringLiteral("orbit.capability-snapshot.v1|%1|%2").arg(snapshot.version).arg(snapshot.contractIds.join(QStringLiteral(",")));
  snapshot.canonicalJson = QJsonObject{{QStringLiteral("schemaVersion"), snapshot.schemaVersion},
                                       {QStringLiteral("version"), snapshot.version},
                                       {QStringLiteral("contractIds"), QJsonArray::fromStringList(snapshot.contractIds)}};
  snapshot.hash = QStringLiteral("fnv1a64:%1").arg(fnv1a64(canonicalText.toUtf8()));
  snapshot.valid = true;
  return snapshot;
}

QJsonArray CapabilityRegistry::modelView(const CapabilitySnapshot& snapshot) const {
  return snapshot.valid ? snapshot.modelCapabilities : QJsonArray{};
}

std::optional<CapabilityContract> CapabilityRegistry::find(QStringView id, QStringView version) const {
  const auto it = std::find_if(contracts_.cbegin(), contracts_.cend(), [&](const auto& contract) {
    return QStringView{contract.id} == id && QStringView{contract.version} == version;
  });
  if (it == contracts_.cend()) return std::nullopt;
  return *it;
}

}  // namespace edward::ai
