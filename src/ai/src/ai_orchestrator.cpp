#include "edward/ai/ai_orchestrator.hpp"
#include <QJsonDocument>

namespace edward::ai {

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

}  // namespace edward::ai
