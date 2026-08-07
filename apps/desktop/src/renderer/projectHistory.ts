import type { Project } from '@ai-video/domain';

export type ProjectHistory = {
  readonly current: Project;
  readonly canUndo: boolean;
  readonly canRedo: boolean;
  record(project: Project): void;
  undo(): Project | undefined;
  redo(): Project | undefined;
};

export function createProjectHistory(initial: Project): ProjectHistory {
  const entries = [initial];
  let cursor = 0;

  return {
    get current() { return entries[cursor]!; },
    get canUndo() { return cursor > 0; },
    get canRedo() { return cursor < entries.length - 1; },
    record(project) {
      if (project === entries[cursor]) return;
      entries.splice(cursor + 1);
      entries.push(project);
      cursor = entries.length - 1;
    },
    undo() {
      if (cursor === 0) return undefined;
      cursor -= 1;
      return entries[cursor];
    },
    redo() {
      if (cursor === entries.length - 1) return undefined;
      cursor += 1;
      return entries[cursor];
    },
  };
}
