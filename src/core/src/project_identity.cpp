#include "edward/core/project_identity.hpp"

#include <QUuid>

namespace edward::core {

ProjectIdentity::ProjectIdentity(QString value) : value_(std::move(value)) {}

std::optional<ProjectIdentity> ProjectIdentity::parse(const QString& value) {
  const auto uuid = QUuid::fromString(value);
  if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != value.toLower()) return std::nullopt;
  return ProjectIdentity(uuid.toString(QUuid::WithoutBraces));
}

ProjectIdentity ProjectIdentity::create() {
  return ProjectIdentity(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

const QString& ProjectIdentity::value() const {
  return value_;
}

}  // namespace edward::core
