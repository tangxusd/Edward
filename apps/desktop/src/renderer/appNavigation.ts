import type { Project } from '@ai-video/domain';

export type AppView = 'home' | 'workbench';

export interface AppState {
  view: AppView;
  project?: Project;
}

export function openProject(state: AppState, project: Project): AppState {
  if (state.view !== 'home') {
    throw new Error('Cannot switch project while in workbench');
  }
  return { view: 'workbench', project };
}

export function closeProject(state: AppState): AppState {
  return { view: 'home' };
}