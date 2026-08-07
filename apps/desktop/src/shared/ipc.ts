import type { Project } from '@ai-video/domain';

export type DesktopBridge = {
  projects: {
    create(project: Project): Promise<Project>;
    open(id: string): Promise<Project>;
    save(project: Project): Promise<void>;
    archive(id: string): Promise<void>;
  };
};
