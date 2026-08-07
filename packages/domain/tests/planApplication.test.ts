import { describe, expect, it } from 'vitest';

import { applyAiPlan, createProject, markClipUserEdited, setClipStyle, type AiEditPlan } from '../src/index.js';

const initialPlan: AiEditPlan = {
  summary: '初版',
  clips: [
    { track: 'cards', start: 1, duration: 2, content: { text: '原始关键词' }, styleId: 'card-style' },
    { track: 'subtitles', start: 1, duration: 2, content: { text: '原始字幕' }, styleId: 'subtitle-style' },
  ],
};

function projectWithAiPlan() {
  return applyAiPlan(createProject({
    id: 'project-1',
    scriptPath: '/script.txt',
    mediaPath: '/media.mp3',
    mediaKind: 'audio',
  }), initialPlan, 'replace-all');
}

describe('AI plan application', () => {
  it('keeps user-edited clips when replacing only unmodified AI clips', () => {
    const edited = markClipUserEdited(projectWithAiPlan(), 'ai-cards-0', { text: '人工关键词' });
    const regenerated: AiEditPlan = {
      summary: '重生成',
      clips: [
        { track: 'cards', start: 1, duration: 2, content: { text: '新关键词' }, styleId: 'card-style' },
        { track: 'subtitles', start: 1, duration: 2, content: { text: '新字幕' }, styleId: 'subtitle-style' },
      ],
    };

    const next = applyAiPlan(edited, regenerated, 'unmodified-only');

    expect(next.tracks.cards.clips).toEqual([expect.objectContaining({ content: { text: '人工关键词' }, userEditedAt: expect.any(String) })]);
    expect(next.tracks.subtitles.clips).toEqual([expect.objectContaining({ content: { text: '新字幕' } })]);
  });

  it('only appends previously unseen AI clips in new-only mode', () => {
    const current = projectWithAiPlan();
    const regenerated: AiEditPlan = {
      summary: '增量',
      clips: [
        initialPlan.clips[0],
        { track: 'graphics', start: 2, duration: 1, content: { type: 'timeline' }, styleId: 'timeline-style' },
      ],
    };

    const next = applyAiPlan(current, regenerated, 'new-only');

    expect(next.tracks.cards.clips).toHaveLength(1);
    expect(next.tracks.graphics.clips).toEqual([expect.objectContaining({ id: 'ai-graphics-1', content: { type: 'timeline' } })]);
  });

  it('replaces a clip style without changing the input project', () => {
    const project = projectWithAiPlan();
    const updated = setClipStyle(project, 'ai-cards-0', 'new-card-style');

    expect(updated.tracks.cards.clips[0]).toEqual(expect.objectContaining({ styleId: 'new-card-style', userEditedAt: expect.any(String) }));
    expect(project.tracks.cards.clips[0].styleId).toBe('card-style');
  });
});
