#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <iostream>

int main() {
  std::string line;
  if (!std::getline(std::cin, line)) return 2;
  QJsonParseError parseError;
  const auto request = QJsonDocument::fromJson(QByteArray::fromStdString(line), &parseError);
  if (parseError.error != QJsonParseError::NoError || !request.isObject()) return 3;
  const auto object = request.object();
  const auto params = object.value("params").toObject();
  if (object.value("method").toString() == "renderExport") {
    const QJsonObject result{{"outputPath", params.value("outputPath")}, {"width", params.value("width")},
                             {"height", params.value("height")}, {"frameCount", 24}, {"hasAlpha", true}};
    const QJsonObject response{{"jsonrpc", "2.0"}, {"id", object.value("id")}, {"result", result}};
    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
  }
  const int width = params.value("width").toInt();
  const int height = params.value("height").toInt();
  const int frame = params.value("frame").toInt(-1);
  if (width <= 0 || height <= 0 || frame < 0) return 4;
  QImage image(QSize(width, height), QImage::Format_RGBA8888);
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
