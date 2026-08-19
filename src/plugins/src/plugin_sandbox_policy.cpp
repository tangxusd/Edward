#include "edward/plugins/plugin_sandbox_policy.hpp"

namespace edward::plugins {
namespace {

QString quote(const QString& value) {
  QString result = value;
  result.replace('\\', "\\\\");
  result.replace('"', "\\\"");
  return QStringLiteral("\"") + result + QStringLiteral("\"");
}

}  // namespace

QString buildMacosSeatbeltProfile(const PluginSandboxPaths& paths) {
  const auto read = [&paths](const QString& path) {
    return QStringLiteral("(allow file-read* (subpath %1))\n").arg(quote(path));
  };
  const auto write = [&paths](const QString& path) {
    return QStringLiteral("(allow file-write* (subpath %1))\n").arg(quote(path));
  };
  QString profile = QStringLiteral("(version 1)\n(deny default)\n"
                                   "(allow process-exec*)\n"
                                   "(allow signal (target self))\n"
                                   "(allow sysctl-read)\n"
                                   "(allow mach-lookup (global-name \"com.apple.system.logger\"))\n");
  profile += read(paths.pluginRoot) + read(paths.runtimePath) + read(paths.inputRoot) +
             read(paths.chromiumCacheRoot) + read(QStringLiteral("/usr/lib")) +
             read(QStringLiteral("/System/Library"));
  profile += write(paths.outputRoot) + write(paths.chromiumTempRoot);
  return profile;
}

}  // namespace edward::plugins
