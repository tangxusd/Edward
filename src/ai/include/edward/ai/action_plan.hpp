#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace edward::ai {

struct ProjectSnapshot final {
  qint64 revision = 0;
  QStringList knownTargetIds;
  QStringList verifiedResourceIds;
  QStringList knownMediaIds;
  qint64 capabilitySetVersion = -1;
  QString capabilitySetHash;
  QString referenceSnapshotId;
};

// Project facts captured when a request is sent.  The resolver only reads this
// immutable value; it never consults the live editor selection.
struct ClipReference final {
  QString id;
  QString trackId;
  qint64 startFrame = 0;
  qint64 durationFrames = 0;
  qint64 version = 0;
  bool locked = false;
  QStringList linkedClipIds;
};

struct TrackReference final {
  QString id;
  qint64 version = 0;
  bool locked = false;
};

struct MarkerReference final {
  QString markerId;
  QString scope = QStringLiteral("timeline");
  QString clipId;
  qint64 timelineFrame = 0;
  qint64 localFrame = 0;
  int displayNumber = 0;
  QString color;
  qint64 version = 0;
};

struct ReferenceSnapshot final {
  QString referenceSnapshotId;
  qint64 projectRevision = 0;
  qint64 capabilitySetVersion = 0;
  QString capabilitySetHash;
  int fps = 0;
  qint64 playheadFrame = 0;
  std::optional<qint64> inFrame;
  std::optional<qint64> outFrame;
  QStringList selectedClipIds;
  QStringList selectedTrackIds;
  QStringList selectedMediaIds;
  QStringList selectedComponentIds;
  QString currentTrackId;
  QList<ClipReference> clips;
  QList<TrackReference> tracks;
  QList<MarkerReference> markers;

  [[nodiscard]] const ClipReference* clip(const QString& id) const;
  [[nodiscard]] const TrackReference* track(const QString& id) const;
  [[nodiscard]] QList<const MarkerReference*> markersForClip(const QString& clipId) const;
};

struct ActionPlan final {
  QString schemaVersion;
  QString requestId;
  qint64 baseProjectRevision = -1;
  QString referenceSnapshotId;
  QJsonObject capabilitySet;
  QJsonArray operations;

  static std::optional<ActionPlan> parse(const QJsonObject& object, QString* error = nullptr);
  [[nodiscard]] bool validate(const ProjectSnapshot& project, QString* error = nullptr) const;
};

}  // namespace edward::ai
