#include "edward/resources/component_upload_scheduler.hpp"

#include <QFileInfo>

#include <algorithm>

namespace edward::resources {
namespace {

void setError(QString* error, const QString& value) {
  if (error) *error = value;
}

}  // namespace

bool ComponentUploadScheduler::persist(QString* error) {
  return ComponentUploadQueue::save(statePath_, items_, error);
}

bool ComponentUploadScheduler::restore(const QDateTime& now, QString* error) {
  if (!now.isValid()) {
    setError(error, QStringLiteral("upload queue restore time is invalid"));
    return false;
  }
  if (!QFileInfo::exists(statePath_)) {
    items_.clear();
    return true;
  }
  const auto stored = ComponentUploadQueue::load(statePath_, error);
  if (!stored) return false;
  items_.clear();
  bool changed = false;
  for (const auto& item : *stored) {
    if (ComponentUploadQueue::expire(item, now)) {
      changed = true;
      continue;
    }
    items_.push_back(item);
  }
  return !changed || persist(error);
}

bool ComponentUploadScheduler::enqueue(const QString& localPackagePath, const QString& resourceId,
                                       const QDateTime& now, QString* error) {
  if (std::any_of(items_.begin(), items_.end(), [&resourceId](const auto& item) {
        return item.resourceId == resourceId;
      })) {
    setError(error, QStringLiteral("component upload is already queued"));
    return false;
  }
  const auto item = ComponentUploadQueue::enqueue(localPackagePath, pendingRoot_, resourceId, now, error);
  if (!item) return false;
  items_.push_back(*item);
  if (persist(error)) return true;
  ComponentUploadQueue::complete(*item);
  items_.pop_back();
  return false;
}

std::optional<ComponentUploadQueueItem> ComponentUploadScheduler::nextReady(const QDateTime& now) const {
  const auto found = std::find_if(items_.begin(), items_.end(), [&now](const auto& item) {
    return ComponentUploadQueue::readyForAttempt(item, now);
  });
  return found == items_.end() ? std::nullopt : std::optional<ComponentUploadQueueItem>(*found);
}

std::optional<ComponentPackage> ComponentUploadScheduler::loadReadyPackage(const QDateTime& now,
                                                                            QString* error) const {
  const auto item = nextReady(now);
  if (!item) return std::nullopt;
  return ComponentPackage::load(item->queuedCopyPath.toStdString(), error);
}

bool ComponentUploadScheduler::complete(const QString& resourceId, QString* error) {
  const auto found = std::find_if(items_.begin(), items_.end(), [&resourceId](const auto& item) {
    return item.resourceId == resourceId;
  });
  if (found == items_.end()) {
    setError(error, QStringLiteral("queued component upload was not found"));
    return false;
  }
  const auto item = *found;
  items_.erase(found);
  if (persist(error)) {
    ComponentUploadQueue::complete(item);
    return true;
  }
  items_.push_back(item);
  return false;
}

bool ComponentUploadScheduler::recordFailure(const QString& resourceId, const QDateTime& now,
                                             QString* error) {
  const auto found = std::find_if(items_.begin(), items_.end(), [&resourceId](const auto& item) {
    return item.resourceId == resourceId;
  });
  if (found == items_.end()) {
    setError(error, QStringLiteral("queued component upload was not found"));
    return false;
  }
  const auto original = *found;
  if (original.failureCount + 1 >= ComponentUploadQueue::kMaximumFailures) {
    items_.erase(found);
    if (!persist(error)) {
      items_.push_back(original);
      return false;
    }
    ComponentUploadQueue::recordFailure(original, now);
    return true;
  }
  const auto updated = ComponentUploadQueue::recordFailure(original, now);
  if (!updated) {
    setError(error, QStringLiteral("component upload failure state is invalid"));
    return false;
  }
  *found = *updated;
  if (persist(error)) return true;
  *found = original;
  return false;
}

}  // namespace edward::resources
