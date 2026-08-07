import { createHash } from 'node:crypto';
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, extname, join } from 'node:path';
import AdmZip from 'adm-zip';
import { parseStylePackageManifest, type Resource } from '@ai-video/domain';

import { LibraryRepository } from './libraryRepository.js';
import type { WorkspacePaths } from './workspace.js';

const allowedExtensions = new Set(['.css', '.json', '.png', '.jpg', '.jpeg', '.webp', '.svg', '.woff', '.woff2', '.ttf', '.otf']);

export async function importStylePackage(zipPath: string, workspace: WorkspacePaths, library: LibraryRepository): Promise<Resource> {
  const zip = new AdmZip(zipPath);
  const manifestEntry = zip.getEntry('manifest.json');
  if (!manifestEntry) throw new Error('style package is missing manifest.json');
  const manifest = parseStylePackageManifest(JSON.parse(manifestEntry.getData().toString('utf8')));
  const declared = new Set(['manifest.json', ...manifest.assets, ...(manifest.preview ? [manifest.preview] : [])]);
  const entries = zip.getEntries().filter((entry) => !entry.isDirectory);
  for (const entry of entries) {
    if (!declared.has(entry.entryName)) throw new Error(`undeclared package file: ${entry.entryName}`);
    if (!allowedExtensions.has(extname(entry.entryName).toLowerCase())) throw new Error(`unsupported package file: ${entry.entryName}`);
  }
  const target = join(workspace.library, manifest.type, manifest.id);
  await mkdir(target, { recursive: true });
  for (const entry of entries) {
    const output = join(target, entry.entryName);
    await mkdir(dirname(output), { recursive: true });
    await writeFile(output, entry.getData());
  }
  const hash = createHash('sha256');
  for (const entry of entries) hash.update(entry.entryName).update(entry.getData());
  const fileHash = hash.digest('hex');
  const type = manifest.type === 'chart' ? 'chart-style' : manifest.type === 'style-pack' ? 'card-style' : manifest.type;
  return library.upsert({ id: manifest.id, type, name: manifest.name, category: manifest.category, thumbnailPath: manifest.preview ? join(target, manifest.preview) : undefined, favorite: false, style: manifest, fileHash, updatedAt: new Date().toISOString() });
}
