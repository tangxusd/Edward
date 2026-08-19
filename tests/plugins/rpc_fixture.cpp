#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QFile>
#include <QThread>
#include <QProcess>
#include <iostream>

int main() {
  std::string line;
  if (!std::getline(std::cin, line)) return 2;
  QJsonParseError parseError;
  const auto request = QJsonDocument::fromJson(QByteArray::fromStdString(line), &parseError);
  if (parseError.error != QJsonParseError::NoError || !request.isObject()) return 3;
  const auto object = request.object();
  const auto params = object.value("params").toObject();
  const auto mode = qgetenv("EDWARD_TEST_RPC_MODE");
  if (mode == "timeout") {
    QThread::msleep(250);
    return 0;
  }
  if (mode == "crash") return 6;
  if (object.value("method").toString() == "describe") {
    const QJsonObject component{{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}};
    const QJsonObject result{{"compositionId", mode == "describe-mismatch" ? "other" : params.value("compositionId").toString()}, {"component", component},
                             {"editableProps", QJsonArray{"opacity"}}};
    const QJsonObject response{{"jsonrpc", "2.0"}, {"id", object.value("id")}, {"result", result}};
    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
  }
  if (object.value("method").toString() == "renderExport") {
    const auto outputPath = params.value("outputPath").toString();
    const int width = params.value("width").toInt();
    const int height = params.value("height").toInt();
    const int frameCount = params.value("frameCount").toInt();
    const int fps = params.value("fpsNumerator").toInt();
    const int fpsDenominator = params.value("fpsDenominator").toInt(1);
    const QString duration = QString::number(static_cast<double>(frameCount) * fpsDenominator / fps, 'f', 6);
    const auto exitCode = QProcess::execute(QStringLiteral("ffmpeg"), {
      QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
      QStringLiteral("-f"), QStringLiteral("lavfi"),
      QStringLiteral("-i"), QStringLiteral("color=c=black@0.0:s=%1x%2:r=%3:d=%4").arg(width).arg(height).arg(fps).arg(duration),
      QStringLiteral("-vf"), QStringLiteral("format=rgba"), QStringLiteral("-c:v"), QStringLiteral("prores_ks"),
      QStringLiteral("-profile:v"), QStringLiteral("4"), QStringLiteral("-pix_fmt"), QStringLiteral("yuva444p10le"),
      QStringLiteral("-an"), QStringLiteral("-y"), outputPath});
    if (exitCode != 0) return 5;
    const QJsonObject result{{"outputPath", params.value("outputPath")}, {"width", params.value("width")},
                             {"height", params.value("height")}, {"frameCount", params.value("frameCount")}, {"hasAlpha", true}};
    const QJsonObject response{{"jsonrpc", "2.0"}, {"id", object.value("id")}, {"result", result}};
    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
  }
  const int width = params.value("width").toInt();
  const int height = params.value("height").toInt();
  const int frame = params.value("frame").toInt(-1);
  if (width <= 0 || height <= 0 || frame < 0) return 4;
  QImage image(QSize(width, height), mode == "opaque" ? QImage::Format_RGB888 : QImage::Format_RGBA8888);
  image.fill(Qt::transparent);
  image.setPixelColor(0, 0, QColor(0, 255, 0, 255));
  QByteArray encoded;
  QBuffer buffer(&encoded);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  const QJsonObject result{{"frame", frame}, {"pngBase64", QString::fromUtf8(encoded.toBase64())}};
  const QJsonObject response{{"jsonrpc", "2.0"}, {"id", object.value("id")}, {"result", result}};
  std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString() << '\n';
  return 0;
}
