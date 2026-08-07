import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { LibraryRepository } from '../src/main/libraryRepository.js';
import { ensureWorkspace } from '../src/main/workspace.js';

describe('LibraryRepository.remove', () => {
  it('removes a resource from the index and rejects unknown ids', async () => {
    const root = await mkdtemp(join(tmpdir(), 'ai-video-library-'));
    try {
      const repository = new LibraryRepository(await ensureWorkspace(root));
      await repository.upsert({ id: 'card-1', type: 'card-style', name: '卡片', category: '强调', favorite: false, style: {}, fileHash: 'hash-1', updatedAt: new Date().toISOString() });
      await repository.remove('card-1');
      await expect(repository.list()).resolves.toEqual([]);
      await expect(repository.remove('missing')).rejects.toThrow('resource not found: missing');
    } finally {
      await rm(root, { recursive: true, force: true });
    }
  });
});
