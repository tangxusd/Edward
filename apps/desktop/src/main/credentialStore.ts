import { app } from 'electron';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';

export async function saveCredential(ref: string, value: string): Promise<void> {
  const directory = join(app.getPath('userData'), 'credentials');
  await mkdir(directory, { recursive: true });
  await writeFile(join(directory, `${encodeURIComponent(ref)}.json`), JSON.stringify({ ref, value }) + '\n', 'utf8');
}

export async function readCredential(ref: string): Promise<string> {
  const stored = JSON.parse(await readFile(join(app.getPath('userData'), 'credentials', `${encodeURIComponent(ref)}.json`), 'utf8')) as { ref: string; value: string };
  if (stored.ref !== ref) throw new Error('credential reference mismatch');
  return stored.value;
}
