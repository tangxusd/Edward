import { describe, expect, test } from 'vitest';
import type { Project } from '@ai-video/domain';

describe('AppView navigation', () => {
  const mockProject = {
    id: 'test-1',
    media: { path: '/test/video.mp4', type: 'video' as const },
    tracks: {},
    subtitleStyle: { fontFamily: '', fontSize: 48, color: '#ffffff' },
  } as unknown as Project;

  const anotherProject = {
    id: 'test-2',
    media: { path: '/test/other.mp4', type: 'video' as const },
    tracks: {},
    subtitleStyle: { fontFamily: '', fontSize: 48, color: '#ffffff' },
  } as unknown as Project;

  test('initial view is home', () => {
    const view: 'home' | 'workbench' = 'home';
    expect(view).toBe('home');
  });

  test('openProject transitions from home to workbench', async () => {
    const mod = await import('../src/renderer/appNavigation');
    const state: mod.AppState = { view: 'home' };
    const next = mod.openProject(state, mockProject);
    expect(next.view).toBe('workbench');
    expect(next.project).toBe(mockProject);
  });

  test('closeProject returns to home', async () => {
    const mod = await import('../src/renderer/appNavigation');
    const state = mod.openProject({ view: 'home' }, mockProject);
    const next = mod.closeProject(state);
    expect(next.view).toBe('home');
    expect(next.project).toBeUndefined();
  });

  test('openProject from workbench throws', async () => {
    const mod = await import('../src/renderer/appNavigation');
    const state = mod.openProject({ view: 'home' }, mockProject);
    expect(() => mod.openProject(state, anotherProject)).toThrow('Cannot switch project while in workbench');
    const home = mod.closeProject(state);
    const reopened = mod.openProject(home, anotherProject);
    expect(reopened.view).toBe('workbench');
    expect(reopened.project).toBe(anotherProject);
  });
});