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

it('renders timed text overlays in the export filter graph', () => {
  expect(buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '重点', start: 1, duration: 2 }] })).toContain('-filter_complex');
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '重点', start: 1, duration: 2 }] }).join(' ');
  expect(command).toContain("drawtext=text='重点'");
  expect(command).toContain("enable='between(t,1,3)'");
});

it('applies text color and size to timed overlays', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '标题', start: 0, duration: 1, color: '#ff0000', fontSize: 42 }] }).join(' ');
  expect(command).toContain("fontcolor='#ff0000'");
  expect(command).toContain('fontsize=42');
});

it('centers timed overlays near the lower edge', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '字幕', start: 0, duration: 1 }] }).join(' ');
  expect(command).toContain('x=(w-text_w)/2');
  expect(command).toContain('y=h*0.8');
});

it('renders a background box behind timed overlays', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '字幕', start: 0, duration: 1, background: 'black@0.55' }] }).join(' ');
  expect(command).toContain('box=1');
  expect(command).toContain("boxcolor='black@0.55'");
});

it('converts CSS rgba text backgrounds for FFmpeg', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '字幕', start: 0, duration: 1, background: 'rgba(0,0,0,.55)' }] }).join(' ');
  expect(command).toContain("boxcolor='0x000000@0.55'");
});

it('uses explicit overlay coordinates when provided', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '卡片', start: 0, duration: 1, x: 120, y: 180 }] }).join(' ');
  expect(command).toContain('x=120:y=180');
});

it('escapes drawtext option delimiters in overlay text', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '时间：10:30', start: 0, duration: 1 }] }).join(' ');
  expect(command).toContain("text='时间：10\\:30'");
});

it('preserves line breaks in drawtext overlays', () => {
  const command = buildExportCommand({ input: '/source/input.mp4', output: '/exports/video.mp4', width: 1280, height: 720, transparent: false, overlays: [{ text: '第一行\n第二行', start: 0, duration: 1 }] }).join(' ');
  expect(command).toContain(String.raw`text='第一行\\n第二行'`);
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
