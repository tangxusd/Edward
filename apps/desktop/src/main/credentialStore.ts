import { app, safeStorage } from 'electron';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';

export async function saveCredential(ref: string, value: string): Promise<void> {
  if (!safeStorage.isEncryptionAvailable()) throw new Error('system credential encryption is unavailable');
  const directory = join(app.getPath('userData'), 'credentials');
  await mkdir(directory, { recursive: true });
  await writeFile(join(directory, `${encodeURIComponent(ref)}.bin`), safeStorage.encryptString(value));
}

export async function readCredential(ref: string): Promise<string> {
  if (!safeStorage.isEncryptionAvailable()) throw new Error('system credential encryption is unavailable');
  return safeStorage.decryptString(await readFile(join(app.getPath('userData'), 'credentials', `${encodeURIComponent(ref)}.bin`)));
}
