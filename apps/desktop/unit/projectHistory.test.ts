import { createProject, moveClip } from '@ai-video/domain';
import { describe, expect, it } from 'vitest';

import { createProjectHistory } from '../src/renderer/projectHistory.js';

function createFixture() {
  return createProject({
    id: 'history-project',
    scriptPath: '/tmp/history-script.txt',
    mediaPath: '/tmp/history-source.mp3',
    mediaKind: 'audio',
  });
}

describe('project history', () => {
  it('restores a timeline edit and re-applies it', () => {
    const initial = createFixture();
    const edited = moveClip(initial, 'main-media', 2);
    const history = createProjectHistory(initial);

    history.record(edited);

    expect(history.canUndo).toBe(true);
    expect(history.undo()?.tracks.mainMedia.clips[0]?.start).toBe(0);
    expect(history.canRedo).toBe(true);
    expect(history.redo()?.tracks.mainMedia.clips[0]?.start).toBe(2);
  });

  it('drops the redo branch after a new edit', () => {
    const initial = createFixture();
    const firstEdit = moveClip(initial, 'main-media', 2);
    const replacementEdit = moveClip(initial, 'main-media', 4);
    const history = createProjectHistory(initial);

    history.record(firstEdit);
    history.undo();
    history.record(replacementEdit);

    expect(history.canRedo).toBe(false);
    expect(history.current.tracks.mainMedia.clips[0]?.start).toBe(4);
  });
});
