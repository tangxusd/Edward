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
#include <QScreen>
#include <QtMath>
#include <memory>

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
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QProcess fablecutServer;
  const auto fablecutRoot = QString::fromUtf8(EDWARD_SOURCE_DIR) + QStringLiteral("/third_party/FableCut");
  const auto fablecutEntry = fablecutRoot + QStringLiteral("/server.js");
  if (QFileInfo::exists(fablecutEntry)) {
    fablecutServer.setWorkingDirectory(fablecutRoot);
    fablecutServer.setProgram(QStringLiteral("node"));
    fablecutServer.setArguments({QStringLiteral("server.js")});
    fablecutServer.start();
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
    // Edward 是 Resolve Studio 的辅助侧栏：无系统标题栏/红黄绿按钮，固定置顶。
    // 宽高和位置由宿主强制恢复，用户不能通过拖动或调整边框改变侧栏布局。
    window->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    constexpr double kReferenceSidebarWidth = 388.0;
    constexpr double kReferenceScreenWidth = 2560.0;
    auto snapping = std::make_shared<bool>(false);
    const auto snapToRightSidebar = [window] {
      auto* screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
      if (!screen) return;
      const auto area = screen->availableGeometry();
      const int sidebarWidth = qMax(1, qRound(area.width() * kReferenceSidebarWidth / kReferenceScreenWidth));
      window->setMinimumWidth(sidebarWidth);
      window->setMaximumWidth(sidebarWidth);
      window->setMinimumHeight(area.height());
      window->setMaximumHeight(area.height());
      window->setGeometry(area.right() - sidebarWidth + 1, area.top(),
                          sidebarWidth, area.height());
    };
    const auto enforceSidebar = [window, snapping, snapToRightSidebar] {
      if (*snapping) return;
      *snapping = true;
      snapToRightSidebar();
      *snapping = false;
    };
    QObject::connect(window, &QWindow::screenChanged, window, [enforceSidebar](QScreen*) {
      enforceSidebar();
    });
    QObject::connect(window, &QWindow::xChanged, window, [enforceSidebar](int) {
      enforceSidebar();
    });
    QObject::connect(window, &QWindow::yChanged, window, [enforceSidebar](int) {
      enforceSidebar();
    });
    QObject::connect(window, &QWindow::widthChanged, window, [enforceSidebar](int) {
      enforceSidebar();
    });
    QObject::connect(window, &QWindow::heightChanged, window, [enforceSidebar](int) {
      enforceSidebar();
    });
    if (auto* screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen()) {
      QObject::connect(screen, &QScreen::availableGeometryChanged, window,
                       [snapToRightSidebar](const QRect&) { snapToRightSidebar(); });
    }
    QTimer::singleShot(0, window, snapToRightSidebar);
  }
  QTimer::singleShot(0, &runtime, [&runtime] { runtime.connectResolve(); });
  return app.exec();
}
