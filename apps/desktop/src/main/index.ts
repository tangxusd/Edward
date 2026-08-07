import { app, BrowserWindow, ipcMain } from 'electron';
import { join } from 'node:path';
import { ProjectSchema } from '@ai-video/domain';

import { ProjectRepository } from './projectRepository.js';
import { LibraryRepository } from './libraryRepository.js';
import { importStylePackage } from './stylePackageImporter.js';
import { ModelRepository } from './modelRepository.js';
import { ensureWorkspace } from './workspace.js';

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

  ipcMain.handle('projects:create', (_event, project) => repository.create(ProjectSchema.parse(project)));
  ipcMain.handle('projects:open', (_event, id) => repository.open(String(id)));
  ipcMain.handle('projects:save', (_event, project) => repository.save(ProjectSchema.parse(project)));
  ipcMain.handle('projects:archive', (_event, id) => repository.archive(String(id)));
  ipcMain.handle('library:list', (_event, type) => library.list(type));
  ipcMain.handle('library:toggle-favorite', (_event, id) => library.toggleFavorite(String(id)));
  ipcMain.handle('library:import-style-package', (_event, zipPath) => importStylePackage(String(zipPath), workspace, library));
  ipcMain.handle('models:list', () => models.list());
  ipcMain.handle('models:save', (_event, record) => models.upsert(record));
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
