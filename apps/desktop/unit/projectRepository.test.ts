import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { createProject } from '@ai-video/domain';
import { afterEach, describe, expect, it } from 'vitest';
import { ProjectRepository } from '../src/main/projectRepository.js';
import { ensureWorkspace } from '../src/main/workspace.js';

describe('ProjectRepository', () => {
  let root = '';

  afterEach(async () => {
    if (root) await rm(root, { recursive: true, force: true });
    root = '';
  });

  it('rejects project ids that escape the projects directory', async () => {
    root = await mkdtemp(join(tmpdir(), 'ai-video-project-repository-'));
    const workspace = await ensureWorkspace(root);
    const escapedProject = createProject({ id: 'escaped', scriptPath: '/tmp/script.txt', mediaPath: '/tmp/media.mp3', mediaKind: 'audio' });
    await mkdir(join(root, 'escape'), { recursive: true });
    await writeFile(join(root, 'escape', 'project.json'), `${JSON.stringify(escapedProject)}\n`, 'utf8');

    await expect(new ProjectRepository(workspace).open('../escape')).rejects.toThrow('invalid project id');
  });
});
