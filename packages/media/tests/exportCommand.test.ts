import { expect, it } from 'vitest';
import { calculateExportPercent, parseFfmpegProgress } from '../src/index.js';

it('parses FFmpeg elapsed time in milliseconds', () => {
  expect(parseFfmpegProgress('frame=  120 fps=30.0 time=00:01:15.50 speed=1.2x')).toMatchObject({
    frame: 120,
    time: '00:01:15.50',
    timeMs: 75500,
    speed: '1.2x',
  });
});

it('caps in-progress percentages before FFmpeg completion', () => {
  expect(calculateExportPercent(5000, 10000)).toBe(50);
  expect(calculateExportPercent(10000, 10000)).toBe(99);
});
