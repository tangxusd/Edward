#include "edward/ai/intent_plan.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace edward::ai {
namespace {

constexpr auto kSchemaVersion = "edward.intent-plan.v1";
constexpr qsizetype kDefaultMaxIntents = 32;
constexpr qsizetype kDefaultMaxPlanBytes = 256 * 1024;

void fail(QString* error, const QString& code, const QString& detail) {
  if (error) *error = code + QStringLiteral(": ") + detail;
}

bool nonEmptyString(const QJsonValue& value) { return value.isString() && !value.toString().isEmpty(); }

bool looksLikeLocalPath(const QString& value) {
  return value.startsWith('/') || value.startsWith('~') || value.startsWith(QStringLiteral("\\\\")) ||
         value.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive) ||
         QRegularExpression(QStringLiteral("^[A-Za-z]:[\\\\/]")).match(value).hasMatch() ||
         QRegularExpression(QStringLiteral("(?:^|[\\s(:])/(?:Users|tmp|var|private|Volumes|home)/"))
             .match(value)
             .hasMatch();
}

bool forbiddenKey(const QString& key) {
  const auto normalized = key.toLower();
  static const QSet<QString> forbidden = {
      QStringLiteral("targetid"), QStringLiteral("clipid"), QStringLiteral("markerid"),
      QStringLiteral("trackid"), QStringLiteral("resourceid"), QStringLiteral("operationid"),
      QStringLiteral("projectid"), QStringLiteral("projectrevision"), QStringLiteral("baserevision"),
      QStringLiteral("baseprojectrevision"), QStringLiteral("revision"), QStringLiteral("durationframes"),
      QStringLiteral("framecount"), QStringLiteral("localpath"), QStringLiteral("sourcepath"),
      QStringLiteral("filepath"), QStringLiteral("absolutepath"), QStringLiteral("path"),
      QStringLiteral("fps")};
  return forbidden.contains(normalized);
}

bool safeData(const QJsonValue& value, QString* detail, const QString& location) {
  if (value.isString()) {
    if (looksLikeLocalPath(value.toString())) {
      if (detail) *detail = QStringLiteral("local path at %1").arg(location);
      return false;
    }
    return true;
  }
  if (value.isArray()) {
    const auto array = value.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
      if (!safeData(array.at(index), detail, QStringLiteral("%1[%2]").arg(location).arg(index))) return false;
    }
    return true;
  }
  if (value.isObject()) {
    const auto object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      if (forbiddenKey(it.key())) {
        if (detail) *detail = QStringLiteral("forbidden field '%1' at %2").arg(it.key(), location);
        return false;
      }
      if (!safeData(it.value(), detail, QStringLiteral("%1.%2").arg(location, it.key()))) return false;
    }
  }
  return true;
}

bool validSelector(const QJsonObject& reference) {
  const auto selectorValue = reference.value(QStringLiteral("selector"));
  if (!nonEmptyString(selectorValue)) return false;
  const auto selector = selectorValue.toString();
  static const QSet<QString> simpleSelectors = {
      QStringLiteral("selected_clip"), QStringLiteral("selected_clips"), QStringLiteral("playhead_clip"),
      QStringLiteral("selected_track"), QStringLiteral("current_track"), QStringLiteral("timeline"),
      QStringLiteral("selected_media"), QStringLiteral("selected_component"), QStringLiteral("execution_selection")};
  if (simpleSelectors.contains(selector)) return true;
  if (selector == QStringLiteral("global_marker")) {
    const auto label = reference.value(QStringLiteral("label"));
    const auto index = reference.value(QStringLiteral("index"));
    return (label.isDouble() && label.toInteger(-1) >= 0) || (index.isDouble() && index.toInteger(-1) >= 0);
  }
  return QRegularExpression(QStringLiteral("^global_marker\\([0-9]+\\)$")).match(selector).hasMatch() ||
         QRegularExpression(QStringLiteral("^clip_marker\\([0-9]+\\)$")).match(selector).hasMatch();
}

bool validReference(const QJsonValue& value, QString* detail) {
  if (!value.isObject()) {
    if (detail) *detail = QStringLiteral("targetRef must be an object");
    return false;
  }
  const auto reference = value.toObject();
  static const QSet<QString> allowed = {QStringLiteral("selector"), QStringLiteral("label"), QStringLiteral("index")};
  for (auto it = reference.begin(); it != reference.end(); ++it) {
    if (!allowed.contains(it.key())) {
      if (detail) *detail = QStringLiteral("targetRef field '%1' is not allowed").arg(it.key());
      return false;
    }
  }
  if (!validSelector(reference)) {
    if (detail) *detail = QStringLiteral("targetRef selector is not a supported symbolic reference");
    return false;
  }
  for (const auto& key : {QStringLiteral("label"), QStringLiteral("index")}) {
    if (reference.contains(key) && (!reference.value(key).isDouble() || reference.value(key).toInteger(-1) < 0)) {
      if (detail) *detail = QStringLiteral("targetRef.%1 must be a non-negative integer").arg(key);
      return false;
    }
  }
  return true;
}

bool validIntent(const QJsonValue& value, QString* detail) {
  if (!value.isObject()) {
    if (detail) *detail = QStringLiteral("intent must be an object");
    return false;
  }
  const auto intent = value.toObject();
  static const QSet<QString> allowed = {
      QStringLiteral("intentId"), QStringLiteral("capability"), QStringLiteral("targetRef"),
      QStringLiteral("params"), QStringLiteral("reason")};
  for (auto it = intent.begin(); it != intent.end(); ++it) {
    if (!allowed.contains(it.key())) {
      if (detail) *detail = QStringLiteral("intent field '%1' is not allowed").arg(it.key());
      return false;
    }
  }
  if (!nonEmptyString(intent.value(QStringLiteral("capability")))) {
    if (detail) *detail = QStringLiteral("intent capability is required");
    return false;
  }
  if (!validReference(intent.value(QStringLiteral("targetRef")), detail)) return false;
  if (!intent.value(QStringLiteral("params")).isObject()) {
    if (detail) *detail = QStringLiteral("intent params must be an object");
    return false;
  }
  if (intent.contains(QStringLiteral("intentId")) && !nonEmptyString(intent.value(QStringLiteral("intentId")))) {
    if (detail) *detail = QStringLiteral("intentId must be a non-empty string");
    return false;
  }
  if (intent.contains(QStringLiteral("reason")) && !intent.value(QStringLiteral("reason")).isString()) {
    if (detail) *detail = QStringLiteral("reason must be a string");
    return false;
  }
  if (!safeData(intent.value(QStringLiteral("params")), detail, QStringLiteral("params"))) return false;
  return !intent.contains(QStringLiteral("reason")) ||
         safeData(intent.value(QStringLiteral("reason")), detail, QStringLiteral("reason"));
}

}  // namespace

bool CapabilityView::contains(const QString& capability) const { return capabilityIds.contains(capability); }

std::optional<IntentPlan> IntentPlan::parse(const QJsonObject& object, QString* error) {
  const auto serializedSize = QJsonDocument(object).toJson(QJsonDocument::Compact).size();
  if (serializedSize > kDefaultMaxPlanBytes) {
    fail(error, QStringLiteral("MODEL_OUTPUT_UNTRUSTED"), QStringLiteral("IntentPlan exceeds size limit"));
    return std::nullopt;
  }
  static const QSet<QString> allowed = {QStringLiteral("schemaVersion"), QStringLiteral("requestId"), QStringLiteral("intents")};
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (!allowed.contains(it.key())) {
      fail(error, QStringLiteral("MODEL_OUTPUT_UNTRUSTED"), QStringLiteral("IntentPlan field '%1' is not allowed").arg(it.key()));
      return std::nullopt;
    }
  }
  IntentPlan plan;
  if (object.value(QStringLiteral("schemaVersion")).toString() != QLatin1String(kSchemaVersion) ||
      !nonEmptyString(object.value(QStringLiteral("requestId"))) || !object.value(QStringLiteral("intents")).isArray()) {
    fail(error, QStringLiteral("MODEL_OUTPUT_UNTRUSTED"), QStringLiteral("IntentPlan required fields are invalid"));
    return std::nullopt;
  }
  plan.schemaVersion = object.value(QStringLiteral("schemaVersion")).toString();
  plan.requestId = object.value(QStringLiteral("requestId")).toString();
  plan.intents = object.value(QStringLiteral("intents")).toArray();
  if (plan.intents.isEmpty() || plan.intents.size() > kDefaultMaxIntents) {
    fail(error, QStringLiteral("MODEL_OUTPUT_UNTRUSTED"), QStringLiteral("IntentPlan operation count is outside limits"));
    return std::nullopt;
  }
  for (const auto& value : plan.intents) {
    QString detail;
    if (!validIntent(value, &detail)) {
      fail(error, QStringLiteral("MODEL_OUTPUT_UNTRUSTED"), detail);
      return std::nullopt;
    }
  }
  return plan;
}

bool IntentPlan::validate(const CapabilityView& capabilities, QString* error) const {
  if (schemaVersion != QLatin1String(kSchemaVersion) || requestId.isEmpty() || intents.isEmpty()) {
    fail(error, QStringLiteral("INTENT_INVALID"), QStringLiteral("IntentPlan identity or operation list is invalid"));
    return false;
  }
  const auto maxIntents = capabilities.maxIntents > 0 ? capabilities.maxIntents : kDefaultMaxIntents;
  if (intents.size() > maxIntents) {
    fail(error, QStringLiteral("INTENT_INVALID"), QStringLiteral("IntentPlan operation count exceeds capability limit"));
    return false;
  }
  if (capabilities.maxPlanBytes > 0) {
    const QJsonObject serialized{{QStringLiteral("schemaVersion"), schemaVersion},
                                 {QStringLiteral("requestId"), requestId},
                                 {QStringLiteral("intents"), intents}};
    if (QJsonDocument(serialized).toJson(QJsonDocument::Compact).size() > capabilities.maxPlanBytes) {
      fail(error, QStringLiteral("INTENT_INVALID"), QStringLiteral("IntentPlan exceeds capability size limit"));
      return false;
    }
  }
  QSet<QString> intentIds;
  for (const auto& value : intents) {
    const auto intent = value.toObject();
    const auto capability = intent.value(QStringLiteral("capability")).toString();
    if (!capabilities.contains(capability)) {
      fail(error, QStringLiteral("INTENT_INVALID"), QStringLiteral("unknown capability '%1'").arg(capability));
      return false;
    }
    if (intent.contains(QStringLiteral("intentId"))) {
      const auto intentId = intent.value(QStringLiteral("intentId")).toString();
      if (intentIds.contains(intentId)) {
        fail(error, QStringLiteral("INTENT_INVALID"), QStringLiteral("duplicate intentId"));
        return false;
      }
      intentIds.insert(intentId);
    }
  }
  return true;
}

}  // namespace edward::ai
