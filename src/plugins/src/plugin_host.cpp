#include "edward/plugins/plugin_host.hpp"

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

}  // namespace edward::plugins
