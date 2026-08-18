#include "edward/media/export_job.hpp"

#include <QByteArray>
#include <QProcess>

namespace edward::media {

ExportJob::ExportJob(const RenderGraph& graph) : graph_(graph) {}

std::optional<ExportResult> ExportJob::run(const edward::core::TimelineSnapshot& snapshot,
                                           const ExportRequest& request) const {
  if (snapshot.durationFrames <= 0 || request.outputPath.empty() || request.outputSize.isEmpty() ||
      request.fpsNumerator <= 0 || request.fpsDenominator <= 0) return std::nullopt;
  std::error_code error;
  std::filesystem::create_directories(request.outputPath.parent_path(), error);
  if (error) return std::nullopt;
  std::filesystem::remove(request.outputPath, error);

  QProcess encoder;
  encoder.start(QStringLiteral("ffmpeg"), {
    QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
    QStringLiteral("-f"), QStringLiteral("rawvideo"), QStringLiteral("-pix_fmt"), QStringLiteral("rgba"),
    QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(request.outputSize.width()).arg(request.outputSize.height()),
    QStringLiteral("-framerate"), QStringLiteral("%1/%2").arg(request.fpsNumerator).arg(request.fpsDenominator),
    QStringLiteral("-i"), QStringLiteral("pipe:0"), QStringLiteral("-c:v"), QStringLiteral("libx264"),
    QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), QStringLiteral("-y"),
    QString::fromStdString(request.outputPath.string())});
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
