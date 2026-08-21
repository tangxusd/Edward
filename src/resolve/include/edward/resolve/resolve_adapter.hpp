#pragma once

#include "edward/resolve/resolve_connection.hpp"
#include "edward/core/component_ir.hpp"

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

struct ResolveRenderOptions final {
  QString outputPath;
  int width = 0;
  int height = 0;
  int fps = 0;
  QString codec;
  int quality = 0;
};

enum class ResolveRenderState { Unknown, Queued, Rendering, Completed, Failed, Canceled };

struct ResolveRenderStatus final {
  QString jobId;
  ResolveRenderState state = ResolveRenderState::Unknown;
  int progress = 0;
  QString error;
};

class ResolveAdapter final {
 public:
  explicit ResolveAdapter(ResolveConnection& connection) : connection_(connection) {}

  bool attach(QString* error = nullptr);
  [[nodiscard]] std::optional<ResolveCapabilities> capabilities(QString* error = nullptr);
  [[nodiscard]] std::optional<ResolveTimelineSnapshot> timelineSnapshot(QString* error = nullptr);
  bool setPlayhead(int frame, QString* error = nullptr);
  bool insertSubtitleComponent(const edward::core::ComponentIr& component,
                               const ResolveTimelineSnapshot& snapshot,
                               QString* error = nullptr);
  bool setComponentKeyframe(const QString& componentId, const QString& nodeId,
                            const QString& field, int frame, double value,
                            QString* error = nullptr);
  bool queueAndStartRender(const ResolveRenderOptions& options, QString* jobId,
                           QString* error = nullptr);
  [[nodiscard]] ResolveRenderStatus renderStatus(const QString& jobId,
                                                 QString* error = nullptr);
  bool cancelRender(const QString& jobId, QString* error = nullptr);
  bool openDeliverPage(QString* error = nullptr);
  [[nodiscard]] QString lastInsertedComponentId() const { return lastInsertedComponentId_; }

 private:
  ResolveConnection& connection_;
  std::optional<ResolveCapabilities> capabilities_;
  QString lastInsertedComponentId_;
};

}  // namespace edward::resolve
