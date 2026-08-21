#include "edward/media/proxy_manager.hpp"

#include "edward/media/media_probe.hpp"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>

namespace edward::media {

ProxyManager::ProxyManager(RenderStorageRoots roots, edward::core::ProjectIdentity project)
    : roots_(std::move(roots)), project_(std::move(project)) {}

std::optional<std::filesystem::path> ProxyManager::proxyPath(const std::filesystem::path& source,
                                                              PreviewQuality quality) const {
  if (source.empty() || quality == PreviewQuality::Original) return std::nullopt;
  const QFileInfo input(QString::fromStdString(source.string()));
  if (!input.exists() || !input.isFile()) return std::nullopt;
  const auto identity = input.canonicalFilePath() + QLatin1Char('|') + QString::number(input.size()) +
                        QLatin1Char('|') + QString::number(input.lastModified().toMSecsSinceEpoch()) +
                        QLatin1Char('|') + QString::number(static_cast<int>(quality));
  const auto name = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
  return RenderStorage::paths(project_, roots_).proxyDirectory /
         (name.toStdString() + (quality == PreviewQuality::Clear ? "-clear.mp4" : "-fluent.mp4"));
}

std::optional<std::filesystem::path> ProxyManager::ensureProxy(const std::filesystem::path& source,
                                                                PreviewQuality quality) const {
  const auto output = proxyPath(source, quality);
  if (!output) return std::nullopt;
  std::error_code error;
  if (std::filesystem::is_regular_file(*output, error) && !error && MediaProbe::probe(*output)) return output;
  const auto directory = output->parent_path();
  std::filesystem::create_directories(directory, error);
  if (error) return std::nullopt;
  QTemporaryFile temporary(QString::fromStdString((directory / ".edward-proxy-XXXXXX").string()));
  if (!temporary.open()) return std::nullopt;
  const auto temporaryPath = temporary.fileName();
  temporary.close();
  const auto scale = quality == PreviewQuality::Clear
      ? QStringLiteral("scale=1920:1080:force_original_aspect_ratio=decrease:force_divisible_by=2")
      : QStringLiteral("scale=854:480:force_original_aspect_ratio=decrease:force_divisible_by=2");
  QProcess ffmpeg;
  ffmpeg.start(QStringLiteral("ffmpeg"), {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
      QStringLiteral("error"), QStringLiteral("-i"), QString::fromStdString(source.string()),
      QStringLiteral("-vf"), scale, QStringLiteral("-c:v"), QStringLiteral("libx264"),
      QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), QStringLiteral("-an"),
      QStringLiteral("-f"), QStringLiteral("mp4"), QStringLiteral("-y"), temporaryPath});
  if (!ffmpeg.waitForStarted(3000) || !ffmpeg.waitForFinished(60000) || ffmpeg.exitCode() != 0 ||
      !MediaProbe::probe(std::filesystem::path(temporaryPath.toStdString()))) {
    std::filesystem::remove(temporaryPath.toStdString(), error);
    return std::nullopt;
  }
  std::filesystem::rename(temporaryPath.toStdString(), *output, error);
  if (error) {
    std::filesystem::remove(temporaryPath.toStdString(), error);
    return std::nullopt;
  }
  return output;
}

std::filesystem::path ProxyManager::sourceFor(const std::filesystem::path& source,
                                               PreviewQuality quality) const {
  const auto proxy = proxyPath(source, quality);
  if (proxy && std::filesystem::is_regular_file(*proxy)) return *proxy;
  return source;
}

}  // namespace edward::media
