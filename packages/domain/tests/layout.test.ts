import { expect, it } from 'vitest';
import { ProjectSchema } from '../src/index.js';

it('accepts persisted canvas layout on clips', () => {
  const result = ProjectSchema.shape.tracks.shape.cards.parse({ id: 'cards', clips: [{ id: 'c', start: 0, duration: 1, content: {}, styleId: 'card', locked: false, layout: { x: 10, y: 20, width: 200, height: 100 } }] });
  expect(result.clips[0].layout?.width).toBe(200);
});
