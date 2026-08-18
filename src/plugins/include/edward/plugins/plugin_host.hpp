#pragma once

#include "edward/plugins/plugin_manifest.hpp"

#include <QString>

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

}  // namespace edward::plugins
