import { describe, expect, test } from 'vitest';
import { createProject } from '@ai-video/domain';

describe('project creation', () => {
  test('media path is required', () => {
    expect(() => createProject({
      mediaPath: '',
      mediaKind: 'video' as const,
      aspectRatio: '16:9' as const,
    })).toThrow();
  });

  test('creates project with media and optional document', () => {
    const project = createProject({
      scriptPath: '/tmp/script.txt',
      mediaPath: '/tmp/video.mp4',
      mediaKind: 'video',
      aspectRatio: '16:9',
    });
    expect(project.media.path).toBe('/tmp/video.mp4');
    expect(project.scriptPath).toBe('/tmp/script.txt');
  });

  test('creates project without document', () => {
    const project = createProject({
      mediaPath: '/tmp/audio.mp3',
      mediaKind: 'audio',
      aspectRatio: '16:9',
    });
    expect(project.media.path).toBe('/tmp/audio.mp3');
    expect(project.scriptPath).toBeUndefined();
  });

  test('project has valid UUID id', () => {
    const project = createProject({
      mediaPath: '/tmp/video.mp4',
      mediaKind: 'video',
      aspectRatio: '16:9',
    });
    expect(project.id).toMatch(/^project-[0-9a-f-]{36}$/);
  });

  test('autosave interval is 10 minutes', async () => {
    const { AUTO_SAVE_INTERVAL_MS } = await import('../src/renderer/projectAutoSave');
    expect(AUTO_SAVE_INTERVAL_MS).toBe(10 * 60 * 1000);
  });
});