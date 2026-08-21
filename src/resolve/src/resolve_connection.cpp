#include "edward/resolve/resolve_connection.hpp"

#include <QElapsedTimer>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QThread>
#include <QProcess>

namespace edward::resolve {

class ResolveConnection::Impl final {
 public:
  explicit Impl(int timeoutMsValue) : timeoutMs(timeoutMsValue), socket(new QTcpSocket), process(new QProcess) {}
  ~Impl() { delete socket; delete process; }

  int timeoutMs;
  QTcpSocket* socket;
  QProcess* process;
  bool directSidecar = false;
  ConnectionState state = ConnectionState::Disconnected;
  int nextId = 1;
};

ResolveConnection::ResolveConnection(int timeoutMs) : impl_(new Impl(timeoutMs)) {}

ResolveConnection::~ResolveConnection() { delete impl_; }

bool ResolveConnection::connectToBridge(const QUrl& url, QString* error) {
  disconnect();
  if (url.scheme() != QStringLiteral("tcp") || url.host().isEmpty() || url.port() <= 0) {
    impl_->state = ConnectionState::Faulted;
    if (error) *error = QStringLiteral("bridge_url_invalid");
    return false;
  }

  impl_->state = ConnectionState::Connecting;
  impl_->socket->connectToHost(url.host(), static_cast<quint16>(url.port()));
  if (!impl_->socket->waitForConnected(impl_->timeoutMs)) {
    impl_->state = ConnectionState::Faulted;
    if (error) *error = QStringLiteral("bridge_connect_timeout");
    return false;
  }
  impl_->state = ConnectionState::Connected;
  impl_->directSidecar = false;
  return true;
}

bool ResolveConnection::connectToDirectSidecar(const QString& pythonExecutable,
                                               const QString& sidecarPath,
                                               QString* error) {
  disconnect();
  if (pythonExecutable.isEmpty() || sidecarPath.isEmpty()) {
    impl_->state = ConnectionState::Faulted;
    if (error) *error = QStringLiteral("direct_sidecar_path_invalid");
    return false;
  }
  impl_->state = ConnectionState::Connecting;
  impl_->process->start(pythonExecutable, {sidecarPath});
  if (!impl_->process->waitForStarted(impl_->timeoutMs)) {
    impl_->state = ConnectionState::Faulted;
    if (error) *error = impl_->process->errorString();
    return false;
  }
  impl_->directSidecar = true;
  impl_->state = ConnectionState::Connected;
  return true;
}

void ResolveConnection::disconnect() {
  if (impl_->directSidecar && impl_->process) {
    impl_->process->closeWriteChannel();
    if (!impl_->process->waitForFinished(50)) impl_->process->kill();
    impl_->directSidecar = false;
  }
  if (!impl_->socket) return;
  impl_->socket->disconnectFromHost();
  if (impl_->socket->state() != QAbstractSocket::UnconnectedState) impl_->socket->waitForDisconnected(50);
  impl_->state = ConnectionState::Disconnected;
}

ConnectionState ResolveConnection::state() const { return impl_->state; }

std::optional<QJsonObject> ResolveConnection::call(const QString& method,
                                                   const QJsonObject& params,
                                                   ResolveError* error) {
  const auto fail = [this, error](const QString& code, const QString& message) {
    impl_->state = ConnectionState::Faulted;
    if (error) *error = {code, message};
    return std::optional<QJsonObject>{};
  };
  if (impl_->state != ConnectionState::Connected) return fail(QStringLiteral("bridge_not_connected"), {});

  if (impl_->directSidecar) {
    const auto operation = method;
    const auto payload = QJsonDocument(QJsonObject{{QStringLiteral("operation"), operation},
                                                    {QStringLiteral("params"), params}})
                             .toJson(QJsonDocument::Compact) + '\n';
    if (impl_->process->write(payload) != payload.size() || !impl_->process->waitForBytesWritten(impl_->timeoutMs)) {
      return fail(QStringLiteral("direct_sidecar_write_failed"), impl_->process->errorString());
    }
    if (!impl_->process->waitForReadyRead(impl_->timeoutMs)) {
      return fail(QStringLiteral("direct_sidecar_response_timeout"), impl_->process->errorString());
    }
    const auto line = impl_->process->readLine();
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
      return fail(QStringLiteral("direct_sidecar_response_invalid"), parseError.errorString());
    }
    const auto object = response.object();
    if (!object.value(QStringLiteral("ok")).toBool()) {
      const auto sidecarError = object.value(QStringLiteral("error")).toObject();
      return fail(sidecarError.value(QStringLiteral("code")).toString(),
                  sidecarError.value(QStringLiteral("message")).toString());
    }
    if (error) *error = {};
    return object;
  }

  const int id = impl_->nextId++;
  const auto payload = QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                                  {QStringLiteral("method"), method},
                                                  {QStringLiteral("params"), params}})
                           .toJson(QJsonDocument::Compact) + '\n';
  if (impl_->socket->write(payload) != payload.size() || !impl_->socket->waitForBytesWritten(impl_->timeoutMs)) {
    return fail(QStringLiteral("bridge_write_failed"), impl_->socket->errorString());
  }

  QElapsedTimer timer;
  timer.start();
  QByteArray line;
  while (!line.contains('\n')) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    if (impl_->socket->bytesAvailable() > 0) {
      line += impl_->socket->readAll();
      continue;
    }
    const auto remaining = impl_->timeoutMs - static_cast<int>(timer.elapsed());
    if (remaining <= 0) {
      return fail(QStringLiteral("bridge_response_timeout"), impl_->socket->errorString());
    }
    QThread::msleep(static_cast<unsigned long>(qMin(remaining, 2)));
  }

  QJsonParseError parseError;
  const auto response = QJsonDocument::fromJson(line.left(line.indexOf('\n')), &parseError);
  if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
    return fail(QStringLiteral("bridge_response_invalid"), parseError.errorString());
  }
  const auto object = response.object();
  if (object.value(QStringLiteral("id")).toInt() != id) {
    return fail(QStringLiteral("bridge_response_id_mismatch"), {});
  }
  if (error) *error = {};
  return object;
}

}  // namespace edward::resolve
