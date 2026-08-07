import type { Project } from '@ai-video/domain';
import type { Resource } from '@ai-video/domain';
import type { ModelRecord } from '../main/modelRepository.js';
import type { ExportProgress, ExportRequest } from '@ai-video/media';

export type DesktopBridge = {
  workspace: { setRoot(root: string): Promise<string>; };
  projects: {
    list(): Promise<Project[]>;
    create(project: Project): Promise<Project>;
    open(id: string): Promise<Project>;
    save(project: Project): Promise<void>;
    archive(id: string): Promise<void>;
  };
  library: {
    list(type?: Resource['type']): Promise<Resource[]>;
    toggleFavorite(id: string): Promise<Resource>;
    importStylePackage(zipPath: string): Promise<Resource>;
  };
  models: { list(): Promise<ModelRecord[]>; save(record: ModelRecord): Promise<ModelRecord>; };
  export: { start(request: ExportRequest): Promise<string>; cancel(jobId: string): Promise<void>; onProgress(listener: (event: { jobId: string; progress: ExportProgress }) => void): () => void; };
};
