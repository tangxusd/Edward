#pragma once

#include "edward/resources/component_upload.hpp"
#include "edward/resources/component_upload_scheduler.hpp"

#include <QObject>

namespace edward::resources {

class ComponentUploadDispatcher final : public QObject {
  Q_OBJECT

 public:
  ComponentUploadDispatcher(QString statePath, QString pendingRoot, QObject* parent = nullptr);

  bool restore(const QDateTime& now, QString* error = nullptr);
  bool enqueue(const QString& localPackagePath, const QString& resourceId, const QDateTime& now,
               QString* error = nullptr);
  bool dispatchNext(const QString& endpoint, const AuthSession& session, const QDateTime& now,
                    QString* error = nullptr);
  int pendingCount() const { return scheduler_.pendingCount(); }
  bool busy() const { return busy_; }

 signals:
  void finished(bool success, QString message);

 private:
  ComponentUploadScheduler scheduler_;
  ComponentUploadClient client_;
  QString activeResourceId_;
  bool busy_ = false;
};

}  // namespace edward::resources
