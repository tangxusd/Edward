import { contextBridge, ipcRenderer } from 'electron';
import type { AiEditPlan, AiPlanApplicationMode, Project } from '@ai-video/domain';
import type { ExportProgress } from '@ai-video/media';
import type { DesktopBridge } from '../shared/ipc.js';

const bridge: DesktopBridge = {
  workspace: { setRoot: (root) => ipcRenderer.invoke('workspace:set-root', root), chooseDirectory: () => ipcRenderer.invoke('workspace:choose-directory'), chooseFile: () => ipcRenderer.invoke('workspace:choose-file'), getRoot: () => ipcRenderer.invoke('workspace:get-root') },
  projects: {
    list: () => ipcRenderer.invoke('projects:list'),
    create: (project: Project) => ipcRenderer.invoke('projects:create', project),
    open: (id: string) => ipcRenderer.invoke('projects:open', id),
    save: (project: Project) => ipcRenderer.invoke('projects:save', project),
    archive: (id: string) => ipcRenderer.invoke('projects:archive', id),
  },
  library: {
    list: (type) => ipcRenderer.invoke('library:list', type),
    toggleFavorite: (id) => ipcRenderer.invoke('library:toggle-favorite', id),
    remove: (id) => ipcRenderer.invoke('library:remove', id),
    updateCategory: (id, category) => ipcRenderer.invoke('library:update-category', id, category),
    importStylePackage: (zipPath) => ipcRenderer.invoke('library:import-style-package', zipPath),
  },
  models: { list: () => ipcRenderer.invoke('models:list'), save: (record, credentialValue) => ipcRenderer.invoke('models:save', record, credentialValue), status: (id) => ipcRenderer.invoke('models:status', id) },
  analysis: {
    generate: (projectId: string, modelId: string) => ipcRenderer.invoke('analysis:generate', projectId, modelId),
    apply: (projectId: string, plan: AiEditPlan, mode?: AiPlanApplicationMode) => ipcRenderer.invoke('analysis:apply', projectId, plan, mode),
    list: (projectId: string) => ipcRenderer.invoke('analysis:list', projectId),
  },
  componentChat: { send: (projectId, clipId, modelId, message, history) => ipcRenderer.invoke('component-chat:send', projectId, clipId, modelId, message, history) },
  export: {
    start: (request) => ipcRenderer.invoke('export:start', request),
    cancel: (jobId) => ipcRenderer.invoke('export:cancel', jobId),
    onProgress: (listener) => {
      const handler = (_event: Electron.IpcRendererEvent, value: { jobId: string; progress: ExportProgress }) => listener(value);
      ipcRenderer.on('export:progress', handler);
      return () => ipcRenderer.removeListener('export:progress', handler);
    },
  },
};

contextBridge.exposeInMainWorld('aiVideo', bridge);
