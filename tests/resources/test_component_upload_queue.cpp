#include <edward/resources/component_upload_queue.hpp>
#include <edward/resources/component_package.hpp>

#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <cassert>

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto component = edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}});
  assert(component);
  const QString localPackage = directory.filePath("local-component");
  const edward::resources::ComponentPackage package{"demo.component", "Demo", *component, {}, {}, {}, {}};
  QString error;
  assert(package.saveLocal(localPackage.toStdString(), &error));

  const auto queued = edward::resources::ComponentUploadQueue::enqueue(
      localPackage, directory.filePath("pending-upload"), "demo.component", QDateTime::fromMSecsSinceEpoch(1'000), &error);
  assert(queued);
  assert(QFile::exists(queued->queuedCopyPath + "/manifest.json"));
  assert(edward::resources::ComponentPackage::load(queued->queuedCopyPath.toStdString(), &error));

  auto item = *queued;
  const auto firstFailure = edward::resources::ComponentUploadQueue::recordFailure(
      item, QDateTime::fromMSecsSinceEpoch(2'000));
  assert(firstFailure);
  assert(firstFailure->failureCount == 1);
  assert(firstFailure->nextAttemptAt == QDateTime::fromMSecsSinceEpoch(182'000));

  assert(!edward::resources::ComponentUploadQueue::readyForAttempt(
      *firstFailure, QDateTime::fromMSecsSinceEpoch(181'999)));
  assert(edward::resources::ComponentUploadQueue::readyForAttempt(
      *firstFailure, QDateTime::fromMSecsSinceEpoch(182'000)));
  assert(!edward::resources::ComponentUploadQueue::readyForAttempt(
      *firstFailure, QDateTime::fromMSecsSinceEpoch(1'000 + 7LL * 24 * 60 * 60 * 1000)));

  const QString queueState = directory.filePath("pending-upload/state.json");
  assert(edward::resources::ComponentUploadQueue::save(queueState, {*firstFailure}, &error));
  const auto restored = edward::resources::ComponentUploadQueue::load(queueState, &error);
  assert(restored && restored->size() == 1);
  assert(restored->front().resourceId == "demo.component");
  assert(restored->front().failureCount == 1);
  assert(restored->front().nextAttemptAt == QDateTime::fromMSecsSinceEpoch(182'000));

  item.failureCount = 4;
  const auto finalFailure = edward::resources::ComponentUploadQueue::recordFailure(
      item, QDateTime::fromMSecsSinceEpoch(2'000));
  assert(!finalFailure);
  assert(QFile::exists(localPackage + "/manifest.json"));
  assert(!QFile::exists(item.queuedCopyPath + "/manifest.json"));
  return 0;
}
