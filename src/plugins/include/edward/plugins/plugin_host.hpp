#pragma once

#include "edward/plugins/plugin_manifest.hpp"

#include <QJsonObject>
#include <QString>
#include <QByteArray>

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

struct RpcRequest final {
  QString id;
  QString method;
  QJsonObject params;

  static std::optional<RpcRequest> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] QJsonObject toJson() const;
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

}  // namespace edward::plugins
