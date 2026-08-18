#include "edward/plugins/plugin_host.hpp"

#include <QProcess>

namespace edward::plugins {
namespace {

bool safeRelative(const QString& value) {
  if (value.isEmpty() || value.startsWith('/') || value.contains("\\")) return false;
  const auto path = std::filesystem::path(value.toStdString());
  return !path.is_absolute() && std::find(path.begin(), path.end(), "..") == path.end();
}

bool fail(QString* error, const QString& message) {
  if (error) *error = message;
  return false;
}

}  // namespace

bool validateRequest(const PluginManifest& manifest,
                    const PluginRequest& request,
                    const std::filesystem::path& taskRoot,
                    QString* error) {
  if (taskRoot.empty() || !std::filesystem::exists(taskRoot)) return fail(error, "task root is unavailable");
  if (request.taskId.isEmpty()) return fail(error, "task id is required");
  if (request.pluginId != manifest.pluginId || request.pluginVersion != manifest.version)
    return fail(error, "plugin identity does not match manifest");
  if (!safeRelative(request.inputPath) || !safeRelative(request.outputPath))
    return fail(error, "request paths must be safe relative paths");
  if (request.maxBytes <= 0) return fail(error, "request size limit is required");
  const QString permission = request.operation == "read_input_asset" ? "read_input_asset" :
                             request.operation == "write_draft_output" ? "write_draft_output" :
                             request.operation == "report_progress" ? "report_progress" : QString{};
  if (permission.isEmpty() || !manifest.allows(permission)) return fail(error, "operation is not permitted");
  return true;
}

std::optional<RpcRequest> RpcRequest::parse(const QJsonObject& object, QString* error) {
  const auto id = object.value("id").toString();
  const auto method = object.value("method").toString();
  const auto params = object.value("params");
  if (id.isEmpty() || method.isEmpty() || !params.isObject()) {
    if (error) *error = QStringLiteral("rpc request requires id, method and object params");
    return std::nullopt;
  }
  return RpcRequest{id, method, params.toObject()};
}

QJsonObject RpcRequest::toJson() const { return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}; }

ProcessResult launchPluginProcess(const PluginManifest& manifest,
                                  const std::filesystem::path& pluginRoot,
                                  const QStringList& arguments,
                                  int timeoutMs) {
  ProcessResult result;
  if (timeoutMs <= 0 || pluginRoot.empty()) return result;
  const auto executable = pluginRoot / manifest.entry.toStdString();
  if (!std::filesystem::is_regular_file(executable)) return result;
  QProcess process;
  process.setProgram(QString::fromStdString(executable.string()));
  process.setArguments(arguments);
  process.start();
  result.started = process.waitForStarted(1000);
  if (!result.started) {
    result.standardError = process.errorString().toUtf8();
    return result;
  }
  if (!process.waitForFinished(timeoutMs)) {
    result.timedOut = true;
    process.kill();
    process.waitForFinished(1000);
  }
  result.exitCode = process.exitCode();
  result.standardOutput = process.readAllStandardOutput();
  result.standardError = process.readAllStandardError();
  return result;
}

}  // namespace edward::plugins
