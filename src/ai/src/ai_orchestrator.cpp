#include "edward/ai/ai_orchestrator.hpp"
#include <QJsonDocument>

namespace edward::ai {

namespace {
QJsonObject parseObject(const QString& output, QString* error) {
  const auto trimmed = output.trimmed();
  QString json = trimmed;
  if (trimmed.startsWith(QStringLiteral("```"))) {
    const auto firstLineEnd = trimmed.indexOf('\n');
    const auto closingFence = trimmed.lastIndexOf(QStringLiteral("```"));
    if (firstLineEnd > 0 && closingFence > firstLineEnd)
      json = trimmed.mid(firstLineEnd + 1, closingFence - firstLineEnd - 1).trimmed();
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (!document.isObject()) {
    if (error) *error = QStringLiteral("模型输出不是 JSON 对象");
    return {};
  }
  return document.object();
}
}

AiResult AiOrchestrator::handle(const QString& output, const ProjectSnapshot& project) const {
  const auto trimmed = output.trimmed();
  QString json = trimmed;
  if (trimmed.startsWith(QStringLiteral("```"))) {
    const auto firstLineEnd = trimmed.indexOf('\n');
    const auto closingFence = trimmed.lastIndexOf(QStringLiteral("```"));
    if (firstLineEnd > 0 && closingFence > firstLineEnd)
      json = trimmed.mid(firstLineEnd + 1, closingFence - firstLineEnd - 1).trimmed();
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (!document.isObject()) return {AiResult::Kind::Conversation, trimmed, {}};
  const auto actionObject = document.object();
  if (actionObject.value(QStringLiteral("schemaVersion")).toString() == QStringLiteral("orbit.bound-action-plan.v2"))
    return {AiResult::Kind::Clarification, QStringLiteral("BoundActionPlan 只能由解析器绑定后执行。"), {}};
  QString error;
  const auto plan = ActionPlan::parse(actionObject, &error);
  if (!plan) {
    if (document.object().contains("react") || document.object().contains("css") || document.object().contains("root"))
      return {AiResult::Kind::Unsupported, QStringLiteral("此组件载荷不符合 Orbit 原生运行时协议。"), {}};
    return {AiResult::Kind::Clarification, QStringLiteral("无法确认该项目修改，请明确目标、资源或位置。"), {}};
  }
  if (!plan->validate(project, &error)) return {AiResult::Kind::Clarification, error, {}};
  return {AiResult::Kind::ActionPlan, {}, plan};
}

AiResult AiOrchestrator::handle(const QString& output, const ProjectSnapshot& project,
                                const ReferenceSnapshot& references,
                                const CapabilitySnapshot& capabilities) const {
  const auto trimmed = output.trimmed();
  QString parseError;
  const auto object = parseObject(trimmed, &parseError);
  if (object.isEmpty()) return {AiResult::Kind::Conversation, trimmed, {}};
  if (object.value(QStringLiteral("schemaVersion")).toString() != QStringLiteral("edward.intent-plan.v1"))
    return handle(output, project);

  CapabilityView view;
  for (const auto& capability : capabilities.modelCapabilities)
    view.capabilityIds.append(capability.toObject().value(QStringLiteral("id")).toString());
  QString error;
  const auto intent = IntentPlan::parse(object, &error);
  if (!intent || !intent->validate(view, &error))
    return {AiResult::Kind::Clarification, error.isEmpty() ? QStringLiteral("IntentPlan 无法验证") : error, {}};
  const auto resolved = IntentResolver{}.resolve(*intent, references, capabilities);
  if (!resolved.succeeded())
    return {AiResult::Kind::Clarification, resolved.error ? resolved.error->detail : QStringLiteral("无法绑定编辑目标"), {}};
  auto plan = *resolved.plan;
  if (!plan.validate(project, &error))
    return {AiResult::Kind::Clarification, error, {}};
  return {AiResult::Kind::ActionPlan, {}, plan};
}

}  // namespace edward::ai
