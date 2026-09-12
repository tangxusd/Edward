#include "edward/desktop/workbench_runtime.hpp"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QUrl>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>
#include <QWindow>
#include <QStandardPaths>
#include <QtWebEngineQuick>

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
    fablecutServer.start();
    fablecutServer.waitForStarted(3000);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &fablecutServer, [&fablecutServer] {
      if (fablecutServer.state() != QProcess::NotRunning) {
        fablecutServer.terminate();
        if (!fablecutServer.waitForFinished(1500)) fablecutServer.kill();
      }
    });
  }
  QQmlApplicationEngine engine;
  edward::desktop::WorkbenchRuntime runtime;
  engine.addImageProvider(QStringLiteral("edward"), new EdwardFrameProvider(runtime));
  engine.rootContext()->setContextProperty(QStringLiteral("workbenchRuntime"), &runtime);
  engine.load(QUrl(QStringLiteral("qrc:/qml/Workbench.qml")));
  if (engine.rootObjects().isEmpty()) return 1;
  if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
    // 使用系统原生标题栏；窗口可移动、缩放，并不再锁定到屏幕右侧。
    // 扩展客户区到系统标题栏：原生窗口按钮仍由系统绘制，QML 控件可在标题栏内水平对齐。
    // 不设置固定几何、置顶或尺寸限制，保留窗口常规移动/缩放能力。
    window->setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
    // QML sets Maximized before the first frame is shown, avoiding a visible
    // normal-window flash during startup.
    window->show();
#ifdef Q_OS_MACOS
    installEdwardTitlebar(window, fablecutServer.state() == QProcess::Running);
#endif
  }
  QTimer::singleShot(0, &runtime, [&runtime] { runtime.connectResolve(); });
  return app.exec();
}
