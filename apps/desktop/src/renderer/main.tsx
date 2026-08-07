import { StrictMode } from 'react';
import { useState } from 'react';
import { createRoot } from 'react-dom/client';
import type { Project } from '@ai-video/domain';
import { LibraryPanel } from './LibraryPanel.js';
import { NewProjectDialog } from './NewProjectDialog.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import { ExportDialog } from './ExportDialog.js';
import { ModelSettings } from './ModelSettings.js';
import { WorkspaceSettings } from './WorkspaceSettings.js';
import { ProjectList } from './ProjectList.js';
import { Timeline } from './Timeline.js';

function App(): React.JSX.Element {
  const [project, setProject] = useState<Project>();
  return <main><h1>AI 剪视频工具</h1><WorkspaceSettings /><ProjectList onOpen={setProject} /><NewProjectDialog onCreated={setProject} /><PreviewCanvas /><Timeline project={project} /><SubtitleStylePanel /><ModelSettings /><ExportDialog /><LibraryPanel /></main>;
}

const root = document.getElementById('root');

if (!root) {
  throw new Error('React root element is missing');
}

createRoot(root).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
