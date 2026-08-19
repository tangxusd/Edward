#include <edward/resources/component_package.hpp>
#include <edward/resources/component_upload_dispatcher.hpp>

#include <QTemporaryDir>

#include <cassert>

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto component = edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}});
  assert(component);
  const QString localPath = directory.filePath("my-components/demo.component");
  const edward::resources::ComponentPackage package{"demo.component", "Demo", *component, {}, {}, {}, {}};
  QString error;
  assert(package.saveLocal(localPath.toStdString(), &error));

  edward::resources::ComponentUploadDispatcher dispatcher(
      directory.filePath("pending/state.json"), directory.filePath("pending"));
  assert(dispatcher.restore(QDateTime::fromMSecsSinceEpoch(1'000), &error));
  assert(dispatcher.enqueue(localPath, "demo.component", QDateTime::fromMSecsSinceEpoch(1'000), &error));
  assert(!dispatcher.dispatchNext("http://localhost/upload", {"user", "demo@example.com", "token"},
                                  QDateTime::fromMSecsSinceEpoch(1'000), &error));
  assert(dispatcher.pendingCount() == 1);
  assert(!dispatcher.dispatchNext("https://catalog.example/upload", {"", "demo@example.com", "token"},
                                  QDateTime::fromMSecsSinceEpoch(1'000), &error));
  assert(dispatcher.pendingCount() == 1);
  return 0;
}
