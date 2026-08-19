#pragma once

#include <QDateTime>
#include <QString>

#include <optional>
#include <vector>

namespace edward::resources {

struct ComponentUploadQueueItem final {
  QString resourceId;
  QString localPackagePath;
  QString queuedCopyPath;
  QDateTime createdAt;
  int failureCount = 0;
  QDateTime nextAttemptAt;
};

class ComponentUploadQueue final {
 public:
  static constexpr int kMaximumFailures = 5;
  static constexpr qint64 kRetryIntervalMilliseconds = 3 * 60 * 1000;
  static constexpr qint64 kMaximumAgeMilliseconds = 7LL * 24 * 60 * 60 * 1000;

  static std::optional<ComponentUploadQueueItem> enqueue(const QString& localPackagePath,
                                                         const QString& pendingRoot,
                                                         const QString& resourceId,
                                                         const QDateTime& now,
                                                         QString* error = nullptr);
  static bool readyForAttempt(const ComponentUploadQueueItem& item, const QDateTime& now);
  static std::optional<ComponentUploadQueueItem> recordFailure(ComponentUploadQueueItem item,
                                                               const QDateTime& now);
  static bool save(const QString& path, const std::vector<ComponentUploadQueueItem>& items,
                   QString* error = nullptr);
  static std::optional<std::vector<ComponentUploadQueueItem>> load(const QString& path,
                                                                    QString* error = nullptr);
};

}  // namespace edward::resources
