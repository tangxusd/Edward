import { describe, expect, it } from 'vitest';

import {
  copyClip,
  createProject,
  deleteClip,
  moveClip,
  resizeClip,
  type Project,
} from '../src/index.js';

function createProjectWithCard(): Project {
  const project = createProject({
    id: 'project-1',
    scriptPath: '/a.txt',
    mediaPath: '/a.mp3',
    mediaKind: 'audio',
    defaultBackgroundResourceId: 'background-default',
  });

  return {
    ...project,
    tracks: {
      ...project.tracks,
      cards: {
        id: 'cards',
        clips: [
          {
            id: 'card-1',
            start: 2,
            duration: 3,
            content: { text: '重点' },
            styleId: 'card-style',
            locked: false,
          },
        ],
      },
    },
  };
}

describe('timeline operations', () => {
  it('moves a clip without changing the input project', () => {
    const project = createProjectWithCard();

    const moved = moveClip(project, 'card-1', 4);

    expect(moved.tracks.cards.clips[0].start).toBe(4);
    expect(project.tracks.cards.clips[0].start).toBe(2);
  });

  it('rejects non-positive durations without changing the input project', () => {
    const project = createProjectWithCard();

    expect(() => resizeClip(project, 'card-1', -1)).toThrow(
      'duration must be positive',
    );
    expect(project.tracks.cards.clips[0].duration).toBe(3);
  });

  it('resizes a clip without changing the input project', () => {
    const project = createProjectWithCard();

    const resized = resizeClip(project, 'card-1', 4);

    expect(resized.tracks.cards.clips[0].duration).toBe(4);
    expect(project.tracks.cards.clips[0].duration).toBe(3);
  });

  it('copies a clip without changing the input project', () => {
    const project = createProjectWithCard();

    const copied = copyClip(project, 'card-1');

    expect(copied.tracks.cards.clips).toEqual([
      expect.objectContaining({ id: 'card-1' }),
      expect.objectContaining({
        id: 'card-1-copy',
        content: { text: '重点' },
        duration: 3,
        start: 2,
      }),
    ]);
    expect(project.tracks.cards.clips).toHaveLength(1);
  });

  it('deletes a clip without changing the input project', () => {
    const project = createProjectWithCard();

    const deleted = deleteClip(project, 'card-1');

    expect(deleted.tracks.cards.clips).toEqual([]);
    expect(project.tracks.cards.clips).toHaveLength(1);
  });
});
