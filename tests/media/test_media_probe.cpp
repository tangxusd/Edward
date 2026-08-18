#include <edward/media/media_probe.hpp>

#include <cassert>

int main(int argc, char **argv) {
  assert(!edward::media::MediaProbe::probe({}));
  assert(!edward::media::MediaProbe::probe("/definitely/missing/edward-media.mp4"));
  if (argc == 2) {
    const auto info = edward::media::MediaProbe::probe(argv[1]);
    assert(info);
    assert(info->width == 16 && info->height == 16);
    assert(info->fpsNumerator == 25 && info->fpsDenominator == 1);
    assert(info->durationFrames == 25);
  }
  return 0;
}
