#pragma once

#include <QString>

#include <optional>

namespace edward::core {

class ProjectIdentity {
 public:
  static std::optional<ProjectIdentity> parse(const QString& value);
  static ProjectIdentity create();

  const QString& value() const;

 private:
  explicit ProjectIdentity(QString value);

  QString value_;
};

}  // namespace edward::core
