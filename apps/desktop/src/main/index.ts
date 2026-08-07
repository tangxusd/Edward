import { app, BrowserWindow, ipcMain } from 'electron';
import { join } from 'node:path';
import { ProjectSchema } from '@ai-video/domain';

import { ProjectRepository } from './projectRepository.js';
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

  ipcMain.handle('projects:create', (_event, project) => repository.create(ProjectSchema.parse(project)));
  ipcMain.handle('projects:open', (_event, id) => repository.open(String(id)));
  ipcMain.handle('projects:save', (_event, project) => repository.save(ProjectSchema.parse(project)));
  ipcMain.handle('projects:archive', (_event, id) => repository.archive(String(id)));
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
