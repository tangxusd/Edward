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
  Q_PROPERTY(int demoOverlayX READ demoOverlayX WRITE setDemoOverlayX NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayY READ demoOverlayY WRITE setDemoOverlayY NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayWidth READ demoOverlayWidth WRITE setDemoOverlayWidth NOTIFY timelineChanged)
  Q_PROPERTY(int demoOverlayHeight READ demoOverlayHeight WRITE setDemoOverlayHeight NOTIFY timelineChanged)
  Q_PROPERTY(double demoOverlayOpacity READ demoOverlayOpacity WRITE setDemoOverlayOpacity NOTIFY timelineChanged)
  Q_PROPERTY(QString demoOverlayText READ demoOverlayText WRITE setDemoOverlayText NOTIFY timelineChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] int playheadFrame() const;
  [[nodiscard]] QVariantList clips() const;
  [[nodiscard]] bool demoOverlayEnabled() const { return demoOverlayEnabled_; }
  [[nodiscard]] int demoOverlayX() const { return demoOverlayX_; }
  [[nodiscard]] int demoOverlayY() const { return demoOverlayY_; }
  [[nodiscard]] int demoOverlayWidth() const { return demoOverlayWidth_; }
  [[nodiscard]] int demoOverlayHeight() const { return demoOverlayHeight_; }
  [[nodiscard]] double demoOverlayOpacity() const { return demoOverlayOpacity_; }
  [[nodiscard]] QString demoOverlayText() const { return demoOverlayText_; }
  void setDemoOverlayX(int value);
  void setDemoOverlayY(int value);
  void setDemoOverlayWidth(int value);
  void setDemoOverlayHeight(int value);
  void setDemoOverlayOpacity(double value);
  void setDemoOverlayText(const QString& value);
  [[nodiscard]] QImage previewFrame() const;
  Q_INVOKABLE bool importMedia(const QString& path);
  Q_INVOKABLE bool selectClip(qlonglong id);
  Q_INVOKABLE void toggleDemoOverlay();
  Q_INVOKABLE void generateComponentDraft();
  Q_INVOKABLE bool loadComponentJson(const QString& json);
  Q_INVOKABLE void clearComponentOverlay();
  Q_INVOKABLE bool setPlayhead(int frame);
  Q_INVOKABLE bool splitSelected();
  Q_INVOKABLE bool deleteSelected();
  Q_INVOKABLE bool rippleDeleteSelected();

 signals:
  void timelineChanged();
  void operationFailed(QString message);

 private:
  void refreshDemoOverlay();
  edward::core::Timeline timeline_;
  edward::core::TrackId videoTrack_;
  TimelineController controller_;
  edward::media::MltAdapter mltAdapter_;
  edward::media::RenderGraph renderGraph_;
  bool demoOverlayEnabled_ = false;
  std::optional<edward::core::ComponentIr> demoOverlayIr_;
  int demoOverlayX_ = 24;
  int demoOverlayY_ = 24;
  int demoOverlayWidth_ = 220;
  int demoOverlayHeight_ = 72;
  double demoOverlayOpacity_ = 0.82;
  QString demoOverlayText_ = QStringLiteral("Edward Component");
};

}  // namespace edward::desktop
