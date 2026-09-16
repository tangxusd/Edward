#include <edward/ai/rule_file_loader.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

namespace {
void write(const QString& path, const QByteArray& content) { QFile file(path); assert(file.open(QIODevice::WriteOnly)); file.write(content); }
void testPrecedenceAndScopes() {
  QTemporaryDir root; assert(root.isValid());
  QDir(root.path()).mkpath("nested");
  write(root.filePath("CLAUDE.md"), "claude");
  write(root.filePath("AGENTS.md"), "agents");
  write(root.filePath("AGENTS.override.md"), "override");
  write(root.filePath("nested/AGENTS.md"), "nested");
  edward::ai::RuleFileLoader loader;
  const auto snapshot = loader.load(root.path().toStdString(), root.filePath("nested/file.txt").toStdString());
  assert(snapshot.entries.size() == 2);
  assert(snapshot.effectiveRules() == "override\n\nnested");
  assert(snapshot.entries.front().sha256.size() == 64);
}
void testRejectsOutsideAndOversize() {
  QTemporaryDir root; QTemporaryDir outside;
  edward::ai::RuleFileLoader loader;
  assert(loader.load(root.path().toStdString(), outside.path().toStdString()).entries.isEmpty());
  write(root.filePath("AGENTS.md"), QByteArray(64 * 1024 + 1, 'x'));
  assert(loader.load(root.path().toStdString(), root.path().toStdString()).entries.isEmpty());
}
}
int main() { testPrecedenceAndScopes(); testRejectsOutsideAndOversize(); }
