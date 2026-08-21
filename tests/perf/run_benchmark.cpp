#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <array>
#include <algorithm>

namespace {
QJsonArray requiredMetrics() {
  return {QStringLiteral("cold_start_median_ms"), QStringLiteral("first_frame_median_ms"),
          QStringLiteral("seek_p95_ms"), QStringLiteral("drag_p95_ms"),
          QStringLiteral("peak_rss_mb"), QStringLiteral("gpu_memory_mb"),
          QStringLiteral("proxy_median_ms"), QStringLiteral("export_fps"),
          QStringLiteral("output_ssim")};
}

QString sha256(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) hash.addData(file.read(1024 * 1024));
  return QString::fromLatin1(hash.result().toHex());
}

bool writeReport(const QString& path, const QJsonObject& report) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) >= 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 5 || QString::fromLocal8Bit(argv[1]) != QStringLiteral("--manifest") ||
      QString::fromLocal8Bit(argv[3]) != QStringLiteral("--report")) return 64;
  const auto manifestPath = QString::fromLocal8Bit(argv[2]);
  const auto reportPath = QString::fromLocal8Bit(argv[4]);
  QJsonObject report{{"schemaVersion", 1},
                     {"collectedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                     {"machine", QJsonObject{{"product", QSysInfo::prettyProductName()},
                                               {"cpuArchitecture", QSysInfo::currentCpuArchitecture()}}},
                     {"dependencies", QJsonObject{{"qt", QString::fromLatin1(qVersion())}}},
                     {"requiredMetrics", requiredMetrics()}};
  QFile manifestFile(manifestPath);
  QString failure;
  QJsonDocument manifest;
  if (!manifestFile.open(QIODevice::ReadOnly)) {
    failure = QStringLiteral("无法读取夹具清单");
  } else {
    manifest = QJsonDocument::fromJson(manifestFile.readAll());
    if (!manifest.isObject() || !manifest.object().value("fixtures").isArray())
      failure = QStringLiteral("夹具清单格式无效");
  }
  const std::array<QString, 3> requiredIds{QStringLiteral("1080p"), QStringLiteral("4k"), QStringLiteral("vfr")};
  if (failure.isEmpty()) {
    const auto fixtures = manifest.object().value("fixtures").toArray();
    for (const auto& requiredId : requiredIds) {
      const auto found = std::find_if(fixtures.begin(), fixtures.end(), [&requiredId](const QJsonValue& value) {
        return value.isObject() && value.toObject().value("id").toString() == requiredId;
      });
      if (found == fixtures.end()) {
        failure = QStringLiteral("缺少必需夹具：%1").arg(requiredId);
        break;
      }
      const auto fixture = found->toObject();
      const auto path = fixture.value("path").toString();
      const auto expectedHash = fixture.value("sha256").toString().toLower();
      if (path.isEmpty() || expectedHash.size() != 64 || !QFileInfo::exists(path) ||
          sha256(path) != expectedHash) {
        failure = QStringLiteral("夹具校验失败：%1").arg(requiredId);
        break;
      }
    }
  }
  report.insert("status", failure.isEmpty() ? QStringLiteral("fixtures_ready")
                                             : QStringLiteral("fixture_validation_failed"));
  if (!failure.isEmpty()) report.insert("failure", failure);
  if (!writeReport(reportPath, report)) return 73;
  return failure.isEmpty() ? 0 : 2;
}
