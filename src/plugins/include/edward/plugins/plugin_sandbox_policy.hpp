#pragma once

#include <QString>

namespace edward::plugins {

struct PluginSandboxPaths final {
  QString pluginRoot;
  QString runtimePath;
  QString inputRoot;
  QString outputRoot;
  QString chromiumCacheRoot;
  QString chromiumTempRoot;
};

QString buildMacosSeatbeltProfile(const PluginSandboxPaths& paths);

}  // namespace edward::plugins
