#include <edward/resources/auth_session_store.hpp>

#include <cassert>

int main() {
  edward::resources::AuthSessionStore store;
  assert(!store.authenticated());
  store.setSession({"user-1", "demo@example.com", "token"});
  assert(store.authenticated());
  assert(store.userId() == "user-1");
  assert(store.username() == "demo@example.com");
  assert(store.session().accessToken == "token");
  store.clear();
  assert(!store.authenticated());
  return 0;
}
