import { expect, it } from 'vitest';

import { startExport } from '../src/main/exportService.js';

it('reports a missing FFmpeg runtime', async () => {
  const job = startExport({ input: '/media/source.mp4', output: '/exports/result.mp4', width: 1280, height: 720, transparent: false }, () => undefined, '__missing_ffmpeg_runtime__');
  await expect(job.done).rejects.toThrow('未找到 FFmpeg');
});
