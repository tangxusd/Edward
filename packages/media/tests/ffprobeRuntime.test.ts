import { expect, it } from 'vitest';

import { probeMedia } from '../src/index.js';

it('reports a missing FFprobe runtime', async () => {
  await expect(probeMedia('/media/source.mp4', '__missing_ffprobe_runtime__')).rejects.toThrow('未找到 FFprobe');
});
