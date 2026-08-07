import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { afterEach, describe, expect, it } from 'vitest';

import { loadWorkspaceRoot, saveWorkspaceRoot } from '../src/main/workspaceConfig.js';

const temporaryDirectories: string[] = [];

afterEach(async () => {
  await Promise.all(temporaryDirectories.splice(0).map((directory) => rm(directory, { recursive: true, force: true })));
});

describe('workspace configuration', () => {
  it('persists and reloads the selected root', async () => {
    const directory = await mkdtemp(join(tmpdir(), 'ai-video-workspace-config-'));
    temporaryDirectories.push(directory);
    const configPath = join(directory, 'workspace.json');

    await saveWorkspaceRoot(configPath, '/Users/test/Projects');

    await expect(loadWorkspaceRoot(configPath, '/default')).resolves.toBe('/Users/test/Projects');
    await expect(readFile(configPath, 'utf8')).resolves.toContain('"root"');
  });

  it('uses the fallback for a missing or invalid config', async () => {
    const directory = await mkdtemp(join(tmpdir(), 'ai-video-workspace-config-'));
    temporaryDirectories.push(directory);
    const configPath = join(directory, 'workspace.json');

    await expect(loadWorkspaceRoot(configPath, '/default')).resolves.toBe('/default');
  });
});
