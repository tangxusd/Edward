#include "edward/media/audio_preview.hpp"

#include "edward/media/media_probe.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace edward::media {
namespace {

QString secondsForFrames(edward::core::Frame frames, int numerator, int denominator) {
  return QString::number(static_cast<double>(frames) * denominator / numerator, 'f', 6);
}

}  // namespace

AudioPreview::AudioPreview() {
  QObject::connect(&process_, &QProcess::readyReadStandardOutput, [this] { pump(); });
}

AudioPreview::~AudioPreview() { stop(); }

std::optional<QStringList> AudioPreview::argumentsFor(const edward::core::TimelineSnapshot& snapshot,
                                                       edward::core::Frame startFrame, int fpsNumerator,
                                                       int fpsDenominator) {
  if (startFrame < 0 || startFrame >= snapshot.durationFrames || fpsNumerator <= 0 || fpsDenominator <= 0)
    return std::nullopt;
  struct AudioClip { edward::core::TimelineClip clip; MediaInfo info; };
  std::vector<AudioClip> clips;
  for (const auto& clip : snapshot.clips) {
    if (clip.kind != edward::core::TimelineClipKind::Media) continue;
    const auto info = MediaProbe::probe(clip.source);
    if (info && info->hasAudio) clips.push_back({clip, *info});
  }
  if (clips.empty()) return std::nullopt;
  QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error")};
  for (const auto& audio : clips) arguments << QStringLiteral("-i") << QString::fromStdString(audio.clip.source.string());
  QStringList filters;
  for (std::size_t index = 0; index < clips.size(); ++index) {
    const auto& audio = clips[index];
    const auto sourceStart = secondsForFrames(audio.clip.sourceIn, audio.info.fpsNumerator, audio.info.fpsDenominator);
    const auto sourceEnd = secondsForFrames(audio.clip.sourceOut, audio.info.fpsNumerator, audio.info.fpsDenominator);
    const auto delay = static_cast<long long>(audio.clip.timelineStart) * 1000 * fpsDenominator / fpsNumerator;
    filters << QStringLiteral("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,adelay=%4:all=1[a%5]")
                   .arg(index).arg(sourceStart, sourceEnd).arg(delay).arg(index);
  }
  QString inputs;
  for (std::size_t index = 0; index < clips.size(); ++index) inputs += QStringLiteral("[a%1]").arg(index);
  const auto start = secondsForFrames(startFrame, fpsNumerator, fpsDenominator);
  const auto end = secondsForFrames(snapshot.durationFrames, fpsNumerator, fpsDenominator);
  filters << QStringLiteral("%1amix=inputs=%2:duration=longest:dropout_transition=0,atrim=start=%3:end=%4,asetpts=PTS-STARTPTS[mixed]")
                 .arg(inputs).arg(clips.size()).arg(start, end);
  arguments << QStringLiteral("-filter_complex") << filters.join(QLatin1Char(';')) << QStringLiteral("-map")
            << QStringLiteral("[mixed]") << QStringLiteral("-f") << QStringLiteral("f32le") << QStringLiteral("-ac")
            << QStringLiteral("2") << QStringLiteral("-ar") << QStringLiteral("48000") << QStringLiteral("pipe:1");
  return arguments;
}

bool AudioPreview::start(const edward::core::TimelineSnapshot& snapshot, edward::core::Frame startFrame,
                         int fpsNumerator, int fpsDenominator) {
  stop();
  const auto arguments = argumentsFor(snapshot, startFrame, fpsNumerator, fpsDenominator);
  if (!arguments) return false;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  audioInitialized_ = true;
  const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
  auto* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audioInitialized_ = false;
    return false;
  }
  stream_ = stream;
  SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
  process_.setProcessChannelMode(QProcess::SeparateChannels);
  process_.start(QStringLiteral("ffmpeg"), *arguments);
  if (!process_.waitForStarted(3000)) {
    stop();
    return false;
  }
  return true;
}

void AudioPreview::stop() {
  if (process_.state() != QProcess::NotRunning) {
    process_.kill();
    process_.waitForFinished(1000);
  }
  if (stream_) {
    SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
    stream_ = nullptr;
  }
  if (audioInitialized_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audioInitialized_ = false;
  }
}

void AudioPreview::pump() {
  if (!stream_ || process_.state() == QProcess::NotRunning) return;
  auto* stream = static_cast<SDL_AudioStream*>(stream_);
  constexpr int maximumQueuedBytes = 48000 * 2 * static_cast<int>(sizeof(float)) / 2;
  const auto queued = SDL_GetAudioStreamQueued(stream);
  if (queued < 0 || queued >= maximumQueuedBytes || process_.bytesAvailable() <= 0) return;
  const auto bytes = process_.read(std::min<qint64>(process_.bytesAvailable(), maximumQueuedBytes - queued));
  if (!bytes.isEmpty()) SDL_PutAudioStreamData(stream, bytes.constData(), static_cast<int>(bytes.size()));
}

bool AudioPreview::active() const { return stream_ && process_.state() != QProcess::NotRunning; }

}  // namespace edward::media
