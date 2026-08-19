#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QFile>
#include <QThread>
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
    QFile output(params.value("outputPath").toString());
    if (!output.open(QIODevice::WriteOnly)) return 5;
    output.write("edward-plugin-export");
    output.close();
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
