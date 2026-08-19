#pragma once

#include "edward/resources/component_upload_queue.hpp"

#include "edward/resources/component_package.hpp"

#include <QString>

#include <optional>
#include <vector>

namespace edward::resources {

class ComponentUploadScheduler final {
 public:
  ComponentUploadScheduler(QString statePath, QString pendingRoot)
      : statePath_(std::move(statePath)), pendingRoot_(std::move(pendingRoot)) {}

  bool restore(const QDateTime& now, QString* error = nullptr);
  bool enqueue(const QString& localPackagePath, const QString& resourceId, const QDateTime& now,
               QString* error = nullptr);
  std::optional<ComponentUploadQueueItem> nextReady(const QDateTime& now) const;
  std::optional<ComponentPackage> loadReadyPackage(const QDateTime& now, QString* error = nullptr) const;
  bool complete(const QString& resourceId, QString* error = nullptr);
  bool recordFailure(const QString& resourceId, const QDateTime& now, QString* error = nullptr);
  int pendingCount() const { return static_cast<int>(items_.size()); }

 private:
  bool persist(QString* error);

  QString statePath_;
  QString pendingRoot_;
  std::vector<ComponentUploadQueueItem> items_;
};

}  // namespace edward::resources
