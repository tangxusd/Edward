import { mkdir, readFile, rename, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { ResourceSchema, type Resource } from '@ai-video/domain';

import type { WorkspacePaths } from './workspace.js';

const ResourceIndexSchema = ResourceSchema.array();

export class LibraryRepository {
  constructor(private readonly workspace: WorkspacePaths) {}

  async list(type?: Resource['type']): Promise<Resource[]> {
    const resources = await this.readIndex();
    return type ? resources.filter((resource) => resource.type === type) : resources;
  }

  async upsert(resource: Resource): Promise<Resource> {
    const parsed = ResourceSchema.parse(resource);
    const resources = await this.readIndex();
    const next = [...resources.filter((candidate) => candidate.id !== parsed.id), parsed];
    await this.writeIndex(next);
    return parsed;
  }

  async toggleFavorite(id: string): Promise<Resource> {
    const resources = await this.readIndex();
    const resource = resources.find((candidate) => candidate.id === id);
    if (!resource) throw new Error(`resource not found: ${id}`);
    const updated = { ...resource, favorite: !resource.favorite, updatedAt: new Date().toISOString() };
    await this.writeIndex(resources.map((candidate) => candidate.id === id ? updated : candidate));
    return updated;
  }

  async remove(id: string): Promise<void> {
    const resources = await this.readIndex();
    if (!resources.some((resource) => resource.id === id)) throw new Error(`resource not found: ${id}`);
    await this.writeIndex(resources.filter((resource) => resource.id !== id));
  }

  private async readIndex(): Promise<Resource[]> {
    try {
      return ResourceIndexSchema.parse(JSON.parse(await readFile(this.indexPath(), 'utf8')));
    } catch (error: unknown) {
      if ((error as NodeJS.ErrnoException).code === 'ENOENT') return [];
      throw error;
    }
  }

  private async writeIndex(resources: Resource[]): Promise<void> {
    await mkdir(this.workspace.library, { recursive: true });
    const target = this.indexPath();
    const temporary = `${target}.tmp`;
    await writeFile(temporary, `${JSON.stringify(resources, null, 2)}\n`, 'utf8');
    await rename(temporary, target);
  }

  private indexPath(): string { return join(this.workspace.library, 'index.json'); }
}
