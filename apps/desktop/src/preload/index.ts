import { contextBridge, ipcRenderer } from 'electron';
import type { Project } from '@ai-video/domain';
import type { DesktopBridge } from '../shared/ipc.js';

const bridge: DesktopBridge = {
  workspace: { setRoot: (root) => ipcRenderer.invoke('workspace:set-root', root) },
  projects: {
    create: (project: Project) => ipcRenderer.invoke('projects:create', project),
    open: (id: string) => ipcRenderer.invoke('projects:open', id),
    save: (project: Project) => ipcRenderer.invoke('projects:save', project),
    archive: (id: string) => ipcRenderer.invoke('projects:archive', id),
  },
  library: {
    list: (type) => ipcRenderer.invoke('library:list', type),
    toggleFavorite: (id) => ipcRenderer.invoke('library:toggle-favorite', id),
    importStylePackage: (zipPath) => ipcRenderer.invoke('library:import-style-package', zipPath),
  },
  models: { list: () => ipcRenderer.invoke('models:list'), save: (record) => ipcRenderer.invoke('models:save', record) },
};

contextBridge.exposeInMainWorld('aiVideo', bridge);
