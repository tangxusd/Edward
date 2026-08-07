import { mkdir, readFile, rename, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';

export async function loadWorkspaceRoot(configPath: string, fallbackRoot: string): Promise<string> {
  try {
    const value = JSON.parse(await readFile(configPath, 'utf8')) as { root?: unknown };
    return typeof value.root === 'string' && value.root.trim() ? value.root : fallbackRoot;
  } catch {
    return fallbackRoot;
  }
}

export async function saveWorkspaceRoot(configPath: string, root: string): Promise<void> {
  const normalized = root.trim();
  if (!normalized) throw new Error('workspace root must not be empty');
  await mkdir(dirname(configPath), { recursive: true });
  const temporaryPath = `${configPath}.tmp`;
  await writeFile(temporaryPath, `${JSON.stringify({ root: normalized }, null, 2)}\n`, 'utf8');
  await rename(temporaryPath, configPath);
}
