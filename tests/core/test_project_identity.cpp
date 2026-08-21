#include <edward/core/project_identity.hpp>

#include <cassert>

int main() {
  assert(!edward::core::ProjectIdentity::parse(QStringLiteral("../outside")));
  assert(!edward::core::ProjectIdentity::parse(QStringLiteral("550e8400-e29b-41d4-a716-44665544000")));
  const auto identity = edward::core::ProjectIdentity::parse(
      QStringLiteral("550e8400-e29b-41d4-a716-446655440000"));
  assert(identity);
  assert(identity->value() == QStringLiteral("550e8400-e29b-41d4-a716-446655440000"));
  assert(!edward::core::ProjectIdentity::create().value().isEmpty());
  return 0;
}
