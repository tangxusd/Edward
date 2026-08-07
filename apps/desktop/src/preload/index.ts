import { contextBridge } from 'electron';

contextBridge.exposeInMainWorld('aiVideo', {});
