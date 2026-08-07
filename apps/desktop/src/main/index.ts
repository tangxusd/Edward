import { app, BrowserWindow, ipcMain } from 'electron';
import { join } from 'node:path';
import { readFile } from 'node:fs/promises';
import { AiEditPlanSchema, ProjectSchema, applyAiPlan } from '@ai-video/domain';

import { ProjectRepository } from './projectRepository.js';
import { LibraryRepository } from './libraryRepository.js';
import { importStylePackage } from './stylePackageImporter.js';
import { ModelRepository } from './modelRepository.js';
import { startExport } from './exportService.js';
import type { ExportRequest } from '@ai-video/media';
import { ensureWorkspace } from './workspace.js';
import { readCredential, saveCredential } from './credentialStore.js';
import { transcribe } from './transcriptionService.js';
import { analyzeSemantics } from './semanticAnalysisService.js';

function createWindow(): BrowserWindow {
  const window = new BrowserWindow({
    width: 1280,
    height: 800,
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  if (process.env.ELECTRON_RENDERER_URL) {
    void window.loadURL(process.env.ELECTRON_RENDERER_URL);
  } else {
    void window.loadFile(join(__dirname, '../renderer/index.html'));
  }

  return window;
}

app.whenReady().then(() => {
  void registerProjectIpc();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

async function registerProjectIpc(): Promise<void> {
  const workspace = await ensureWorkspace(join(app.getPath('documents'), 'AI Video Editor'));
  const repository = new ProjectRepository(workspace);
  const library = new LibraryRepository(workspace);
  const models = new ModelRepository(workspace);
  const jobs = new Map<string, ReturnType<typeof startExport>>();
  ipcMain.handle('workspace:set-root', async (_event, root) => (await ensureWorkspace(String(root))).root);

  ipcMain.handle('projects:create', (_event, project) => repository.create(ProjectSchema.parse(project)));
  ipcMain.handle('projects:list', () => repository.list());
  ipcMain.handle('projects:open', (_event, id) => repository.open(String(id)));
  ipcMain.handle('projects:save', (_event, project) => repository.save(ProjectSchema.parse(project)));
  ipcMain.handle('projects:archive', (_event, id) => repository.archive(String(id)));
  ipcMain.handle('library:list', (_event, type) => library.list(type));
  ipcMain.handle('library:toggle-favorite', (_event, id) => library.toggleFavorite(String(id)));
  ipcMain.handle('library:import-style-package', (_event, zipPath) => importStylePackage(String(zipPath), workspace, library));
  ipcMain.handle('models:list', () => models.list());
  ipcMain.handle('models:save', async (_event, record, credentialValue?: string) => {
    const saved = await models.upsert(record);
    if (credentialValue) await saveCredential(saved.credentialRef, credentialValue);
    return saved;
  });
  ipcMain.handle('analysis:generate', async (_event, projectId: string, modelId: string) => {
    const project = await repository.open(String(projectId));
    const model = (await models.list()).find((candidate) => candidate.id === String(modelId));
    if (!model) throw new Error('analysis model not found');
    const [script, transcript] = await Promise.all([readFile(project.scriptPath, 'utf8'), transcribe(project.media.path)]);
    return analyzeSemantics({ baseUrl: model.baseUrl, modelId: model.modelId, apiKey: await readCredential(model.credentialRef), script, transcript });
  });
  ipcMain.handle('analysis:apply', async (_event, projectId: string, plan: unknown) => {
    const project = await repository.open(String(projectId));
    const next = applyAiPlan(project, AiEditPlanSchema.parse(plan));
    await repository.save(next);
    return next;
  });
  ipcMain.handle('export:start', (event, request: ExportRequest) => {
    const jobId = crypto.randomUUID();
    const job = startExport(request, (progress) => event.sender.send('export:progress', { jobId, progress }));
    jobs.set(jobId, job);
    void job.done.then(() => event.sender.send('export:progress', { jobId, progress: { status: 'completed' } })).catch((error: unknown) => event.sender.send('export:progress', { jobId, progress: { status: 'failed', error: String(error) } })).finally(() => jobs.delete(jobId));
    return jobId;
  });
  ipcMain.handle('export:cancel', (_event, jobId) => { jobs.get(String(jobId))?.cancel(); });
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
