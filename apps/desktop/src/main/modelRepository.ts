import { readFile, rename, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { z } from 'zod';
import type { WorkspacePaths } from './workspace.js';

export const ModelRecordSchema = z.object({ id: z.string().min(1), name: z.string().min(1), baseUrl: z.string().url(), modelId: z.string().min(1), credentialRef: z.string().min(1) });
export type ModelRecord = z.infer<typeof ModelRecordSchema>;

export class ModelRepository {
  constructor(private readonly workspace: WorkspacePaths) {}
  async list(): Promise<ModelRecord[]> {
    try { return ModelRecordSchema.array().parse(JSON.parse(await readFile(this.path(), 'utf8'))); }
    catch (error: unknown) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return []; throw error; }
  }
  async upsert(record: ModelRecord): Promise<ModelRecord> {
    const parsed = ModelRecordSchema.parse(record); const all = await this.list(); const target = this.path();
    await writeFile(`${target}.tmp`, JSON.stringify([...all.filter((item) => item.id !== parsed.id), parsed], null, 2));
    await rename(`${target}.tmp`, target); return parsed;
  }
  private path(): string { return join(this.workspace.root, 'models.json'); }
}
