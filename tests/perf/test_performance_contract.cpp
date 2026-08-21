#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 2);
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto manifestPath = directory.path() + QStringLiteral("/fixtures.json");
  const auto reportPath = directory.path() + QStringLiteral("/report.json");
  QFile manifest(manifestPath);
  assert(manifest.open(QIODevice::WriteOnly));
  manifest.write(QJsonDocument(QJsonObject{{"fixtures", QJsonArray{}}}).toJson(QJsonDocument::Compact));
  manifest.close();
  QProcess benchmark;
  benchmark.start(QString::fromLocal8Bit(argv[1]), {QStringLiteral("--manifest"), manifestPath,
                                                     QStringLiteral("--report"), reportPath});
  assert(benchmark.waitForFinished(5000));
  assert(benchmark.exitCode() != 0);
  QFile report(reportPath);
  assert(report.open(QIODevice::ReadOnly));
  const auto document = QJsonDocument::fromJson(report.readAll());
  assert(document.isObject());
  const auto object = document.object();
  assert(object.value("status").toString() == QStringLiteral("fixture_validation_failed"));
  assert(object.value("requiredMetrics").toArray().contains(QStringLiteral("peak_rss_mb")));
  assert(object.value("requiredMetrics").toArray().contains(QStringLiteral("seek_p95_ms")));
  assert(object.contains("machine"));
  assert(object.contains("dependencies"));
  return 0;
}
