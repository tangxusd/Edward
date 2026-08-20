#include "edward/media/audio_waveform.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace edward::media {

namespace {

struct AudioStreamInfo {
  int sampleRate = 0;
  double durationSeconds = 0.0;
};

std::optional<AudioStreamInfo> audioStreamInfo(const std::filesystem::path& path) {
  QProcess probe;
  probe.start(QStringLiteral("ffprobe"), {QStringLiteral("-v"), QStringLiteral("error"),
    QStringLiteral("-select_streams"), QStringLiteral("a:0"), QStringLiteral("-show_entries"),
    QStringLiteral("stream=sample_rate:format=duration"), QStringLiteral("-of"), QStringLiteral("json"),
    QString::fromStdString(path.string())});
  if (!probe.waitForStarted(1000) || !probe.waitForFinished(5000) ||
      probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) return std::nullopt;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(probe.readAllStandardOutput(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;
  const auto streams = document.object().value(QStringLiteral("streams")).toArray();
  if (streams.empty()) return std::nullopt;
  const auto stream = streams.at(0).toObject();
  bool sampleRateOk = false;
  const auto sampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt(&sampleRateOk);
  const auto duration = document.object().value(QStringLiteral("format")).toObject()
    .value(QStringLiteral("duration")).toString().toDouble();
  if (!sampleRateOk || sampleRate <= 0 || !(duration > 0.0)) return std::nullopt;
  return AudioStreamInfo{sampleRate, duration};
}

void accumulate(QByteArray& pending, AudioWaveform& waveform, std::size_t expectedSamples,
                std::size_t& sampleIndex) {
  const auto sampleBytes = static_cast<std::size_t>(2);
  const auto availableSamples = static_cast<std::size_t>(pending.size()) / sampleBytes;
  for (std::size_t index = 0; index < availableSamples; ++index) {
    const auto offset = static_cast<int>(index * sampleBytes);
    const auto low = static_cast<std::uint8_t>(pending.at(offset));
    const auto high = static_cast<std::uint8_t>(pending.at(offset + 1));
    const auto sample = static_cast<std::int16_t>((high << 8) | low);
    const auto bucket = std::min(waveform.peaks.size() - 1, sampleIndex * waveform.peaks.size() / expectedSamples);
    waveform.peaks[bucket] = std::max(waveform.peaks[bucket], std::abs(static_cast<float>(sample) / 32768.0f));
    ++sampleIndex;
  }
  pending.remove(0, static_cast<int>(availableSamples * sampleBytes));
}

}  // namespace

std::optional<AudioWaveform> AudioWaveformExtractor::extract(const std::filesystem::path& path,
                                                              std::size_t bucketCount, double startSeconds,
                                                              std::optional<double> endSeconds) {
  if (path.empty() || !std::filesystem::is_regular_file(path) || bucketCount == 0 || startSeconds < 0.0 ||
      (endSeconds && (*endSeconds <= startSeconds))) return std::nullopt;
  const auto info = audioStreamInfo(path);
  if (!info) return std::nullopt;
  const auto end = std::min(endSeconds.value_or(info->durationSeconds), info->durationSeconds);
  if (startSeconds >= end) return std::nullopt;
  const auto expectedSamples = static_cast<std::size_t>(std::ceil((end - startSeconds) * info->sampleRate));
  if (expectedSamples == 0) return std::nullopt;
  AudioWaveform waveform{std::vector<float>(bucketCount, 0.0f), info->sampleRate};
  QProcess decoder;
  decoder.setProcessChannelMode(QProcess::SeparateChannels);
  decoder.start(QStringLiteral("ffmpeg"), {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
    QStringLiteral("error"), QStringLiteral("-ss"), QString::number(startSeconds, 'f', 6),
    QStringLiteral("-t"), QString::number(end - startSeconds, 'f', 6), QStringLiteral("-i"),
    QString::fromStdString(path.string()), QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("1"),
    QStringLiteral("-f"), QStringLiteral("s16le"), QStringLiteral("pipe:1")});
  if (!decoder.waitForStarted(1000)) return std::nullopt;
  QByteArray pending;
  std::size_t sampleIndex = 0;
  while (decoder.state() != QProcess::NotRunning) {
    if (!decoder.waitForReadyRead(1000)) continue;
    pending.append(decoder.readAllStandardOutput());
    accumulate(pending, waveform, expectedSamples, sampleIndex);
  }
  pending.append(decoder.readAllStandardOutput());
  accumulate(pending, waveform, expectedSamples, sampleIndex);
  if (decoder.exitStatus() != QProcess::NormalExit || decoder.exitCode() != 0) return std::nullopt;
  return waveform;
}

}  // namespace edward::media
