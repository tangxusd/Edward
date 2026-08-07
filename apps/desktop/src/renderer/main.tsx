import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { LibraryPanel } from './LibraryPanel.js';
import { NewProjectDialog } from './NewProjectDialog.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import { ExportDialog } from './ExportDialog.js';
import { ModelSettings } from './ModelSettings.js';

const root = document.getElementById('root');

if (!root) {
  throw new Error('React root element is missing');
}

createRoot(root).render(
  <StrictMode>
    <main><h1>AI 剪视频工具</h1><NewProjectDialog /><PreviewCanvas /><SubtitleStylePanel /><ModelSettings /><ExportDialog /><LibraryPanel /></main>
  </StrictMode>,
);
