#include <edward/desktop/workbench_runtime.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir settings;
  QTemporaryDir exportDirectory;
  assert(settings.isValid());
  assert(exportDirectory.isValid());
  qputenv("EDWARD_SETTINGS_PATH", settings.filePath("settings.ini").toUtf8());
  qputenv("EDWARD_DIAGNOSTIC_LOG_PATH", settings.filePath("edward.log").toUtf8());
  edward::desktop::WorkbenchRuntime runtime;
  const auto before = runtime.preferenceStoreStatus();
  assert(before.contains("pending"));
  assert(runtime.flushPreferencesForProjectClose());
  assert(runtime.flushPreferencesForExport());
  assert(runtime.compilePreferencesNow());
  assert(runtime.preferenceStoreStatus().value("pending").toLongLong() == 0);
  assert(runtime.preferenceStore() != nullptr);
  const auto defaultSettingsSnapshot = runtime.fablecutSettings();
  assert(defaultSettingsSnapshot.value("provider") == "DeepSeek");
  assert(defaultSettingsSnapshot.value("providerId") == "deepseek");
  assert(defaultSettingsSnapshot.value("endpoint") == "https://api.deepseek.com/chat/completions");
  assert(defaultSettingsSnapshot.value("model") == "deepseek-chat");
  assert(runtime.saveFablecutSettings("deepseek", "DeepSeek", "https://api.deepseek.com/v1",
                                      "", "deepseek-chat", "openai-completions", exportDirectory.path()));
  assert(runtime.fablecutSettings().value("endpoint") == "https://api.deepseek.com/chat/completions");
  assert(runtime.saveFablecutSettings("openai", "OpenAI", "https://api.example.test/v1/chat/completions",
                                      "secret-key", "gpt-4.1", "openai-completions", exportDirectory.path()));
  const auto settingsSnapshot = runtime.fablecutSettings();
  assert(settingsSnapshot.value("provider") == "OpenAI");
  assert(settingsSnapshot.value("providerId") == "openai");
  assert(settingsSnapshot.value("endpoint") == "https://api.example.test/v1/chat/completions");
  assert(settingsSnapshot.value("model") == "gpt-4.1");
  assert(settingsSnapshot.value("exportDirectory") == exportDirectory.path());
  assert(!settingsSnapshot.contains("apiKey"));
  assert(!settingsSnapshot.contains("preferenceDatabase"));
  assert(settingsSnapshot.value("preferenceStorage") == "本地偏好存储已启用");
  assert(runtime.testFablecutAiProvider("https://api.example.test/v1/chat/completions", "", "gpt-4.1", "openai-completions"));
  assert(!runtime.saveFablecutSettings("openai", "OpenAI", "http://api.example.test", "", "gpt-4.1", "openai-completions", exportDirectory.path()));

  QTemporaryDir projectDirectory;
  QTemporaryDir migrationTarget;
  assert(projectDirectory.isValid());
  assert(migrationTarget.isValid());
  QFile projectFile(projectDirectory.filePath("project.json"));
  assert(projectFile.open(QIODevice::WriteOnly));
  assert(projectFile.write("{\"revision\":1}") > 0);
  projectFile.close();
  const QVariantMap pathSettings{{"projectRoot", projectDirectory.path()}, {"cacheRoot", settings.filePath("cache")},
                                 {"exportRoot", exportDirectory.path()}, {"componentDownloadRoot", settings.filePath("components")},
                                 {"mediaDownloadRoot", settings.filePath("media")}, {"pluginDownloadRoot", settings.filePath("plugins/downloads")},
                                 {"pluginRuntimeRoot", settings.filePath("plugins/runtime")}, {"proxyRoot", settings.filePath("proxies")},
                                 {"prerenderRoot", settings.filePath("prerenders")}};
  assert(runtime.saveFablecutPathSettings(pathSettings));
  assert(runtime.migrateFablecutPath("projectRoot", migrationTarget.path()));
  const auto migratedPaths = runtime.fablecutSettings().value("paths").toMap();
  assert(migratedPaths.value("projectRoot") == migrationTarget.path());
  assert(QFile::exists(migrationTarget.filePath("project.json")));
  assert(!QFile::exists(projectDirectory.filePath("project.json")));
  assert(!runtime.clearDerivedFablecutPath("projectRoot"));
  const auto preferencePackage = settings.filePath("my-preferences.edwardprefs");
  assert(runtime.exportFablecutPreferences(preferencePackage));
  assert(!runtime.exportFablecutPreferences(settings.filePath("my-preferences.sqlite")));
  assert(!runtime.importFablecutPreferences(settings.filePath("my-preferences.db")));
  QFile diagnosticLog(settings.filePath("edward.log"));
  assert(diagnosticLog.open(QIODevice::WriteOnly | QIODevice::Append));
  diagnosticLog.write("Authorization: Bearer test-token /Users/example/private-project\\n");
  diagnosticLog.close();
  const auto diagnostics = runtime.fablecutDiagnostics();
  assert(diagnostics.value("log").toString().contains("diagnostic_snapshot_requested"));
  assert(!diagnostics.value("log").toString().contains("test-token"));
  assert(!diagnostics.value("log").toString().contains("Authorization"));
  assert(!diagnostics.value("log").toString().contains("/Users/example"));
  assert(runtime.recordFablecutDiagnostic("script_error detail=unexpected_state"));
  assert(runtime.fablecutDiagnostics().value("log").toString().contains("fablecut script_error"));
  assert(!runtime.recordFablecutDiagnostic(QString(513, 'x')));
  return 0;
}
