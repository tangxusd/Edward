import { expect, it } from 'vitest';

import { transcribe } from '../src/main/transcriptionService.js';

it('reports a missing local Python runtime', async () => {
  await expect(transcribe('/media/source.mp3', undefined, '__missing_python_runtime__')).rejects.toThrow('未找到 Python 运行时');
});
