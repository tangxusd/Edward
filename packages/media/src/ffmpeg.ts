export function buildAudioExtractionCommand(input: string, output: string): string[] {
  return ['-y', '-i', input, '-vn', '-ac', '1', '-ar', '16000', '-c:a', 'pcm_s16le', output];
}
