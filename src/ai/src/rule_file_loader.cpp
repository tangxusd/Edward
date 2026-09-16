#include "edward/ai/rule_file_loader.hpp"

#include <QCryptographicHash>
#include <QFile>

#include <array>

namespace edward::ai {
namespace {
constexpr qint64 kMaxRuleFileBytes = 64 * 1024;
bool contained(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto rootText = QString::fromStdString(std::filesystem::weakly_canonical(root).string());
  const auto candidateText = QString::fromStdString(std::filesystem::weakly_canonical(candidate).string());
  return candidateText == rootText || candidateText.startsWith(rootText + QLatin1Char('/'));
}
}

RuleSnapshot RuleFileLoader::load(const std::filesystem::path& projectRoot,
                                  const std::filesystem::path& target) const {
  RuleSnapshot snapshot;
  if (!std::filesystem::is_directory(projectRoot) || !contained(projectRoot, target)) return snapshot;
  auto directory = std::filesystem::is_directory(target) ? target : target.parent_path();
  QVector<std::filesystem::path> directories;
  while (contained(projectRoot, directory)) {
    directories.push_back(directory);
    if (directory == projectRoot) break;
    directory = directory.parent_path();
  }
  std::reverse(directories.begin(), directories.end());
  constexpr std::array<const char*, 3> names = {"AGENTS.override.md", "AGENTS.md", "CLAUDE.md"};
  for (int depth = 0; depth < directories.size(); ++depth) {
    for (const auto* name : names) {
      const auto path = directories[depth] / name;
      if (!std::filesystem::is_regular_file(path)) continue;
      QFile file(QString::fromStdString(path.string()));
      if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxRuleFileBytes) continue;
      const auto data = file.readAll();
      snapshot.entries.push_back({QString::fromStdString(path.string()),
                                  QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()), depth});
      if (!snapshot.text.isEmpty()) snapshot.text += QLatin1String("\n\n");
      snapshot.text += QString::fromUtf8(data);
      break;
    }
  }
  return snapshot;
}
}  // namespace edward::ai
