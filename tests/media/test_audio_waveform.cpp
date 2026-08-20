#include <edward/media/audio_waveform.hpp>

#include <algorithm>
#include <cassert>

int main(int argc, char** argv) {
  assert(argc == 3);
  assert(!edward::media::AudioWaveformExtractor::extract({}, 25));
  assert(!edward::media::AudioWaveformExtractor::extract(argv[1], 0));
  const auto waveform = edward::media::AudioWaveformExtractor::extract(argv[1], 25);
  assert(waveform);
  assert(waveform->sampleRate == 48000);
  assert(waveform->peaks.size() == 25);
  assert(*std::max_element(waveform->peaks.begin(), waveform->peaks.end()) > 0.1f);
  assert(edward::media::AudioWaveformExtractor::extract(argv[1], 12, 0.25, 0.75)->peaks.size() == 12);
  assert(!edward::media::AudioWaveformExtractor::extract(argv[1], 12, 0.75, 0.25));
  assert(!edward::media::AudioWaveformExtractor::extract(argv[2], 25));
  return 0;
}
