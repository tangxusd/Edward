#pragma once

#include "edward/resolve/resolve_connection.hpp"

#include <QVector>

#include <optional>

namespace edward::resolve {

struct ResolveCapabilities final {
  bool timeline = false;
  bool fusion = false;
  bool render = false;
  QString studioVersion;
};

struct ResolveTrack final {
  QString id;
  QString name;
  bool video = false;
  bool audio = false;
};

struct ResolveTimelineSnapshot final {
  QString projectName;
  QString timelineName;
  int playheadFrame = 0;
  int timelineStartFrame = 0;
  int fpsNumerator = 0;
  int fpsDenominator = 1;
  QVector<ResolveTrack> tracks;
};

class ResolveAdapter final {
 public:
  explicit ResolveAdapter(ResolveConnection& connection) : connection_(connection) {}

  bool attach(QString* error = nullptr);
  [[nodiscard]] std::optional<ResolveCapabilities> capabilities(QString* error = nullptr);
  [[nodiscard]] std::optional<ResolveTimelineSnapshot> timelineSnapshot(QString* error = nullptr);
  bool setPlayhead(int frame, QString* error = nullptr);

 private:
  ResolveConnection& connection_;
  std::optional<ResolveCapabilities> capabilities_;
};

}  // namespace edward::resolve
