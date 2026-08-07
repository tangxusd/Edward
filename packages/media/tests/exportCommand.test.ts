import { expect, it } from 'vitest';
import { buildExportCommand, calculateExportPercent, createPartialOutputPath, parseFfmpegProgress } from '../src/index.js';

it('uses ProRes 4444 with an alpha-capable pixel format for transparent exports', () => {
  expect(buildExportCommand({
    input: '/source/input.mov',
    output: '/exports/alpha.mov',
    width: 1920,
    height: 1080,
    transparent: true,
  })).toEqual([
    '-y', '-i', '/source/input.mov', '-vf', 'scale=1920:1080',
    '-c:v', 'prores_ks', '-profile:v', '4444', '-pix_fmt', 'yuva444p10le',
    '-c:a', 'pcm_s16le', '/exports/alpha.mov',
  ]);
});

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

it('keeps partial exports in the destination directory with their media extension', () => {
  expect(createPartialOutputPath('/exports/video.mp4')).toBe('/exports/video.partial.mp4');
  expect(createPartialOutputPath('C:\\exports\\alpha.mov')).toBe('C:\\exports\\alpha.partial.mov');
});
