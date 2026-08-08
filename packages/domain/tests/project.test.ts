import { describe, expect, it } from 'vitest';

import { createProject, ProjectSchema, setClipStyle } from '../src/index.js';

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
    expect(project.aspectRatio).toBe('16:9');
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

  it('preserves a non-empty component conversation in a project', () => {
    const project = createProject({ id: 'project-chat', scriptPath: '/a.txt', mediaPath: '/a.mp3', mediaKind: 'audio' });
    const parsed = ProjectSchema.parse({ ...project, componentConversations: { 'ai-cards-0': { modelId: 'model-1', messages: [{ role: 'user', content: '修改折线图' }, { role: 'assistant', content: '已生成数据草案' }], draftContent: { points: [1, 2] }, updatedAt: '2026-08-08T00:00:00.000Z' } } });

    expect(parsed.componentConversations?.['ai-cards-0']).toMatchObject({ modelId: 'model-1', draftContent: { points: [1, 2] } });
  });
});
