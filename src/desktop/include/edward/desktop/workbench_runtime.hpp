#pragma once

#include "edward/desktop/timeline_controller.hpp"

#include <QObject>
#include <QVariantList>
#include <QImage>

#include "edward/media/mlt_adapter.hpp"
#include "edward/media/render_graph.hpp"

namespace edward::desktop {

class WorkbenchRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(int playheadFrame READ playheadFrame NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList clips READ clips NOTIFY timelineChanged)
  Q_PROPERTY(bool demoOverlayEnabled READ demoOverlayEnabled NOTIFY timelineChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] int playheadFrame() const;
  [[nodiscard]] QVariantList clips() const;
  [[nodiscard]] bool demoOverlayEnabled() const { return demoOverlayEnabled_; }
  [[nodiscard]] QImage previewFrame() const;
  Q_INVOKABLE bool importMedia(const QString& path);
  Q_INVOKABLE bool selectClip(qlonglong id);
  Q_INVOKABLE void toggleDemoOverlay();
  Q_INVOKABLE bool setPlayhead(int frame);
  Q_INVOKABLE bool splitSelected();
  Q_INVOKABLE bool deleteSelected();
  Q_INVOKABLE bool rippleDeleteSelected();

 signals:
  void timelineChanged();
  void operationFailed(QString message);

 private:
  edward::core::Timeline timeline_;
  edward::core::TrackId videoTrack_;
  TimelineController controller_;
  edward::media::MltAdapter mltAdapter_;
  edward::media::RenderGraph renderGraph_;
  bool demoOverlayEnabled_ = false;
};

}  // namespace edward::desktop
