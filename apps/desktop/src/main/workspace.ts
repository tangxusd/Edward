import { mkdir } from 'node:fs/promises';
import { isAbsolute, join } from 'node:path';

export type WorkspacePaths = {
  root: string;
  projects: string;
  library: string;
  cache: string;
};

export function validateWorkspaceRoot(root: string): string {
  const normalized = root.trim();
  if (!normalized) throw new Error('workspace root must not be empty');
  if (!isAbsolute(normalized)) throw new Error('workspace root must be absolute');
  return normalized;
}

export async function ensureWorkspace(root: string): Promise<WorkspacePaths> {
  const normalized = validateWorkspaceRoot(root);
  const paths: WorkspacePaths = {
    root: normalized,
    projects: join(normalized, 'projects'),
    library: join(normalized, 'library'),
    cache: join(normalized, 'cache'),
  };

  await Promise.all(Object.values(paths).map((path) => mkdir(path, { recursive: true })));
  return paths;
}
