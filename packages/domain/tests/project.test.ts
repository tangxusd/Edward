import { describe, expect, it } from 'vitest';

import { createProject, setClipStyle } from '../src/index.js';

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

  it('replaces the audio default background without changing the input project', () => {
    const project = createProject({ id: 'project-background', scriptPath: '/a.txt', mediaPath: '/a.mp3', mediaKind: 'audio' });
    const updated = setClipStyle(project, 'default-background', 'background-2');

    expect(updated.tracks.background.clips[0]).toEqual(expect.objectContaining({ styleId: 'background-2', userEditedAt: expect.any(String) }));
    expect(project.tracks.background.clips[0].styleId).toBe('default-background');
  });
});
