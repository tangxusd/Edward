#include "edward/desktop/workbench_runtime.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QUrl>

class EdwardFrameProvider final : public QQuickImageProvider {
 public:
  explicit EdwardFrameProvider(const edward::desktop::WorkbenchRuntime& runtime)
      : QQuickImageProvider(QQuickImageProvider::Image), runtime_(runtime) {}
  QImage requestImage(const QString&, QSize* size, const QSize&) override {
    const auto image = runtime_.previewFrame();
    if (size) *size = image.size();
    return image;
  }
 private:
  const edward::desktop::WorkbenchRuntime& runtime_;
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QQmlApplicationEngine engine;
  edward::desktop::WorkbenchRuntime runtime;
  engine.addImageProvider(QStringLiteral("edward"), new EdwardFrameProvider(runtime));
  engine.rootContext()->setContextProperty(QStringLiteral("workbenchRuntime"), &runtime);
  const auto qmlPath = QUrl::fromLocalFile(QStringLiteral("%1/src/desktop/qml/Workbench.qml").arg(QStringLiteral(EDWARD_SOURCE_DIR)));
  engine.load(qmlPath);
  if (engine.rootObjects().isEmpty()) return 1;
  return app.exec();
}
