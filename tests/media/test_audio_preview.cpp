#include <edward/core/timeline.hpp>
#include <edward/media/audio_preview.hpp>

#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 3);
  edward::core::Timeline timeline(100);
  const auto track = timeline.addVideoTrack();
  assert(timeline.insertClip({1, track, argv[1], 0, 25, 20}));

  const auto arguments = edward::media::AudioPreview::argumentsFor(timeline.snapshot(), 25, 25, 1);
  assert(arguments.has_value());
  assert(arguments->contains(QStringLiteral("-filter_complex")));
  assert(arguments->join(QLatin1Char(' ')).contains(QStringLiteral("atrim=start=0.000000:end=1.000000")));
  assert(arguments->join(QLatin1Char(' ')).contains(QStringLiteral("adelay=800:all=1")));
  assert(arguments->join(QLatin1Char(' ')).contains(QStringLiteral("atrim=start=1.000000:end=4.000000")));
  assert(arguments->contains(QStringLiteral("f32le")));
  assert(arguments->contains(QStringLiteral("48000")));
  assert(arguments->last() == QStringLiteral("pipe:1"));

  edward::core::Timeline silentTimeline(25);
  const auto silentTrack = silentTimeline.addVideoTrack();
  assert(silentTimeline.insertClip({1, silentTrack, argv[2], 0, 25, 0}));
  assert(!edward::media::AudioPreview::argumentsFor(silentTimeline.snapshot(), 0, 25, 1));
  return 0;
}
