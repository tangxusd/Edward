#include <edward/core/timeline.hpp>
#include <edward/core/project_identity.hpp>
#include <edward/media/media_probe.hpp>
#include <edward/media/mlt_adapter.hpp>
#include <edward/media/proxy_manager.hpp>
#include <edward/media/export_job.hpp>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <array>
#include <algorithm>
#include <vector>
#include <sys/resource.h>

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
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  if (file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

double percentile(std::vector<double> samples, double ratio) {
  std::sort(samples.begin(), samples.end());
  const auto index = static_cast<std::size_t>(std::ceil((samples.size() - 1) * ratio));
  return samples.at(index);
}

double peakRssMb() {
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0.0;
#if defined(__APPLE__)
  return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
}

std::optional<QJsonObject> collectFixture(const QString& path, const std::filesystem::path& sampleRoot) {
  constexpr int sampleCount = 5;
  std::vector<double> imports;
  imports.reserve(sampleCount);
  std::optional<edward::media::MediaInfo> info;
  for (int sample = 0; sample < sampleCount; ++sample) {
    QElapsedTimer timer;
    timer.start();
    info = edward::media::MediaProbe::probe(std::filesystem::path(path.toStdString()));
    imports.push_back(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
    if (!info) return std::nullopt;
  }
  const auto duration = std::max<edward::core::Frame>(1, info->durationFrames);
  edward::core::Timeline timeline(duration);
  const auto track = timeline.addVideoTrack();
  if (!timeline.insertClip({1, track, std::filesystem::path(path.toStdString()), 0, duration, 0}))
    return std::nullopt;
  const auto snapshot = timeline.snapshot();
  const edward::media::MltAdapter adapter;
  std::vector<double> firstFrames;
  std::vector<double> seeks;
  std::vector<double> proxies;
  std::vector<double> exports;
  firstFrames.reserve(sampleCount);
  seeks.reserve(sampleCount);
  proxies.reserve(sampleCount);
  exports.reserve(sampleCount);
  for (int sample = 0; sample < sampleCount; ++sample) {
    QElapsedTimer timer;
    timer.start();
    const auto frame = adapter.renderFrame(snapshot, 0);
    firstFrames.push_back(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
    if (!frame || frame->isNull()) return std::nullopt;
  }
  for (int sample = 0; sample < sampleCount; ++sample) {
    const auto frameIndex = duration <= 1 ? 0 : duration * (sample + 1) / (sampleCount + 1);
    QElapsedTimer timer;
    timer.start();
    const auto frame = adapter.renderFrame(snapshot, frameIndex);
    seeks.push_back(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
    if (!frame || frame->isNull()) return std::nullopt;
  }
  for (int sample = 0; sample < sampleCount; ++sample) {
    const edward::media::RenderStorageRoots roots{sampleRoot / "proxies", sampleRoot / "cache",
                                                   sampleRoot / "renders"};
    const edward::media::ProxyManager manager(roots, edward::core::ProjectIdentity::create());
    QElapsedTimer timer;
    timer.start();
    const auto proxy = manager.ensureProxy(std::filesystem::path(path.toStdString()),
                                           edward::media::PreviewQuality::Fluent);
    proxies.push_back(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
    if (!proxy || !edward::media::MediaProbe::probe(*proxy)) return std::nullopt;
  }
  const edward::media::RenderGraph graph(adapter);
  const edward::media::ExportJob exportJob(graph);
  for (int sample = 0; sample < sampleCount; ++sample) {
    const auto output = sampleRoot / "exports" / ("sample-" + std::to_string(sample) + ".mp4");
    std::error_code cleanupError;
    std::filesystem::remove(output, cleanupError);
    QElapsedTimer timer;
    timer.start();
    const auto result = exportJob.run(snapshot, {output, QSize(info->width, info->height),
                                                 info->fpsNumerator, info->fpsDenominator,
                                                 edward::media::ExportQuality::High});
    const auto milliseconds = static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0;
    const auto outputInfo = result ? edward::media::MediaProbe::probe(output) : std::nullopt;
    const auto outputFrame = result ? adapter.renderSourceFrame(output, 0) : std::nullopt;
    std::filesystem::remove(output, cleanupError);
    if (!result || !outputInfo || outputInfo->width != info->width || outputInfo->height != info->height ||
        !outputFrame || outputFrame->isNull() || milliseconds <= 0.0) return std::nullopt;
    exports.push_back(static_cast<double>(result->frameCount) * 1000.0 / milliseconds);
  }
  return QJsonObject{{"sampleCount", sampleCount},
                     {"importMedianMs", percentile(imports, 0.5)},
                     {"firstFrameMedianMs", percentile(firstFrames, 0.5)},
                     {"seekP95Ms", percentile(seeks, 0.95)},
                     {"proxyMedianMs", percentile(proxies, 0.5)},
                     {"exportMedianFps", percentile(exports, 0.5)},
                     {"exportFirstFrameValid", true},
                     {"peakRssMb", peakRssMb()}};
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  const auto collect = argc >= 6 && QString::fromLocal8Bit(argv[5]) == QStringLiteral("--collect");
  const auto resume = collect && argc == 7 && QString::fromLocal8Bit(argv[6]) == QStringLiteral("--resume");
  if ((argc != 5 && !collect) || (argc == 7 && !resume) ||
      QString::fromLocal8Bit(argv[1]) != QStringLiteral("--manifest") ||
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
  if (failure.isEmpty() && collect) {
    QJsonObject samples;
    if (resume) {
      QFile checkpoint(reportPath);
      if (checkpoint.open(QIODevice::ReadOnly)) {
        const auto previous = QJsonDocument::fromJson(checkpoint.readAll()).object();
        if (previous.value("status").toString() == QStringLiteral("collecting") &&
            previous.value("samples").isObject()) samples = previous.value("samples").toObject();
      }
    }
    const auto fixtures = manifest.object().value("fixtures").toArray();
    for (const auto& requiredId : requiredIds) {
      const auto previous = samples.value(requiredId).toObject();
      if (resume && previous.value("sampleCount").toInt() == 5 &&
          previous.value("importMedianMs").isDouble() && previous.value("firstFrameMedianMs").isDouble() &&
          previous.value("seekP95Ms").isDouble() && previous.value("proxyMedianMs").isDouble() &&
          previous.value("exportMedianFps").isDouble()) {
        continue;
      }
      const auto found = std::find_if(fixtures.begin(), fixtures.end(), [&requiredId](const QJsonValue& value) {
        return value.toObject().value("id").toString() == requiredId;
      });
      const auto sampleRoot = std::filesystem::path(QFileInfo(reportPath).absolutePath().toStdString()) /
                              "edward-benchmark-samples" / requiredId.toStdString();
      std::error_code cleanupError;
      std::filesystem::remove_all(sampleRoot, cleanupError);
      if (cleanupError) {
        failure = QStringLiteral("无法准备代理采样目录：%1").arg(requiredId);
        break;
      }
      const auto result = collectFixture(found->toObject().value("path").toString(), sampleRoot);
      std::filesystem::remove_all(sampleRoot, cleanupError);
      if (!result) {
        failure = QStringLiteral("采集失败：%1").arg(requiredId);
        break;
      }
      samples.insert(requiredId, *result);
      QJsonObject checkpoint = report;
      checkpoint.insert("status", QStringLiteral("collecting"));
      checkpoint.insert("metricsCollected", false);
      checkpoint.insert("completedFixture", requiredId);
      checkpoint.insert("samples", samples);
      if (!writeReport(reportPath, checkpoint)) {
        failure = QStringLiteral("无法写入采集检查点：%1").arg(requiredId);
        break;
      }
    }
    if (failure.isEmpty()) {
      report.insert("samples", samples);
      report.insert("uncollectedRequiredMetrics", QJsonArray{
          QStringLiteral("cold_start_median_ms"), QStringLiteral("drag_p95_ms"),
          QStringLiteral("gpu_memory_mb"),
          QStringLiteral("output_ssim")});
    }
  }
  report.insert("status", failure.isEmpty()
      ? (collect ? QStringLiteral("metrics_collected_partial") : QStringLiteral("fixtures_ready_not_collected"))
      : (collect ? QStringLiteral("metric_collection_failed") : QStringLiteral("fixture_validation_failed")));
  report.insert("metricsCollected", failure.isEmpty() && collect);
  if (!failure.isEmpty()) report.insert("failure", failure);
  if (!writeReport(reportPath, report)) return 73;
  return failure.isEmpty() ? 0 : 2;
}
