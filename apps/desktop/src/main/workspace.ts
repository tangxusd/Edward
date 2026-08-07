import { mkdir } from 'node:fs/promises';
import { join } from 'node:path';

export type WorkspacePaths = {
  root: string;
  projects: string;
  library: string;
  cache: string;
};

export async function ensureWorkspace(root: string): Promise<WorkspacePaths> {
  const paths: WorkspacePaths = {
    root,
    projects: join(root, 'projects'),
    library: join(root, 'library'),
    cache: join(root, 'cache'),
  };

  await Promise.all(Object.values(paths).map((path) => mkdir(path, { recursive: true })));
  return paths;
}
