#include <edward/resources/component_upload_queue.hpp>

#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <cassert>

int main() {
  QTemporaryDir directory;
  assert(directory.isValid());
  const QString localPackage = directory.filePath("local-component");
  const QString queuedCopy = directory.filePath("pending-upload/component.json");
  assert(QDir().mkpath(directory.filePath("pending-upload")));
  QFile localFile(localPackage);
  assert(localFile.open(QIODevice::WriteOnly));
  localFile.write("local component remains available");
  localFile.close();
  QFile pendingFile(queuedCopy);
  assert(pendingFile.open(QIODevice::WriteOnly));
  pendingFile.write("upload copy");
  pendingFile.close();

  edward::resources::ComponentUploadQueueItem item{
      "demo.component", localPackage, queuedCopy, QDateTime::fromMSecsSinceEpoch(1'000), 0,
      QDateTime::fromMSecsSinceEpoch(1'000)};
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
  QString error;
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
  assert(QFile::exists(localPackage));
  assert(!QFile::exists(queuedCopy));
  return 0;
}
