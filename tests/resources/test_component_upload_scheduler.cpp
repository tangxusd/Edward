#include <edward/resources/component_package.hpp>
#include <edward/resources/component_upload_scheduler.hpp>

#include <QFile>
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

  const QString pendingRoot = directory.filePath("pending");
  const QString statePath = pendingRoot + "/state.json";
  edward::resources::ComponentUploadScheduler scheduler(statePath, pendingRoot);
  assert(scheduler.enqueue(localPath, "demo.component", QDateTime::fromMSecsSinceEpoch(1'000), &error));
  assert(scheduler.pendingCount() == 1);
  assert(QFile::exists(statePath));

  edward::resources::ComponentUploadScheduler resumed(statePath, pendingRoot);
  assert(resumed.restore(QDateTime::fromMSecsSinceEpoch(2'000), &error));
  assert(resumed.pendingCount() == 1);
  const auto due = resumed.nextReady(QDateTime::fromMSecsSinceEpoch(2'000));
  assert(due && due->resourceId == "demo.component");
  const auto queuedPackage = resumed.loadReadyPackage(QDateTime::fromMSecsSinceEpoch(2'000), &error);
  assert(queuedPackage && queuedPackage->resourceId == "demo.component");
  assert(resumed.complete("demo.component", &error));
  assert(resumed.pendingCount() == 0);
  const auto stored = edward::resources::ComponentUploadQueue::load(statePath, &error);
  assert(stored && stored->empty());
  assert(QFile::exists(localPath + "/manifest.json"));
  assert(!QFile::exists(due->queuedCopyPath + "/manifest.json"));

  assert(resumed.enqueue(localPath, "demo.component", QDateTime::fromMSecsSinceEpoch(3'000), &error));
  assert(resumed.recordFailure("demo.component", QDateTime::fromMSecsSinceEpoch(4'000), &error));
  const auto delayed = resumed.nextReady(QDateTime::fromMSecsSinceEpoch(4'000));
  assert(!delayed);
  for (int failure = 1; failure < 5; ++failure)
    assert(resumed.recordFailure("demo.component", QDateTime::fromMSecsSinceEpoch(4'000 + failure), &error));
  assert(resumed.pendingCount() == 0);
  assert(QFile::exists(localPath + "/manifest.json"));
  return 0;
}
