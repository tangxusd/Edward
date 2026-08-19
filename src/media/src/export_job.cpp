#include "edward/media/export_job.hpp"
#include "edward/media/media_probe.hpp"

#include <QByteArray>
#include <QProcess>

#include <vector>

namespace edward::media {

namespace {

QString secondsForFrames(edward::core::Frame frames, int numerator, int denominator) {
  return QString::number(static_cast<double>(frames) * denominator / numerator, 'f', 6);
}

struct AudioClip {
  edward::core::TimelineClip clip;
  MediaInfo media;
};

}  // namespace

ExportJob::ExportJob(const RenderGraph& graph) : graph_(graph) {}

std::optional<ExportResult> ExportJob::run(const edward::core::TimelineSnapshot& snapshot,
                                           const ExportRequest& request) const {
  if (snapshot.durationFrames <= 0 || request.outputPath.empty() || request.outputSize.isEmpty() ||
      request.fpsNumerator <= 0 || request.fpsDenominator <= 0) return std::nullopt;
  std::error_code error;
  std::filesystem::create_directories(request.outputPath.parent_path(), error);
  if (error) return std::nullopt;
  std::filesystem::remove(request.outputPath, error);

  std::vector<AudioClip> audioClips;
  for (const auto& clip : snapshot.clips) {
    const auto media = MediaProbe::probe(clip.source);
    if (media && media->hasAudio) audioClips.push_back({clip, *media});
  }

  QStringList arguments{
    QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
    QStringLiteral("-f"), QStringLiteral("rawvideo"), QStringLiteral("-pix_fmt"), QStringLiteral("rgba"),
    QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(request.outputSize.width()).arg(request.outputSize.height()),
    QStringLiteral("-framerate"), QStringLiteral("%1/%2").arg(request.fpsNumerator).arg(request.fpsDenominator),
    QStringLiteral("-i"), QStringLiteral("pipe:0")};
  for (const auto& audio : audioClips) {
    arguments << QStringLiteral("-i") << QString::fromStdString(audio.clip.source.string());
  }

  if (!audioClips.empty()) {
    QStringList filters;
    for (std::size_t index = 0; index < audioClips.size(); ++index) {
      const auto& audio = audioClips[index];
      const auto sourceStart = secondsForFrames(audio.clip.sourceIn, audio.media.fpsNumerator, audio.media.fpsDenominator);
      const auto sourceEnd = secondsForFrames(audio.clip.sourceOut, audio.media.fpsNumerator, audio.media.fpsDenominator);
      const auto delayMilliseconds = static_cast<long long>(audio.clip.timelineStart) * 1000 * request.fpsDenominator /
        request.fpsNumerator;
      filters << QStringLiteral("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,adelay=%4:all=1[a%5]")
        .arg(index + 1).arg(sourceStart, sourceEnd).arg(delayMilliseconds).arg(index);
    }
    const auto duration = secondsForFrames(snapshot.durationFrames, request.fpsNumerator, request.fpsDenominator);
    if (audioClips.size() == 1) {
      filters << QStringLiteral("[a0]atrim=end=%1[mixed]").arg(duration);
    } else {
      QString inputs;
      for (std::size_t index = 0; index < audioClips.size(); ++index) inputs += QStringLiteral("[a%1]").arg(index);
      filters << QStringLiteral("%1amix=inputs=%2:duration=longest:dropout_transition=0,atrim=end=%3[mixed]")
        .arg(inputs).arg(audioClips.size()).arg(duration);
    }
    arguments << QStringLiteral("-filter_complex") << filters.join(QLatin1Char(';'))
      << QStringLiteral("-map") << QStringLiteral("0:v:0") << QStringLiteral("-map") << QStringLiteral("[mixed]")
      << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-shortest");
  }
  arguments << QStringLiteral("-c:v") << QStringLiteral("libx264")
    << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
    << QStringLiteral("-y") << QString::fromStdString(request.outputPath.string());

  QProcess encoder;
  encoder.start(QStringLiteral("ffmpeg"), arguments);
  if (!encoder.waitForStarted(3000)) return std::nullopt;

  for (edward::core::Frame frame = 0; frame < snapshot.durationFrames; ++frame) {
    const auto scene = graph_.build(snapshot, {frame});
    if (!scene) {
      encoder.kill();
      encoder.waitForFinished(1000);
      std::filesystem::remove(request.outputPath, error);
      return std::nullopt;
    }
    const auto image = scene->frame.scaled(request.outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
      .convertToFormat(QImage::Format_RGBA8888);
    const QByteArray pixels(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes());
    if (encoder.write(pixels) != pixels.size() || !encoder.waitForBytesWritten(5000)) {
      encoder.kill();
      encoder.waitForFinished(1000);
      std::filesystem::remove(request.outputPath, error);
      return std::nullopt;
    }
  }
  encoder.closeWriteChannel();
  if (!encoder.waitForFinished(60000) || encoder.exitStatus() != QProcess::NormalExit || encoder.exitCode() != 0 ||
      !std::filesystem::is_regular_file(request.outputPath)) {
    std::filesystem::remove(request.outputPath, error);
    return std::nullopt;
  }
  return ExportResult{request.outputPath, snapshot.durationFrames};
}

}  // namespace edward::media
