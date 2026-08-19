#include "edward/resources/component_upload_queue.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace edward::resources {
namespace {

void setError(QString* error, const QString& value) {
  if (error) *error = value;
}

QJsonObject toJson(const ComponentUploadQueueItem& item) {
  return {{"resourceId", item.resourceId},
          {"localPackagePath", item.localPackagePath},
          {"queuedCopyPath", item.queuedCopyPath},
          {"createdAt", QString::number(item.createdAt.toMSecsSinceEpoch())},
          {"failureCount", item.failureCount},
          {"nextAttemptAt", QString::number(item.nextAttemptAt.toMSecsSinceEpoch())}};
}

std::optional<ComponentUploadQueueItem> fromJson(const QJsonObject& object) {
  bool createdOk = false;
  bool nextOk = false;
  const auto createdAt = object.value("createdAt").toString().toLongLong(&createdOk);
  const auto nextAttemptAt = object.value("nextAttemptAt").toString().toLongLong(&nextOk);
  const auto failureCount = object.value("failureCount");
  if (object.value("resourceId").toString().isEmpty() || object.value("localPackagePath").toString().isEmpty() ||
      object.value("queuedCopyPath").toString().isEmpty() || !createdOk || !nextOk ||
      !failureCount.isDouble() || failureCount.toInt() < 0 || failureCount.toInt() >= ComponentUploadQueue::kMaximumFailures)
    return std::nullopt;
  return ComponentUploadQueueItem{object.value("resourceId").toString(),
                                  object.value("localPackagePath").toString(),
                                  object.value("queuedCopyPath").toString(),
                                  QDateTime::fromMSecsSinceEpoch(createdAt), failureCount.toInt(),
                                  QDateTime::fromMSecsSinceEpoch(nextAttemptAt)};
}

}  // namespace

bool ComponentUploadQueue::readyForAttempt(const ComponentUploadQueueItem& item, const QDateTime& now) {
  return item.failureCount < kMaximumFailures && now < item.createdAt.addMSecs(kMaximumAgeMilliseconds) &&
         now >= item.nextAttemptAt;
}

std::optional<ComponentUploadQueueItem> ComponentUploadQueue::recordFailure(
    ComponentUploadQueueItem item, const QDateTime& now) {
  ++item.failureCount;
  if (item.failureCount >= kMaximumFailures) {
    QFile::remove(item.queuedCopyPath);
    return std::nullopt;
  }
  item.nextAttemptAt = now.addMSecs(kRetryIntervalMilliseconds);
  return item;
}

bool ComponentUploadQueue::save(const QString& path, const std::vector<ComponentUploadQueueItem>& items,
                                QString* error) {
  QJsonArray array;
  for (const auto& item : items) array.append(toJson(item));
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    setError(error, QStringLiteral("upload queue state cannot be written"));
    return false;
  }
  if (file.write(QJsonDocument(array).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
    setError(error, QStringLiteral("upload queue state cannot be committed"));
    return false;
  }
  return true;
}

std::optional<std::vector<ComponentUploadQueueItem>> ComponentUploadQueue::load(const QString& path,
                                                                                  QString* error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    setError(error, QStringLiteral("upload queue state cannot be read"));
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    setError(error, QStringLiteral("upload queue state is invalid"));
    return std::nullopt;
  }
  std::vector<ComponentUploadQueueItem> items;
  for (const auto& value : document.array()) {
    if (!value.isObject()) {
      setError(error, QStringLiteral("upload queue item is invalid"));
      return std::nullopt;
    }
    const auto item = fromJson(value.toObject());
    if (!item) {
      setError(error, QStringLiteral("upload queue item is invalid"));
      return std::nullopt;
    }
    items.push_back(*item);
  }
  return items;
}

}  // namespace edward::resources
