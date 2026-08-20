#include "edward/media/component_renderer.hpp"

#include <QColor>
#include <QFont>
#include <QJsonArray>
#include <QPainter>

#include <algorithm>
#include <vector>

namespace edward::media {
namespace {

double number(const QJsonObject& object, const char* key, double fallback) {
  const auto value = object.value(QLatin1String(key));
  return value.isDouble() ? value.toDouble() : fallback;
}

double animatedNumber(const QJsonObject& keyframes, const QString& property, int frame, double fallback) {
  const auto frames = keyframes.value(property).toArray();
  if (frames.isEmpty()) return fallback;
  struct Point {
    int frame;
    double value;
    QString easing;
    double controlOut;
    double controlIn;
  };
  std::vector<Point> points;
  for (const auto& value : frames) {
    const auto item = value.toObject();
    if (item.value("frame").isDouble() && item.value("value").isDouble()) {
      const auto valueNumber = item.value("value").toDouble();
      points.push_back({item.value("frame").toInt(), valueNumber, item.value("easing").toString(),
                        item.value("controlOut").toDouble(valueNumber),
                        item.value("controlIn").toDouble(valueNumber)});
    }
  }
  if (points.empty()) return fallback;
  std::sort(points.begin(), points.end(), [](const Point& a, const Point& b) { return a.frame < b.frame; });
  if (frame <= points.front().frame) return points.front().value;
  if (frame >= points.back().frame) return points.back().value;
  for (size_t i = 1; i < points.size(); ++i) {
    if (frame <= points[i].frame) {
      const auto& left = points[i - 1];
      const auto& right = points[i];
      const int span = right.frame - left.frame;
      if (span <= 0) continue;
      const double t = std::clamp(static_cast<double>(frame - left.frame) / span, 0.0, 1.0);
      if (left.easing == "bezier") {
        const double c1 = left.controlOut;
        const double c2 = right.controlIn;
        const double inverse = 1.0 - t;
        return inverse * inverse * inverse * left.value +
               3.0 * inverse * inverse * t * c1 +
               3.0 * inverse * t * t * c2 + t * t * t * right.value;
      }
      return left.value + (right.value - left.value) * t;
    }
  }
  return fallback;
}

QString animatedString(const QJsonObject& keyframes, const QString& property, int frame, const QString& fallback) {
  const auto frames = keyframes.value(property).toArray();
  QString result = fallback;
  int bestFrame = -1;
  for (const auto& value : frames) {
    const auto item = value.toObject();
    const int itemFrame = item.value("frame").toInt(-1);
    if (itemFrame >= 0 && itemFrame <= frame && itemFrame >= bestFrame && item.value("value").isString()) {
      bestFrame = itemFrame;
      result = item.value("value").toString();
    }
  }
  return result;
}

QColor color(const QJsonObject& properties, const char* key, const QColor& fallback) {
  const QColor parsed(properties.value(QLatin1String(key)).toString());
  return parsed.isValid() ? parsed : fallback;
}

QColor animatedColor(const QJsonObject& properties, const QJsonObject& keyframes,
                     const char* key, int frame, const QColor& fallback) {
  const auto frames = keyframes.value(QLatin1String(key)).toArray();
  if (frames.isEmpty()) return color(properties, key, fallback);
  struct Point { int frame; QColor value; };
  std::vector<Point> points;
  for (const auto& value : frames) {
    const auto item = value.toObject();
    const QColor parsed(item.value("value").toString());
    if (item.value("frame").isDouble() && parsed.isValid())
      points.push_back({item.value("frame").toInt(), parsed});
  }
  if (points.empty()) return color(properties, key, fallback);
  std::sort(points.begin(), points.end(), [](const Point& a, const Point& b) { return a.frame < b.frame; });
  if (frame <= points.front().frame) return points.front().value;
  if (frame >= points.back().frame) return points.back().value;
  for (size_t i = 1; i < points.size(); ++i) {
    if (frame <= points[i].frame) {
      const auto& left = points[i - 1];
      const auto& right = points[i];
      const double t = static_cast<double>(frame - left.frame) / (right.frame - left.frame);
      const auto lerp = [t](int a, int b) { return static_cast<int>(a + (b - a) * t + 0.5); };
      QColor result(lerp(left.value.red(), right.value.red()), lerp(left.value.green(), right.value.green()),
                   lerp(left.value.blue(), right.value.blue()), lerp(left.value.alpha(), right.value.alpha()));
      return result;
    }
  }
  return fallback;
}

void renderNode(QPainter& painter, const edward::core::ComponentNode& node, int frame) {
  const auto& transform = node.transform;
  const auto& properties = node.properties;
  const double x = animatedNumber(node.keyframes, "x", frame, number(transform, "x", 0));
  const double y = animatedNumber(node.keyframes, "y", frame, number(transform, "y", 0));
  const double width = animatedNumber(node.keyframes, "width", frame, number(transform, "width", 0));
  const double height = animatedNumber(node.keyframes, "height", frame, number(transform, "height", 0));
  const double scaleX = animatedNumber(node.keyframes, "scaleX", frame, number(transform, "scaleX", 1));
  const double scaleY = animatedNumber(node.keyframes, "scaleY", frame, number(transform, "scaleY", 1));
  const double rotation = animatedNumber(node.keyframes, "rotation", frame, number(transform, "rotation", 0));
  const double opacity = std::clamp(animatedNumber(node.keyframes, "opacity", frame, number(properties, "opacity", 1)), 0.0, 1.0);

  painter.save();
  // Edward preview coordinates use the canvas center as (0, 0): left/up are positive.
  painter.translate(-x, -y);
  painter.rotate(-rotation);
  painter.scale(scaleX, scaleY);
  painter.setOpacity(painter.opacity() * opacity);
  const QRectF bounds(-width / 2.0, -height / 2.0, width, height);
  switch (node.type) {
    case edward::core::ComponentNodeType::Text: {
      painter.setPen(animatedColor(properties, node.keyframes, "color", frame, Qt::white));
      QFont font;
      const auto family = properties.value("fontFamily").toString();
      if (!family.isEmpty()) font.setFamily(family);
      font.setPixelSize(static_cast<int>(animatedNumber(node.keyframes, "fontSize", frame,
                                                        number(properties, "fontSize", 24))));
      painter.setFont(font);
      painter.drawText(bounds, Qt::AlignLeft | Qt::AlignTop,
                       animatedString(node.keyframes, "text", frame, properties.value("text").toString()));
      break;
    }
    case edward::core::ComponentNodeType::Shape: {
      const double borderWidth = std::max(0.0, animatedNumber(node.keyframes, "borderWidth", frame,
                                                                number(properties, "borderWidth", 0)));
      if (borderWidth > 0) {
        painter.setPen(QPen(animatedColor(properties, node.keyframes, "borderColor", frame, Qt::white), borderWidth));
      } else {
        painter.setPen(Qt::NoPen);
      }
      painter.setBrush(animatedColor(properties, node.keyframes, "fill", frame, Qt::white));
      if (properties.value("shape").toString() == "ellipse") painter.drawEllipse(bounds);
      else painter.drawRect(bounds);
      break;
    }
    case edward::core::ComponentNodeType::Image: {
      const QImage image(properties.value("source").toString());
      if (!image.isNull()) painter.drawImage(bounds, image);
      break;
    }
    case edward::core::ComponentNodeType::Container:
    case edward::core::ComponentNodeType::Svg:
      break;
  }
  for (const auto& child : node.children) renderNode(painter, child, frame);
  painter.restore();
}

}  // namespace

QImage ComponentRenderer::render(const edward::core::ComponentIr& component, int frame, const QSize& canvasSize) const {
  if (!component.validate() || canvasSize.isEmpty()) return {};
  QImage output(canvasSize, QImage::Format_RGBA8888);
  output.fill(Qt::transparent);
  QPainter painter(&output);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(canvasSize.width() / 2.0, canvasSize.height() / 2.0);
  renderNode(painter, component.root(), frame);
  return output;
}

}  // namespace edward::media
