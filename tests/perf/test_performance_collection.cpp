#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <cassert>

namespace {
QString sha256(const QString& path) {
  QFile file(path);
  assert(file.open(QIODevice::ReadOnly));
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(file.readAll());
  return QString::fromLatin1(hash.result().toHex());
}
}

int main(int argc, char** argv) {
  assert(argc == 3);
  QTemporaryDir directory;
  assert(directory.isValid());
  const auto fixture = QString::fromLocal8Bit(argv[2]);
  const auto manifestPath = directory.path() + QStringLiteral("/fixtures.json");
  const auto reportPath = directory.path() + QStringLiteral("/report.json");
  const QJsonArray fixtures{
      QJsonObject{{"id", "1080p"}, {"path", fixture}, {"sha256", sha256(fixture)}},
      QJsonObject{{"id", "4k"}, {"path", fixture}, {"sha256", sha256(fixture)}},
      QJsonObject{{"id", "vfr"}, {"path", fixture}, {"sha256", sha256(fixture)}}};
  QFile manifest(manifestPath);
  assert(manifest.open(QIODevice::WriteOnly));
  manifest.write(QJsonDocument(QJsonObject{{"fixtures", fixtures}}).toJson(QJsonDocument::Compact));
  manifest.close();

  QProcess benchmark;
  benchmark.start(QString::fromLocal8Bit(argv[1]), {QStringLiteral("--manifest"), manifestPath,
                                                     QStringLiteral("--report"), reportPath,
                                                     QStringLiteral("--collect")});
  assert(benchmark.waitForFinished(15000));
  assert(benchmark.exitCode() == 0);
  QFile report(reportPath);
  assert(report.open(QIODevice::ReadOnly));
  const auto result = QJsonDocument::fromJson(report.readAll()).object();
  assert(result.value("status").toString() == QStringLiteral("metrics_collected_partial"));
  assert(result.value("metricsCollected").toBool());
  assert(result.value("coldStartMedianMs").toDouble() > 0.0);
  assert(!result.contains("completedFixture"));
  assert(!result.value("uncollectedRequiredMetrics").toArray().contains(QStringLiteral("export_fps")));
  assert(!result.value("uncollectedRequiredMetrics").toArray().contains(QStringLiteral("proxy_median_ms")));
  assert(result.value("uncollectedMetricReasons").toObject().value("gpu_memory_mb").isString());
  const auto samples = result.value("samples").toObject();
  for (const auto& id : {QStringLiteral("1080p"), QStringLiteral("4k"), QStringLiteral("vfr")}) {
    const auto sample = samples.value(id).toObject();
    assert(sample.value("sampleCount").toInt() == 5);
    assert(sample.value("importMedianMs").toDouble() >= 0.0);
    assert(sample.value("firstFrameMedianMs").toDouble() >= 0.0);
    assert(sample.value("seekP95Ms").toDouble() >= 0.0);
    assert(sample.value("dragP95Ms").toDouble() >= 0.0);
    assert(sample.value("proxyMedianMs").toDouble() >= 0.0);
    assert(sample.value("exportMedianFps").toDouble() > 0.0);
    assert(sample.value("outputSsim").toDouble() > 0.0);
    assert(sample.value("outputSsim").toDouble() <= 1.0);
    assert(sample.value("exportFirstFrameValid").toBool());
    assert(sample.value("peakRssMb").toDouble() > 0.0);
  }
  const auto preserved = result.value("samples").toObject().value("1080p").toObject();
  report.close();
  QJsonObject checkpoint{{"status", "collecting"}, {"samples", QJsonObject{{"1080p", preserved}}}};
  assert(report.open(QIODevice::WriteOnly | QIODevice::Truncate));
  report.write(QJsonDocument(checkpoint).toJson(QJsonDocument::Compact));
  report.close();
  benchmark.start(QString::fromLocal8Bit(argv[1]), {QStringLiteral("--manifest"), manifestPath,
                                                     QStringLiteral("--report"), reportPath,
                                                     QStringLiteral("--collect"), QStringLiteral("--resume")});
  assert(benchmark.waitForFinished(15000));
  assert(benchmark.exitCode() == 0);
  assert(report.open(QIODevice::ReadOnly));
  const auto resumed = QJsonDocument::fromJson(report.readAll()).object();
  assert(resumed.value("samples").toObject().value("1080p").toObject() == preserved);
  assert(resumed.value("samples").toObject().size() == 3);
  return 0;
}
