#include "edward/media/media_probe.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cmath>
#include <numeric>

namespace edward::media {

std::optional<MediaInfo> MediaProbe::probe(const std::filesystem::path &path) {
  if (path.empty() || !std::filesystem::is_regular_file(path)) return std::nullopt;
  QProcess process;
  process.start(QStringLiteral("ffprobe"), {
    QStringLiteral("-v"), QStringLiteral("error"),
    QStringLiteral("-show_entries"), QStringLiteral("stream=codec_type,width,height,r_frame_rate,pix_fmt:format=duration"),
    QStringLiteral("-of"), QStringLiteral("json"), QString::fromStdString(path.string())});
  if (!process.waitForStarted(1000) || !process.waitForFinished(5000) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return std::nullopt;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(process.readAllStandardOutput(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;
  const auto root = document.object();
  const auto streams = root.value(QStringLiteral("streams")).toArray();
  QJsonObject stream;
  bool hasAudio = false;
  bool hasAlpha = false;
  for (const auto& value : streams) {
    if (!value.isObject()) continue;
    const auto candidate = value.toObject();
    const auto codecType = candidate.value(QStringLiteral("codec_type")).toString();
    if (codecType == QStringLiteral("audio")) hasAudio = true;
    if (codecType == QStringLiteral("video") && stream.isEmpty()) {
      stream = candidate;
      const auto pixelFormat = candidate.value(QStringLiteral("pix_fmt")).toString();
      hasAlpha = pixelFormat.startsWith(QStringLiteral("yuva")) ||
                 pixelFormat.startsWith(QStringLiteral("rgba")) ||
                 pixelFormat.startsWith(QStringLiteral("argb"));
    }
  }
  if (stream.isEmpty()) return std::nullopt;
  const int width = stream.value(QStringLiteral("width")).toInt();
  const int height = stream.value(QStringLiteral("height")).toInt();
  const auto parts = stream.value(QStringLiteral("r_frame_rate")).toString().split(QLatin1Char('/'));
  if (width <= 0 || height <= 0 || parts.size() != 2) return std::nullopt;
  bool numeratorOk = false, denominatorOk = false;
  int numerator = parts[0].toInt(&numeratorOk), denominator = parts[1].toInt(&denominatorOk);
  if (!numeratorOk || !denominatorOk || numerator <= 0 || denominator <= 0) return std::nullopt;
  const int divisor = std::gcd(numerator, denominator);
  numerator /= divisor; denominator /= divisor;
  const double seconds = root.value(QStringLiteral("format")).toObject()
    .value(QStringLiteral("duration")).toString().toDouble();
  if (!(seconds > 0.0)) return std::nullopt;
  MediaInfo info{width, height, numerator, denominator,
    static_cast<std::int64_t>(std::ceil(seconds * numerator / denominator)), hasAudio, hasAlpha};
  return info.durationFrames > 0 ? std::optional<MediaInfo>(info) : std::nullopt;
}

}  // namespace edward::media
