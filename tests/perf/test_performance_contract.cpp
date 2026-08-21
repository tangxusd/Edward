#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <cassert>

static QString sha256(const QString& path) {
  QFile file(path);
  assert(file.open(QIODevice::ReadOnly));
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(file.readAll());
  return QString::fromLatin1(hash.result().toHex());
}

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
  report.close();

  QJsonArray fixtures;
  for (const auto& id : {QStringLiteral("1080p"), QStringLiteral("4k"), QStringLiteral("vfr")}) {
    const auto fixturePath = directory.path() + QStringLiteral("/") + id + QStringLiteral(".fixture");
    QFile fixture(fixturePath);
    assert(fixture.open(QIODevice::WriteOnly));
    fixture.write(id.toUtf8());
    fixture.close();
    fixtures.push_back(QJsonObject{{"id", id}, {"path", fixturePath}, {"sha256", sha256(fixturePath)}});
  }
  assert(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
  manifest.write(QJsonDocument(QJsonObject{{"fixtures", fixtures}}).toJson(QJsonDocument::Compact));
  manifest.close();
  benchmark.start(QString::fromLocal8Bit(argv[1]), {QStringLiteral("--manifest"), manifestPath,
                                                     QStringLiteral("--report"), reportPath});
  assert(benchmark.waitForFinished(5000));
  assert(benchmark.exitCode() == 0);
  assert(report.open(QIODevice::ReadOnly));
  const auto ready = QJsonDocument::fromJson(report.readAll()).object();
  assert(ready.value("status").toString() == QStringLiteral("fixtures_ready_not_collected"));
  assert(!ready.value("metricsCollected").toBool());
  return 0;
}
