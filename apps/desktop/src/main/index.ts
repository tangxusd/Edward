import { app, BrowserWindow, dialog, ipcMain, screen } from 'electron';
import { join } from 'node:path';
import { readFile } from 'node:fs/promises';
import { AiPlanApplicationModeSchema, AiEditPlanSchema, ProjectSchema, applyAiPlan } from '@ai-video/domain';

import { ProjectRepository } from './projectRepository.js';
import { LibraryRepository } from './libraryRepository.js';
import { importStylePackage } from './stylePackageImporter.js';
import { ModelRepository } from './modelRepository.js';
import { startExport } from './exportService.js';
import { probeMedia, type ExportRequest } from '@ai-video/media';
import { ensureWorkspace } from './workspace.js';
import { readCredential, saveCredential } from './credentialStore.js';
import { transcribe } from './transcriptionService.js';
import { analyzeSemantics, requestComponentContentEdit } from './semanticAnalysisService.js';
import { checkModelAvailability } from './modelAvailabilityService.js';
import { loadWorkspaceRoot, saveWorkspaceRoot } from './workspaceConfig.js';

function createWindow(): BrowserWindow {
  const { width: screenWidth, height: screenHeight } = screen.getPrimaryDisplay().workAreaSize;
  const window = new BrowserWindow({
    width: Math.round(screenWidth / 2),
    height: Math.round(screenHeight / 2),
    center: true,
    resizable: false,
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

let mainWindow: BrowserWindow | null = null;
let homeBounds: { x: number; y: number; width: number; height: number } | null = null;

app.whenReady().then(() => {
  mainWindow = createWindow();
  void registerProjectIpc();
  registerWindowIpc();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      mainWindow = createWindow();
    }
  });
});

function registerWindowIpc(): void {
  ipcMain.handle('window:enter-workbench', () => {
    if (!mainWindow) return;
    homeBounds = mainWindow.getBounds();
    mainWindow.setResizable(true);
    mainWindow.maximize();
  });

  ipcMain.handle('window:exit-workbench', () => {
    if (!mainWindow || !homeBounds) return;
    mainWindow.unmaximize();
    mainWindow.setResizable(false);
    mainWindow.setBounds(homeBounds);
  });
}

async function registerProjectIpc(): Promise<void> {
  const configPath = join(app.getPath('userData'), 'workspace.json');
  const defaultRoot = join(app.getPath('documents'), 'AI Video Editor');
  let workspace = await ensureWorkspace(await loadWorkspaceRoot(configPath, defaultRoot));
  let repository = new ProjectRepository(workspace);
  let library = new LibraryRepository(workspace);
  let models = new ModelRepository(workspace);
  const jobs = new Map<string, ReturnType<typeof startExport>>();
  ipcMain.handle('workspace:set-root', async (_event, root) => {
    workspace = await ensureWorkspace(String(root));
    await saveWorkspaceRoot(configPath, workspace.root);
    repository = new ProjectRepository(workspace);
    library = new LibraryRepository(workspace);
    models = new ModelRepository(workspace);
    return workspace.root;
  });
  ipcMain.handle('workspace:get-root', () => workspace.root);
  ipcMain.handle('workspace:choose-directory', async () => {
    const result = await dialog.showOpenDialog({ properties: ['openDirectory', 'createDirectory'] });
    return result.canceled ? undefined : result.filePaths[0];
  });
  ipcMain.handle('workspace:choose-file', async () => {
    const result = await dialog.showOpenDialog({ properties: ['openFile'] });
    return result.canceled ? undefined : result.filePaths[0];
  });

  ipcMain.handle('projects:create', (_event, project) => repository.create(ProjectSchema.parse(project)));
  ipcMain.handle('projects:list', () => repository.list());
  ipcMain.handle('projects:open', (_event, id) => repository.open(String(id)));
  ipcMain.handle('projects:save', (_event, project) => repository.save(ProjectSchema.parse(project)));
  ipcMain.handle('projects:archive', (_event, id) => repository.archive(String(id)));
  ipcMain.handle('library:list', (_event, type) => library.list(type));
  ipcMain.handle('library:toggle-favorite', (_event, id) => library.toggleFavorite(String(id)));
  ipcMain.handle('library:remove', (_event, id) => library.remove(String(id)));
  ipcMain.handle('library:update-category', (_event, id, category) => library.updateCategory(String(id), String(category)));
  ipcMain.handle('library:import-style-package', (_event, zipPath) => importStylePackage(String(zipPath), workspace, library));
  ipcMain.handle('models:list', () => models.list());
  ipcMain.handle('models:save', async (_event, record, credentialValue?: string) => {
    const saved = await models.upsert(record);
    if (credentialValue) await saveCredential(saved.credentialRef, credentialValue);
    return saved;
  });
  ipcMain.handle('models:status', async (_event, id) => {
    const model = (await models.list()).find((candidate) => candidate.id === String(id));
    if (!model) return { online: false };
    return checkModelAvailability({ baseUrl: model.baseUrl, apiKey: await readCredential(model.credentialRef) });
  });
  ipcMain.handle('analysis:generate', async (_event, projectId: string, modelId: string) => {
    const project = await repository.open(String(projectId));
    const model = (await models.list()).find((candidate) => candidate.id === String(modelId));
    if (!model) throw new Error('analysis model not found');
    const [script, transcript] = await Promise.all([readFile(project.scriptPath, 'utf8'), transcribe(project.media.path)]);
    const plan = await analyzeSemantics({ baseUrl: model.baseUrl, modelId: model.modelId, apiKey: await readCredential(model.credentialRef), script, transcript });
    await repository.saveAiPlan(project.id, plan);
    return plan;
  });
  ipcMain.handle('analysis:apply', async (_event, projectId: string, plan: unknown, mode: unknown) => {
    const project = await repository.open(String(projectId));
    const next = applyAiPlan(project, AiEditPlanSchema.parse(plan), mode === undefined ? 'unmodified-only' : AiPlanApplicationModeSchema.parse(mode));
    await repository.save(next);
    return next;
  });
  ipcMain.handle('analysis:list', (_event, projectId: string) => repository.listAiPlans(String(projectId)));
  ipcMain.handle('component-chat:send', async (_event, projectId: string, clipId: string, modelId: string, message: string, history: Array<{ role: 'user' | 'assistant'; content: string }>) => {
    const project = await repository.open(String(projectId));
    const clip = [...project.tracks.cards.clips, ...project.tracks.graphics.clips, ...project.tracks.subtitles.clips].find((candidate) => candidate.id === String(clipId));
    if (!clip) throw new Error('component not found');
    const model = (await models.list()).find((candidate) => candidate.id === String(modelId));
    if (!model) throw new Error('component chat model not found');
    const script = await readFile(project.scriptPath, 'utf8');
    return requestComponentContentEdit({ baseUrl: model.baseUrl, modelId: model.modelId, apiKey: await readCredential(model.credentialRef), script, componentType: clip.styleId, content: clip.content, message: String(message), history });
  });
  ipcMain.handle('export:start', async (event, request: ExportRequest) => {
    const jobId = crypto.randomUUID();
    const media = await probeMedia(request.input);
    const job = startExport({ ...request, durationMs: media.durationMs }, (progress) => event.sender.send('export:progress', { jobId, progress }));
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
