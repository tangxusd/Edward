import { mkdir, readFile, readdir, rename, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { randomUUID } from 'node:crypto';
import { AiEditPlanSchema, ProjectSchema, type AiEditPlan, type Project } from '@ai-video/domain';

import type { WorkspacePaths } from './workspace.js';

const projectFiles = ['media', 'transcripts', 'ai-plans', 'previews', 'exports'];

export class ProjectRepository {
  constructor(private readonly workspace: WorkspacePaths) {}

  async create(project: Project): Promise<Project> {
    const parsed = ProjectSchema.parse(project);
    const directory = this.projectDirectory(parsed.id);
    await Promise.all(projectFiles.map((name) => mkdir(join(directory, name), { recursive: true })));
    await this.save(parsed);
    return parsed;
  }

  async open(id: string): Promise<Project> {
    return ProjectSchema.parse(JSON.parse(await readFile(this.projectFile(id), 'utf8')));
  }

  async list(): Promise<Project[]> {
    const entries = await readdir(this.workspace.projects, { withFileTypes: true });
    const projects: Project[] = [];
    for (const entry of entries) {
      if (!entry.isDirectory()) continue;
      try { projects.push(await this.open(entry.name)); } catch { /* ignore incomplete project folders */ }
    }
    return projects.filter((project) => !project.archivedAt);
  }

  async save(project: Project): Promise<void> {
    const parsed = ProjectSchema.parse(project);
    const directory = this.projectDirectory(parsed.id);
    await mkdir(directory, { recursive: true });
    const target = this.projectFile(parsed.id);
    const temporary = `${target}.tmp`;
    await writeFile(temporary, `${JSON.stringify(parsed, null, 2)}\n`, 'utf8');
    await rename(temporary, target);
  }

  async archive(id: string): Promise<void> {
    const project = await this.open(id);
    await this.save({ ...project, archivedAt: new Date().toISOString() } as Project);
  }

  async saveAiPlan(projectId: string, plan: AiEditPlan): Promise<void> {
    const directory = join(this.projectDirectory(projectId), 'ai-plans');
    await mkdir(directory, { recursive: true });
    await writeFile(join(directory, `${Date.now()}-${randomUUID()}.json`), `${JSON.stringify(AiEditPlanSchema.parse(plan), null, 2)}\n`, 'utf8');
  }

  private projectDirectory(id: string): string {
    if (!id || id === '.' || id === '..' || /[\\/\0]/.test(id)) throw new Error('invalid project id');
    return join(this.workspace.projects, id);
  }

  private projectFile(id: string): string {
    return join(this.projectDirectory(id), 'project.json');
  }
}
