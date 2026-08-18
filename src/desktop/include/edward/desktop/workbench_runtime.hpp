#pragma once

#include "edward/desktop/timeline_controller.hpp"

#include <QObject>
#include <QVariantList>

namespace edward::desktop {

class WorkbenchRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(int playheadFrame READ playheadFrame NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList clips READ clips NOTIFY timelineChanged)

 public:
  explicit WorkbenchRuntime(QObject* parent = nullptr);
  [[nodiscard]] int playheadFrame() const;
  [[nodiscard]] QVariantList clips() const;
  Q_INVOKABLE bool importMedia(const QString& path);
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
};

}  // namespace edward::desktop
