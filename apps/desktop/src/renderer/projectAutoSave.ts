export const AUTO_SAVE_INTERVAL_MS = 10 * 60 * 1000;

export function startProjectAutoSave<T>(getProject: () => T | undefined, save: (project: T) => void): () => void {
  const timer = setInterval(() => {
    const project = getProject();
    if (project) save(project);
  }, AUTO_SAVE_INTERVAL_MS);
  return () => clearInterval(timer);
}
