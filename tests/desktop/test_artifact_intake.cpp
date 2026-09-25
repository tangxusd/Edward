#include "edward/desktop/artifact_intake.hpp"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>

int main() {
  QTemporaryDir dir; assert(dir.isValid());
  QFile manifest(QDir(dir.path()).filePath("artifact.json")); assert(manifest.open(QIODevice::WriteOnly)); manifest.write(R"({"entry":"main.svg"})"); manifest.close();
  QFile entry(QDir(dir.path()).filePath("main.svg")); assert(entry.open(QIODevice::WriteOnly)); entry.write("<svg></svg>"); entry.close();
  QString error; const auto package = edward::desktop::ArtifactIntake::inspect(dir.path(), &error);
  assert(error.isEmpty() && package.entry == "main.svg" && package.files.size() == 2);
  QFile unsafe(QDir(dir.path()).filePath("unsafe.js")); assert(unsafe.open(QIODevice::WriteOnly)); unsafe.write("fetch(https://example.com)"); unsafe.close();
  assert(edward::desktop::ArtifactIntake::inspect(dir.path(), &error).files.isEmpty() && !error.isEmpty());
}
