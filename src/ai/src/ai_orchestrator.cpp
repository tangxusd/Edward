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
  auto actionObject = document.object();
  // revision 是桌面运行时的内部并发校验值，用户和模型都不需要手工填写。
  // 模型未返回该字段时，绑定当前已读取的项目 revision；显式返回的旧值仍然拒绝。
  if (actionObject.value(QStringLiteral("schemaVersion")).toString() == QStringLiteral("edward.action-plan.v1") &&
      !actionObject.value(QStringLiteral("baseProjectRevision")).isDouble()) {
    actionObject.insert(QStringLiteral("baseProjectRevision"), static_cast<double>(project.revision));
  }
  // 常见模型会把操作字段写成 op；协议内部统一为 type，避免把可执行方案误判成普通对话。
  if (actionObject.value(QStringLiteral("schemaVersion")).toString() == QStringLiteral("edward.action-plan.v1") &&
      actionObject.value(QStringLiteral("operations")).isArray()) {
    QJsonArray normalizedOperations;
    for (const auto& value : actionObject.value(QStringLiteral("operations")).toArray()) {
      auto operation = value.toObject();
      if (!operation.contains(QStringLiteral("type")) && operation.value(QStringLiteral("op")).isString()) {
        operation.insert(QStringLiteral("type"), operation.value(QStringLiteral("op")));
        operation.remove(QStringLiteral("op"));
      }
      normalizedOperations.append(operation);
    }
    actionObject.insert(QStringLiteral("operations"), normalizedOperations);
  }
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
