#pragma once

#include <QString>
#include <QVector>

#include <filesystem>

namespace edward::ai {

struct RuleEntry final {
  QString path;
  QString sha256;
  int depth = 0;
};

struct RuleSnapshot final {
  QVector<RuleEntry> entries;
  QString text;
  [[nodiscard]] QString effectiveRules() const { return text; }
};

class RuleFileLoader final {
 public:
  [[nodiscard]] RuleSnapshot load(const std::filesystem::path& projectRoot,
                                  const std::filesystem::path& target) const;
};

}  // namespace edward::ai
