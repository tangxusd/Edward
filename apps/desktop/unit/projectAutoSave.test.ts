import { describe, expect, it, vi } from 'vitest';
import { AUTO_SAVE_INTERVAL_MS, startProjectAutoSave } from '../src/renderer/projectAutoSave.js';

describe('startProjectAutoSave', () => {
  it('saves the current project every ten minutes', () => {
    vi.useFakeTimers();
    const save = vi.fn();
    const stop = startProjectAutoSave(() => ({ id: 'project-1' }), save);

    vi.advanceTimersByTime(AUTO_SAVE_INTERVAL_MS - 1);
    expect(save).not.toHaveBeenCalled();
    vi.advanceTimersByTime(1);
    expect(save).toHaveBeenCalledWith({ id: 'project-1' });

    stop();
    vi.useRealTimers();
  });

  it('does not save when no project is open', () => {
    vi.useFakeTimers();
    const save = vi.fn();
    const stop = startProjectAutoSave(() => undefined, save);

    vi.advanceTimersByTime(AUTO_SAVE_INTERVAL_MS);
    expect(save).not.toHaveBeenCalled();

    stop();
    vi.useRealTimers();
  });
});
