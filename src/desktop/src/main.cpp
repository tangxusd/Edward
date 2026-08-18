#include "edward/desktop/workbench_runtime.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QQmlApplicationEngine engine;
  edward::desktop::WorkbenchRuntime runtime;
  engine.rootContext()->setContextProperty(QStringLiteral("workbenchRuntime"), &runtime);
  const auto qmlPath = QUrl::fromLocalFile(QStringLiteral("%1/src/desktop/qml/Workbench.qml").arg(QStringLiteral(EDWARD_SOURCE_DIR)));
  engine.load(qmlPath);
  if (engine.rootObjects().isEmpty()) return 1;
  return app.exec();
}
