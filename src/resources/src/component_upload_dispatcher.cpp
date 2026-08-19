#include "edward/resources/component_upload_dispatcher.hpp"

namespace edward::resources {
namespace {

void setError(QString* error, const QString& value) {
  if (error) *error = value;
}

}  // namespace

ComponentUploadDispatcher::ComponentUploadDispatcher(QString statePath, QString pendingRoot,
                                                     QObject* parent)
    : QObject(parent), scheduler_(std::move(statePath), std::move(pendingRoot)), client_(this) {
  connect(&client_, &ComponentUploadClient::completed, this,
          [this](bool success, const QString& message, const QJsonObject&) {
            QString stateError;
            const bool updated = success
                                     ? scheduler_.complete(activeResourceId_, &stateError)
                                     : scheduler_.recordFailure(activeResourceId_, QDateTime::currentDateTimeUtc(),
                                                                &stateError);
            busy_ = false;
            activeResourceId_.clear();
            emit finished(success && updated, updated ? message : stateError);
          });
}

bool ComponentUploadDispatcher::restore(const QDateTime& now, QString* error) {
  return scheduler_.restore(now, error);
}

bool ComponentUploadDispatcher::enqueue(const QString& localPackagePath, const QString& resourceId,
                                        const QDateTime& now, QString* error) {
  return scheduler_.enqueue(localPackagePath, resourceId, now, error);
}

bool ComponentUploadDispatcher::dispatchNext(const QString& endpoint, const AuthSession& session,
                                             const QDateTime& now, QString* error) {
  if (busy_) {
    setError(error, QStringLiteral("component upload is already in progress"));
    return false;
  }
  const auto item = scheduler_.nextReady(now);
  if (!item) return false;
  const auto package = scheduler_.loadReadyPackage(now, error);
  if (!package) return false;
  if (!ComponentUploadClient::buildRequest(endpoint, *package, session, error)) return false;
  activeResourceId_ = item->resourceId;
  busy_ = true;
  if (client_.submit(endpoint, *package, session)) return true;
  busy_ = false;
  activeResourceId_.clear();
  return false;
}

}  // namespace edward::resources
