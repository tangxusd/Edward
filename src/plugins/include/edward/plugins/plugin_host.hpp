#pragma once

#include "edward/plugins/plugin_manifest.hpp"
#include "edward/core/component_ir.hpp"

#include <QJsonObject>
#include <QString>
#include <QByteArray>
#include <QImage>

#include <filesystem>

namespace edward::plugins {

struct PluginRequest final {
  QString taskId;
  QString pluginId;
  QString pluginVersion;
  QString operation;
  QString inputPath;
  QString outputPath;
  qint64 maxBytes = 0;
};

bool validateRequest(const PluginManifest& manifest,
                    const PluginRequest& request,
                    const std::filesystem::path& taskRoot,
                    QString* error = nullptr);

bool validateRpcMethod(const PluginManifest& manifest, const QString& method, QString* error = nullptr);
bool validateRpcParams(const QString& method, const QJsonObject& params, QString* error = nullptr);
struct RpcResponse;
std::optional<edward::core::ComponentIr> parseDescribeResult(const PluginManifest& manifest,
                                                             const QJsonObject& result,
                                                             QString* error = nullptr);
std::optional<QImage> parseRenderFrameResult(const QJsonObject& result,
                                             int expectedFrame,
                                             const QSize& expectedSize,
                                             QString* error = nullptr);
std::optional<QImage> parseRenderFrameResponse(const RpcResponse& response,
                                               const QString& requestId,
                                               int expectedFrame,
                                               const QSize& expectedSize,
                                               QString* error = nullptr);

struct RenderExportResult final {
  QString outputPath;
  int frameCount = 0;
  QSize size;
  bool hasAlpha = false;
};

std::optional<RenderExportResult> parseRenderExportResult(const QJsonObject& result,
                                                          QString* error = nullptr);

struct RpcRequest final {
  QString id;
  QString method;
  QJsonObject params;

  static std::optional<RpcRequest> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] QJsonObject toJson() const;
};

struct RpcResponse final {
  QString id;
  QJsonObject result;
  QJsonObject error;

  static std::optional<RpcResponse> parse(const QJsonObject& object, QString* error = nullptr);
};

struct ProcessResult final {
  bool started = false;
  bool timedOut = false;
  int exitCode = -1;
  QByteArray standardOutput;
  QByteArray standardError;
};

ProcessResult launchPluginProcess(const PluginManifest& manifest,
                                  const std::filesystem::path& pluginRoot,
                                  const QStringList& arguments,
                                  int timeoutMs);

std::optional<RpcResponse> callPlugin(const PluginManifest& manifest,
                                      const std::filesystem::path& pluginRoot,
                                      const RpcRequest& request,
                                      int timeoutMs,
                                      QString* error = nullptr);

std::optional<QImage> renderPluginFrame(const PluginManifest& manifest,
                                        const std::filesystem::path& pluginRoot,
                                        const QString& requestId,
                                        const QString& compositionId,
                                        int frame,
                                        const QSize& size,
                                        int timeoutMs,
                                        QString* error = nullptr);

std::optional<RenderExportResult> exportPlugin(const PluginManifest& manifest,
                                               const std::filesystem::path& pluginRoot,
                                               const QString& requestId,
                                               const QString& compositionId,
                                               const QString& outputPath,
                                               const QSize& size,
                                               int timeoutMs,
                                               QString* error = nullptr);

}  // namespace edward::plugins
