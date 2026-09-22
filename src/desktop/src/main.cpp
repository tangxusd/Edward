#include "edward/desktop/workbench_runtime.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QUrl>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QWindow>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtWebEngineQuick>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#include <QtWebEngineCore/QWebEngineScript>

#ifdef Q_OS_MACOS
void installEdwardTitlebar(QWindow *window, bool localServiceStarted);
void updateEdwardTitlebarAuthState(QWindow *window, bool authenticated);
#endif

class EdwardFrameProvider final : public QQuickImageProvider {
 public:
  explicit EdwardFrameProvider(const edward::desktop::WorkbenchRuntime& runtime)
      : QQuickImageProvider(QQuickImageProvider::Image), runtime_(runtime) {}
  QImage requestImage(const QString& id, QSize* size, const QSize&) override {
    bool clipIdOk = false;
    const auto clipId = id.startsWith(QStringLiteral("clip-"))
                            ? id.mid(5).section('-', 0, 0).toLongLong(&clipIdOk)
                            : 0;
    const auto image = clipIdOk ? runtime_.clipThumbnail(clipId) : runtime_.previewFrame();
    if (size) *size = image.size();
    return image;
  }
 private:
  const edward::desktop::WorkbenchRuntime& runtime_;
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QtWebEngineQuick::initialize();
  const auto settingsPath = qEnvironmentVariable("EDWARD_SETTINGS_PATH");
  QString resolvedSettingsPath = settingsPath;
  if (resolvedSettingsPath.isEmpty()) {
    const auto settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(settingsDirectory);
    resolvedSettingsPath = QDir(settingsDirectory).filePath(QStringLiteral("settings.ini"));
  }
  QSettings settings(resolvedSettingsPath, QSettings::IniFormat);
  const auto defaultCacheRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
      + QStringLiteral("/cache");
  const auto cacheRoot = settings.value(QStringLiteral("paths/cacheRoot"), defaultCacheRoot).toString();
  const auto webStorageRoot = QDir(cacheRoot).filePath(QStringLiteral("web-profile"));
  QDir().mkpath(webStorageRoot);
  auto* webProfile = QWebEngineProfile::defaultProfile();
  webProfile->setPersistentStoragePath(webStorageRoot);
  webProfile->setCachePath(webStorageRoot + QStringLiteral("/cache"));
  webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
  QWebEngineScript webChannelBootstrap;
  webChannelBootstrap.setName(QStringLiteral("edward-webchannel-bootstrap"));
  webChannelBootstrap.setInjectionPoint(QWebEngineScript::DocumentCreation);
  webChannelBootstrap.setWorldId(QWebEngineScript::MainWorld);
  QFile webChannelSource(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
  if (webChannelSource.open(QIODevice::ReadOnly))
    webChannelBootstrap.setSourceCode(QString::fromUtf8(webChannelSource.readAll()));
  webProfile->scripts()->insert(webChannelBootstrap);
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QProcess fablecutServer;
  QString fablecutStartupError;
  QObject::connect(&fablecutServer, &QProcess::errorOccurred, &app,
                   [&fablecutServer, &fablecutStartupError](QProcess::ProcessError error) {
                     fablecutStartupError = QStringLiteral("process_error=%1 detail=%2")
                         .arg(static_cast<int>(error)).arg(fablecutServer.errorString());
                   });
  quint16 fablecutPort = 7777;
  const auto fablecutRoot = QString::fromUtf8(EDWARD_SOURCE_DIR) + QStringLiteral("/third_party/FableCut");
  const auto fablecutEntry = fablecutRoot + QStringLiteral("/server.js");
  QString supabaseUrl;
  QString supabaseAnonKey;
  if (QFileInfo::exists(fablecutEntry)) {
#ifdef Q_OS_UNIX
    // 应用被系统终止时，Node 子进程可能成为孤儿。启动新实例前只清理由当前
    // 工作树启动的监听服务，确保固定的本地来源和最新服务代码会被使用。
    QProcess listeners;
    listeners.start(QStringLiteral("lsof"), {QStringLiteral("-tiTCP"), QStringLiteral("-sTCP:LISTEN")});
    if (listeners.waitForFinished(1000)) {
      const auto expectedCwd = QFileInfo(fablecutRoot).canonicalFilePath();
      for (const auto& pidBytes : listeners.readAllStandardOutput().split('\n')) {
        const auto pid = QString::fromLocal8Bit(pidBytes).trimmed();
        if (pid.isEmpty() || !pid.toLongLong()) continue;
        QProcess processCwd;
        processCwd.start(QStringLiteral("lsof"), {QStringLiteral("-a"), QStringLiteral("-p"), pid,
                                                     QStringLiteral("-d"), QStringLiteral("cwd"), QStringLiteral("-Fn")});
        if (!processCwd.waitForFinished(500)) continue;
        bool currentFablecutProcess = false;
        for (const auto& line : processCwd.readAllStandardOutput().split('\n')) {
          if (line.startsWith('n') && QFileInfo(QString::fromLocal8Bit(line.mid(1)).trimmed()).canonicalFilePath() == expectedCwd) {
            currentFablecutProcess = true;
            break;
          }
        }
        if (currentFablecutProcess) QProcess::execute(QStringLiteral("kill"), {QStringLiteral("-TERM"), pid});
      }
      for (int attempt = 0; attempt < 30; ++attempt) {
        QTcpServer portProbe;
        if (portProbe.listen(QHostAddress::LocalHost, fablecutPort)) {
          portProbe.close();
          break;
        }
        QThread::msleep(100);
      }
    }
#endif
    fablecutServer.setWorkingDirectory(fablecutRoot);
    auto nodeProgram = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (nodeProgram.isEmpty() && QFileInfo::exists(QStringLiteral("/opt/homebrew/bin/node")))
      nodeProgram = QStringLiteral("/opt/homebrew/bin/node");
    if (nodeProgram.isEmpty() && QFileInfo::exists(QStringLiteral("/usr/local/bin/node")))
      nodeProgram = QStringLiteral("/usr/local/bin/node");
    fablecutServer.setProgram(nodeProgram.isEmpty() ? QStringLiteral("node") : nodeProgram);
    fablecutServer.setArguments({QStringLiteral("server.js")});
    fablecutServer.setProcessChannelMode(QProcess::MergedChannels);
    // anon key 是公开客户端密钥；资源权限仍由请求携带的用户会话控制。
    auto serverEnvironment = QProcessEnvironment::systemEnvironment();
    serverEnvironment.insert(QStringLiteral("SUPABASE_URL"),
                             QStringLiteral("https://naybqwiqgviuzjtemerc.supabase.co"));
    serverEnvironment.insert(QStringLiteral("SUPABASE_ANON_KEY"),
                             QStringLiteral("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5heWJxd2lxZ3ZpdXpqdGVtZXJjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODY4NTE3NTQsImV4cCI6MjEwMjQyNzc1NH0.MVnAoziZGfFzw9HelNYM6auqmfQE884D8kMTDAScf_Y"));
    supabaseUrl = serverEnvironment.value(QStringLiteral("SUPABASE_URL"));
    supabaseAnonKey = serverEnvironment.value(QStringLiteral("SUPABASE_ANON_KEY"));
    const auto setDirectory = [&settings, &serverEnvironment](const QString& settingKey, const QString& variable) {
      const auto value = settings.value(settingKey).toString().trimmed();
      if (!value.isEmpty()) serverEnvironment.insert(variable, value);
    };
    setDirectory(QStringLiteral("paths/projectRoot"), QStringLiteral("FABLECUT_DATA_DIR"));
    setDirectory(QStringLiteral("paths/mediaDownloadRoot"), QStringLiteral("FABLECUT_MEDIA_DIR"));
    setDirectory(QStringLiteral("paths/exportRoot"), QStringLiteral("FABLECUT_EXPORTS_DIR"));
    setDirectory(QStringLiteral("paths/cacheRoot"), QStringLiteral("FABLECUT_CACHE_ROOT"));
    setDirectory(QStringLiteral("paths/componentDownloadRoot"), QStringLiteral("FABLECUT_COMPONENTS_DIR"));
    serverEnvironment.insert(QStringLiteral("PORT"), QString::number(fablecutPort));
    fablecutServer.setProcessEnvironment(serverEnvironment);
    fablecutServer.start();
    fablecutServer.waitForStarted(3000);
    if (fablecutServer.state() == QProcess::Running) fablecutServer.waitForFinished(300);
    // 旧版本异常退出后可能遗留同一项目的 Node 服务，占用 7777 并让新配置无法生效。
    // 只清理工作目录明确指向本项目 FableCut/server.js 的监听进程。
#ifdef Q_OS_UNIX
    if (fablecutServer.state() == QProcess::NotRunning) {
      QProcess lsof;
      lsof.start(QStringLiteral("lsof"), {QStringLiteral("-tiTCP:7777"), QStringLiteral("-sTCP:LISTEN")});
      if (lsof.waitForFinished(1000)) {
        const auto pids = lsof.readAllStandardOutput().split('\n');
        for (const auto& pidBytes : pids) {
          const auto pid = QString::fromLocal8Bit(pidBytes).trimmed();
          if (pid.isEmpty() || !pid.toLongLong()) continue;
          QProcess processCwd;
          processCwd.start(QStringLiteral("lsof"),
                           {QStringLiteral("-a"), QStringLiteral("-p"), pid,
                            QStringLiteral("-d"), QStringLiteral("cwd"), QStringLiteral("-Fn")});
          if (!processCwd.waitForFinished(500)) continue;
          bool currentFablecutProcess = false;
          const auto expectedCwd = QFileInfo(fablecutRoot).canonicalFilePath();
          for (const auto& line : processCwd.readAllStandardOutput().split('\n')) {
            if (!line.startsWith('n')) continue;
            const auto processDirectory = QString::fromLocal8Bit(line.mid(1)).trimmed();
            if (QFileInfo(processDirectory).canonicalFilePath() == expectedCwd) {
              currentFablecutProcess = true;
              break;
            }
          }
          if (!currentFablecutProcess) continue;
          QProcess::execute(QStringLiteral("kill"), {QStringLiteral("-TERM"), pid});
        }
      }
      fablecutServer.start();
      fablecutServer.waitForStarted(3000);
      if (fablecutServer.state() == QProcess::Running) fablecutServer.waitForFinished(300);
      if (fablecutServer.state() == QProcess::NotRunning) {
        QTcpServer portProbe;
        if (portProbe.listen(QHostAddress::LocalHost, 0)) {
          fablecutPort = portProbe.serverPort();
          portProbe.close();
          serverEnvironment.insert(QStringLiteral("PORT"), QString::number(fablecutPort));
          fablecutServer.setProcessEnvironment(serverEnvironment);
          fablecutServer.start();
          fablecutServer.waitForStarted(3000);
          if (fablecutServer.state() == QProcess::Running) fablecutServer.waitForFinished(300);
        }
      }
    }
#endif
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &fablecutServer, [&fablecutServer] {
      if (fablecutServer.state() != QProcess::NotRunning) {
        fablecutServer.terminate();
        if (!fablecutServer.waitForFinished(1500)) fablecutServer.kill();
      }
    });
  }
  QQmlApplicationEngine engine;
  edward::desktop::WorkbenchRuntime runtime;
  runtime.setSupabaseAuthConfig({supabaseUrl, supabaseAnonKey});
  if (!fablecutStartupError.isEmpty())
    runtime.recordFablecutDiagnostic(QStringLiteral("server_startup_%1").arg(fablecutStartupError));
  QObject::connect(&fablecutServer, &QProcess::readyReadStandardOutput, &runtime, [&fablecutServer, &runtime] {
    const auto output = QString::fromUtf8(fablecutServer.readAllStandardOutput()).trimmed();
    if (!output.isEmpty()) runtime.recordFablecutDiagnostic(QStringLiteral("server %1").arg(output.left(480)));
  });
  QObject::connect(&fablecutServer, &QProcess::errorOccurred, &runtime,
                   [&runtime](QProcess::ProcessError error) {
    runtime.recordFablecutDiagnostic(QStringLiteral("server_process_error code=%1").arg(static_cast<int>(error)));
  });
  const auto initialServerOutput = QString::fromUtf8(fablecutServer.readAllStandardOutput()).trimmed();
  if (!initialServerOutput.isEmpty()) runtime.recordFablecutDiagnostic(QStringLiteral("server %1").arg(initialServerOutput.left(480)));
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &runtime, [&runtime] {
    runtime.flushPreferencesForProjectClose();
  });
  QObject::connect(QWebEngineProfile::defaultProfile(), &QWebEngineProfile::downloadRequested,
                   &app, [&runtime](QWebEngineDownloadRequest* download) {
    const auto target = runtime.pendingFablecutExportPath();
    if (target.isEmpty()) return;
    const QFileInfo targetInfo(target);
    if (!targetInfo.absoluteDir().exists()) return;
    download->setDownloadDirectory(targetInfo.absolutePath());
    // The FableCut server reserves the requested name and appends _1, _2, …
    // on a collision. Keep that resolved filename instead of overwriting the
    // preselected target path in the native download handler.
    download->setDownloadFileName(download->suggestedFileName());
    download->accept();
    runtime.setPendingFablecutExportPath({});
  });
  engine.addImageProvider(QStringLiteral("edward"), new EdwardFrameProvider(runtime));
  engine.rootContext()->setContextProperty(
      QStringLiteral("fablecutServerUrl"), QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(fablecutPort)));
  engine.rootContext()->setContextProperty(QStringLiteral("preferenceStore"), runtime.preferenceStore());
  engine.rootContext()->setContextProperty(QStringLiteral("workbenchRuntime"), &runtime);
  engine.load(QUrl(QStringLiteral("qrc:/qml/Workbench.qml")));
  if (engine.rootObjects().isEmpty()) return 1;
  if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
    // 使用系统原生标题栏；窗口可移动、缩放，并不再锁定到屏幕右侧。
    // 扩展客户区到系统标题栏：原生窗口按钮仍由系统绘制，QML 控件可在标题栏内水平对齐。
    // 不设置固定几何、置顶或尺寸限制，保留窗口常规移动/缩放能力。
    window->setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
    // 在首次 show 前设置窗口状态；仅依赖 QML visibility 会先创建普通窗口，
    // macOS 随后再最大化，产生可见闪烁。
    window->setWindowState(Qt::WindowMaximized);
    window->showMaximized();
#ifdef Q_OS_MACOS
    bool localServiceStarted = fablecutServer.state() == QProcess::Running;
    if (!localServiceStarted) {
      QTcpSocket probe;
      probe.connectToHost(QStringLiteral("127.0.0.1"), fablecutPort);
      localServiceStarted = probe.waitForConnected(300);
    }
    installEdwardTitlebar(window, localServiceStarted);
    QObject::connect(&runtime, &edward::desktop::WorkbenchRuntime::fablecutAuthStateChanged,
                     window, [window](bool authenticated) {
                       updateEdwardTitlebarAuthState(window, authenticated);
                     });
#endif
  }
  return app.exec();
}
