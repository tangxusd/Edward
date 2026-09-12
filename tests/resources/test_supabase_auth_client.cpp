#include <edward/resources/supabase_auth_client.hpp>

#include <cassert>

int main() {
  QString error;
  const edward::resources::SupabaseAuthConfig config{"https://project.supabase.co", "anon-key"};
  const auto request = edward::resources::SupabaseAuthClient::buildPasswordSignInRequest(
      config, "demo@example.com", "password", &error);
  assert(request);
  assert(request->endpoint == "https://project.supabase.co/functions/v1/auth-login");
  assert(request->body.value("identifier").toString() == "demo@example.com");
  assert(!edward::resources::SupabaseAuthClient::buildPasswordSignInRequest(
      {"https://project.supabase.co/rest/v1", "anon-key"}, "demo@example.com", "password", &error));
  assert(!edward::resources::SupabaseAuthClient::buildPasswordSignInRequest(
      config, "", "password", &error));
  edward::resources::AuthSessionStore sessions;
  edward::resources::SupabaseAuthClient client;
  assert(!client.signInWithPassword({"http://project.supabase.co", "anon-key"},
                                    "demo@example.com", "password", &sessions));
  assert(!sessions.authenticated());
  return 0;
}
