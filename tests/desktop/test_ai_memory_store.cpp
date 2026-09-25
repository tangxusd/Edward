#include "edward/desktop/ai_memory_store.hpp"
#include <QTemporaryDir>
#include <cassert>

int main() {
  QTemporaryDir user; QTemporaryDir project; QTemporaryDir session;
  edward::desktop::AiMemoryStore store(user.path(), project.path(), session.path());
  QString error;
  assert(store.append(edward::desktop::MemoryScope::User, "用户偏好：简洁剪辑", &error));
  assert(store.read(edward::desktop::MemoryScope::User).contains("简洁剪辑"));
  assert(store.append(edward::desktop::MemoryScope::Project, "项目风格：冷色调", &error));
  assert(store.read(edward::desktop::MemoryScope::Project).contains("冷色调"));
  assert(!store.append(edward::desktop::MemoryScope::Session, "api_key=secret", &error));
  assert(store.upsert({QStringLiteral("style"), edward::desktop::MemoryScope::User, QStringLiteral("用户偏好" )}, &error));
  assert(store.remove(QStringLiteral("style"), &error));
  assert(store.clear(edward::desktop::MemoryScope::Project, &error));
  assert(store.read(edward::desktop::MemoryScope::Project).isEmpty());
}
