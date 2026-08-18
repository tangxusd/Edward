#include "edward/plugins/plugin_host.hpp"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QBuffer>

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

bool validateRpcMethod(const PluginManifest& manifest, const QString& method, QString* error) {
  const QString capability = method == "describe" ? "describe" :
                             method == "renderFrame" ? "renderFrame" :
                             method == "renderExport" ? "renderExport" : QString{};
  if (capability.isEmpty() || !manifest.capabilities.contains(capability)) {
    if (error) *error = QStringLiteral("rpc method is not declared by plugin");
    return false;
  }
  return true;
}

bool validateRpcParams(const QString& method, const QJsonObject& params, QString* error) {
  const auto positiveInt = [&params](const char* key) {
    return params.value(QLatin1String(key)).isDouble() && params.value(QLatin1String(key)).toInt() > 0;
  };
  if (method == "describe") {
    if (!params.value("compositionId").isString() || params.value("compositionId").toString().isEmpty()) {
      if (error) *error = QStringLiteral("describe requires compositionId");
      return false;
    }
    return true;
  }
  if (method == "renderFrame") {
    if (!params.value("frame").isDouble() || params.value("frame").toInt() < 0 ||
        !positiveInt("width") || !positiveInt("height")) {
      if (error) *error = QStringLiteral("renderFrame requires non-negative frame and positive width/height");
      return false;
    }
    return true;
  }
  if (method == "renderExport") {
    const auto output = params.value("outputPath").toString();
    if (output.isEmpty() || output.startsWith('/') || output.contains("..") || output.contains("\\") ||
        !positiveInt("width") || !positiveInt("height")) {
      if (error) *error = QStringLiteral("renderExport requires safe outputPath and positive width/height");
      return false;
    }
    return true;
  }
  if (error) *error = QStringLiteral("rpc method is unknown");
  return false;
}

std::optional<edward::core::ComponentIr> parseDescribeResult(const PluginManifest& manifest,
                                                             const QJsonObject& result,
                                                             QString* error) {
  if (!result.value("compositionId").isString() || result.value("compositionId").toString().isEmpty()) {
    if (error) *error = QStringLiteral("describe result requires compositionId");
    return std::nullopt;
  }
  const auto component = result.value("component");
  if (!component.isObject()) {
    if (error) *error = QStringLiteral("describe result requires component object");
    return std::nullopt;
  }
  const auto editable = result.value("editableProps");
  if (!editable.isUndefined()) {
    if (!editable.isArray()) {
      if (error) *error = QStringLiteral("editableProps must be an array");
      return std::nullopt;
    }
    for (const auto& value : editable.toArray()) {
      if (!value.isString() || !manifest.editableProps.contains(value.toString())) {
        if (error) *error = QStringLiteral("describe result contains undeclared editable property");
        return std::nullopt;
      }
    }
  }
  auto parsed = edward::core::ComponentIr::parse(component.toObject());
  if (!parsed && error) *error = QStringLiteral("describe result component IR is invalid");
  return parsed;
}

std::optional<QImage> parseRenderFrameResult(const QJsonObject& result,
                                             int expectedFrame,
                                             const QSize& expectedSize,
                                             QString* error) {
  if (expectedFrame < 0 || expectedSize.isEmpty() || result.value("frame").toInt(-1) != expectedFrame) {
    if (error) *error = QStringLiteral("renderFrame response does not match requested frame");
    return std::nullopt;
  }
  const auto encoded = result.value("pngBase64").toString().toUtf8();
  if (encoded.isEmpty()) {
    if (error) *error = QStringLiteral("renderFrame response is missing pngBase64");
    return std::nullopt;
  }
  const auto bytes = QByteArray::fromBase64(encoded);
  QImage image;
  if (!image.loadFromData(bytes, "PNG") || image.size() != expectedSize || !image.hasAlphaChannel()) {
    if (error) *error = QStringLiteral("renderFrame response must be a PNG with matching size and alpha");
    return std::nullopt;
  }
  return image.convertToFormat(QImage::Format_RGBA8888);
}

std::optional<QImage> parseRenderFrameResponse(const RpcResponse& response,
                                               const QString& requestId,
                                               int expectedFrame,
                                               const QSize& expectedSize,
                                               QString* error) {
  if (requestId.isEmpty() || response.id != requestId || response.result.isEmpty()) {
    if (error) *error = QStringLiteral("renderFrame rpc response is not a matching success response");
    return std::nullopt;
  }
  return parseRenderFrameResult(response.result, expectedFrame, expectedSize, error);
}

std::optional<RenderExportResult> parseRenderExportResult(const QJsonObject& result, QString* error) {
  const auto outputPath = result.value("outputPath").toString();
  const auto width = result.value("width");
  const auto height = result.value("height");
  const auto frameCount = result.value("frameCount");
  const auto hasAlpha = result.value("hasAlpha");
  if (!safeRelative(outputPath) || !width.isDouble() || !height.isDouble() ||
      !frameCount.isDouble() || !hasAlpha.isBool() || width.toInt() <= 0 ||
      height.toInt() <= 0 || frameCount.toInt() <= 0) {
    if (error) *error = QStringLiteral("renderExport result requires safe outputPath, positive size/frameCount and hasAlpha");
    return std::nullopt;
  }
  return RenderExportResult{outputPath, frameCount.toInt(), QSize(width.toInt(), height.toInt()), hasAlpha.toBool()};
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

std::optional<RpcResponse> RpcResponse::parse(const QJsonObject& object, QString* error) {
  const auto id = object.value("id").toString();
  const auto result = object.value("result");
  const auto failure = object.value("error");
  if (id.isEmpty() || (result.isUndefined() && failure.isUndefined()) ||
      (!result.isUndefined() && !result.isObject()) || (!failure.isUndefined() && !failure.isObject())) {
    if (error) *error = QStringLiteral("rpc response requires id and result or error object");
    return std::nullopt;
  }
  return RpcResponse{id, result.toObject(), failure.toObject()};
}

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

std::optional<RpcResponse> callPlugin(const PluginManifest& manifest,
                                      const std::filesystem::path& pluginRoot,
                                      const RpcRequest& request,
                                      int timeoutMs,
                                      QString* error) {
  if (timeoutMs <= 0) {
    if (error) *error = QStringLiteral("rpc timeout must be positive");
    return std::nullopt;
  }
  if (!validateRpcMethod(manifest, request.method, error)) return std::nullopt;
  if (!validateRpcParams(request.method, request.params, error)) return std::nullopt;
  const auto executable = pluginRoot / manifest.entry.toStdString();
  if (!std::filesystem::is_regular_file(executable)) {
    if (error) *error = QStringLiteral("plugin entry is unavailable");
    return std::nullopt;
  }
  QProcess process;
  process.setProgram(QString::fromStdString(executable.string()));
  process.start();
  if (!process.waitForStarted(1000)) {
    if (error) *error = process.errorString();
    return std::nullopt;
  }
  process.write(QJsonDocument(request.toJson()).toJson(QJsonDocument::Compact) + '\n');
  process.closeWriteChannel();
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished(1000);
    if (error) *error = QStringLiteral("plugin rpc timed out");
    return std::nullopt;
  }
  const auto response = QJsonDocument::fromJson(process.readAllStandardOutput().trimmed());
  if (!response.isObject()) {
    if (error) *error = QStringLiteral("plugin returned invalid rpc response");
    return std::nullopt;
  }
  auto parsed = RpcResponse::parse(response.object(), error);
  if (!parsed) return std::nullopt;
  if (parsed->id != request.id) {
    if (error) *error = QStringLiteral("plugin rpc response id mismatch");
    return std::nullopt;
  }
  return parsed;
}

std::optional<QImage> renderPluginFrame(const PluginManifest& manifest,
                                        const std::filesystem::path& pluginRoot,
                                        const QString& requestId,
                                        const QString& compositionId,
                                        int frame,
                                        const QSize& size,
                                        int timeoutMs,
                                        QString* error) {
  if (requestId.isEmpty() || compositionId.isEmpty() || frame < 0 || size.isEmpty()) {
    if (error) *error = QStringLiteral("renderFrame request is incomplete");
    return std::nullopt;
  }
  const RpcRequest request{requestId, QStringLiteral("renderFrame"),
                           {{"compositionId", compositionId}, {"frame", frame},
                            {"width", size.width()}, {"height", size.height()}}};
  const auto response = callPlugin(manifest, pluginRoot, request, timeoutMs, error);
  if (!response) return std::nullopt;
  return parseRenderFrameResponse(*response, requestId, frame, size, error);
}

}  // namespace edward::plugins
