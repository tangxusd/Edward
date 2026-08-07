import { describe, expect, it } from 'vitest';

import { createProject } from '../src/index.js';

describe('createProject', () => {
  it('creates all fixed tracks and a default background for audio media', () => {
    const project = createProject({
      id: 'project-1',
      scriptPath: '/a.txt',
      mediaPath: '/a.mp3',
      mediaKind: 'audio',
      defaultBackgroundResourceId: 'background-default',
    });

    expect(Object.keys(project.tracks)).toEqual([
      'mainMedia',
      'background',
      'subtitles',
      'cards',
      'graphics',
    ]);
    expect(project.tracks.background.clips).toEqual([
      expect.objectContaining({
        id: 'default-background',
        content: { resourceId: 'background-default' },
        duration: 0,
        locked: false,
        start: 0,
      }),
    ]);
  });

  it('does not add a default background for video media', () => {
    const project = createProject({
      id: 'project-2',
      scriptPath: '/a.txt',
      mediaPath: '/a.mp4',
      mediaKind: 'video',
      defaultBackgroundResourceId: 'background-default',
    });

    expect(project.tracks.background.clips).toEqual([]);
  });
});
