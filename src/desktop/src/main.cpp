#include "edward/desktop/workbench_runtime.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QUrl>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QFile>
#include <QWindow>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtWebEngineQuick>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#include <QWebChannel>

#ifdef Q_OS_MACOS
void installEdwardTitlebar(QWindow *window, bool localServiceStarted);
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
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QProcess fablecutServer;
  const auto fablecutRoot = QString::fromUtf8(EDWARD_SOURCE_DIR) + QStringLiteral("/third_party/FableCut");
  const auto fablecutEntry = fablecutRoot + QStringLiteral("/server.js");
  if (QFileInfo::exists(fablecutEntry)) {
    fablecutServer.setWorkingDirectory(fablecutRoot);
    auto nodeProgram = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (nodeProgram.isEmpty() && QFileInfo::exists(QStringLiteral("/opt/homebrew/bin/node")))
      nodeProgram = QStringLiteral("/opt/homebrew/bin/node");
    if (nodeProgram.isEmpty() && QFileInfo::exists(QStringLiteral("/usr/local/bin/node")))
      nodeProgram = QStringLiteral("/usr/local/bin/node");
    fablecutServer.setProgram(nodeProgram.isEmpty() ? QStringLiteral("node") : nodeProgram);
    fablecutServer.setArguments({QStringLiteral("server.js")});
    // anon key 是公开客户端密钥；资源权限仍由请求携带的用户会话控制。
    auto serverEnvironment = QProcessEnvironment::systemEnvironment();
    serverEnvironment.insert(QStringLiteral("SUPABASE_URL"),
                             QStringLiteral("https://naybqwiqgviuzjtemerc.supabase.co"));
    serverEnvironment.insert(QStringLiteral("SUPABASE_ANON_KEY"),
                             QStringLiteral("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im5heWJxd2lxZ3ZpdXpqdGVtZXJjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODY4NTE3NTQsImV4cCI6MjEwMjQyNzc1NH0.MVnAoziZGfFzw9HelNYM6auqmfQE884D8kMTDAScf_Y"));
    fablecutServer.setProcessEnvironment(serverEnvironment);
    fablecutServer.start();
    fablecutServer.waitForStarted(3000);
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
          QProcess ps;
          ps.start(QStringLiteral("ps"), {QStringLiteral("-p"), pid, QStringLiteral("-o"), QStringLiteral("command=")});
          if (!ps.waitForFinished(500)) continue;
          const auto command = QString::fromLocal8Bit(ps.readAllStandardOutput());
          if (!command.contains(fablecutEntry)) continue;
          QProcess::execute(QStringLiteral("kill"), {QStringLiteral("-TERM"), pid});
        }
      }
      fablecutServer.start();
      fablecutServer.waitForStarted(3000);
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
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &runtime, [&runtime] {
    runtime.flushPreferencesForProjectClose();
  });
  QWebChannel preferenceChannel;
  preferenceChannel.registerObject(QStringLiteral("preferenceStore"), runtime.preferenceStore());
  QObject::connect(QWebEngineProfile::defaultProfile(), &QWebEngineProfile::downloadRequested,
                   &app, [&runtime](QWebEngineDownloadRequest* download) {
    const auto target = runtime.pendingFablecutExportPath();
    if (target.isEmpty()) return;
    const QFileInfo targetInfo(target);
    if (!targetInfo.absoluteDir().exists()) return;
    if (targetInfo.exists() && (!targetInfo.isFile() || !QFile::remove(targetInfo.absoluteFilePath()))) {
      download->cancel();
      runtime.setPendingFablecutExportPath({});
      return;
    }
    download->setDownloadDirectory(targetInfo.absolutePath());
    download->setDownloadFileName(targetInfo.fileName());
    download->accept();
    runtime.setPendingFablecutExportPath({});
  });
  engine.addImageProvider(QStringLiteral("edward"), new EdwardFrameProvider(runtime));
  engine.rootContext()->setContextProperty(QStringLiteral("workbenchRuntime"), &runtime);
  engine.rootContext()->setContextProperty(QStringLiteral("preferenceWebChannel"), &preferenceChannel);
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
      probe.connectToHost(QStringLiteral("127.0.0.1"), 7777);
      localServiceStarted = probe.waitForConnected(300);
    }
    installEdwardTitlebar(window, localServiceStarted);
#endif
  }
  QTimer::singleShot(0, &runtime, [&runtime] { runtime.connectResolve(false); });
  return app.exec();
}
