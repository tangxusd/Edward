#include "edward/resources/auth_session_store.hpp"

namespace edward::resources {

bool AuthSessionStore::authenticated() const {
  return !session_.userId.isEmpty() && !session_.username.isEmpty() && !session_.accessToken.isEmpty();
}

QString AuthSessionStore::userId() const { return session_.userId; }

QString AuthSessionStore::username() const { return session_.username; }

void AuthSessionStore::setSession(AuthSession session) {
  session_ = std::move(session);
  emit changed();
}

void AuthSessionStore::clear() {
  session_ = {};
  emit changed();
}

}  // namespace edward::resources
