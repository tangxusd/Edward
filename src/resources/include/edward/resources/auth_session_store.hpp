#pragma once

#include "edward/resources/component_upload.hpp"

#include <QObject>

namespace edward::resources {

class AuthSessionStore final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool authenticated READ authenticated NOTIFY changed)
  Q_PROPERTY(QString userId READ userId NOTIFY changed)
  Q_PROPERTY(QString username READ username NOTIFY changed)

 public:
  explicit AuthSessionStore(QObject* parent = nullptr);
  bool authenticated() const;
  QString userId() const;
  QString username() const;
  AuthSession session() const { return session_; }
  void setSession(AuthSession session);
  Q_INVOKABLE void clear();

 signals:
  void changed();

 private:
  AuthSession session_;
};

}  // namespace edward::resources
