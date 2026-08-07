import { describe, expect, it } from 'vitest';
import { snapRectToGuides, type Rect } from '../src/renderer/timelineMath.js';

const bounds: Rect = { x: 0, y: 0, width: 720, height: 405 };

describe('snapRectToGuides', () => {
  it('snaps a card to the canvas and peer center guides', () => {
    const result = snapRectToGuides({ x: 304, y: 92, width: 120, height: 80 }, bounds, [{ x: 100, y: 92, width: 120, height: 80 }]);

    expect(result.rect).toEqual({ x: 300, y: 92, width: 120, height: 80 });
    expect(result.guides).toEqual({ x: [360], y: [92] });
  });

  it('keeps the card in the canvas when no guide is within the threshold', () => {
    const result = snapRectToGuides({ x: 710, y: 390, width: 120, height: 80 }, bounds, []);

    expect(result.rect).toEqual({ x: 600, y: 325, width: 120, height: 80 });
    expect(result.guides).toEqual({ x: [], y: [] });
  });
});
