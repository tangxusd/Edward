#include <edward/resources/supabase_auth_client.hpp>

#include <cassert>

int main() {
  edward::resources::SupabaseAuthClient client;
  // 空凭据在发起网络请求前被拒绝，密码不会进入日志或请求队列。
  assert(!client.signUpWithPassword({"https://project.supabase.co", "anon-key"}, "", "password"));
  assert(!client.signUpWithPassword({"https://project.supabase.co", "anon-key"}, "demo@example.com", ""));
  assert(!client.enrollDevice({"https://project.supabase.co", "anon-key"}, "", {}));
  return 0;
}
