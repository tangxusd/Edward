import { contextBridge, ipcRenderer } from 'electron';
import type { Project } from '@ai-video/domain';
import type { DesktopBridge } from '../shared/ipc.js';

const bridge: DesktopBridge = {
  projects: {
    create: (project: Project) => ipcRenderer.invoke('projects:create', project),
    open: (id: string) => ipcRenderer.invoke('projects:open', id),
    save: (project: Project) => ipcRenderer.invoke('projects:save', project),
    archive: (id: string) => ipcRenderer.invoke('projects:archive', id),
  },
};

contextBridge.exposeInMainWorld('aiVideo', bridge);
