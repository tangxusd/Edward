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
    QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"), QStringLiteral("v:0"),
    QStringLiteral("-show_entries"), QStringLiteral("stream=width,height,r_frame_rate:format=duration"),
    QStringLiteral("-of"), QStringLiteral("json"), QString::fromStdString(path.string())});
  if (!process.waitForStarted(1000) || !process.waitForFinished(5000) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return std::nullopt;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(process.readAllStandardOutput(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;
  const auto root = document.object();
  const auto streams = root.value(QStringLiteral("streams")).toArray();
  if (streams.isEmpty() || !streams.first().isObject()) return std::nullopt;
  const auto stream = streams.first().toObject();
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
    static_cast<std::int64_t>(std::ceil(seconds * numerator / denominator))};
  return info.durationFrames > 0 ? std::optional<MediaInfo>(info) : std::nullopt;
}

}  // namespace edward::media
