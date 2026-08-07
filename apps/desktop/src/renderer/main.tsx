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
  const [project, setProject] = useState<Project>(); const [refreshToken, setRefreshToken] = useState(0);
  const created = (next: Project) => { setProject(next); setRefreshToken((value) => value + 1); };
  const updateProject = (next: Project) => { setProject(next); void window.aiVideo.projects.save(next); };
  return <main><h1>AI 剪视频工具</h1><WorkspaceSettings /><ProjectList onOpen={setProject} refreshToken={refreshToken} /><NewProjectDialog onCreated={created} /><PreviewCanvas /><Timeline project={project} onChange={updateProject} /><SubtitleStylePanel /><ModelSettings /><ExportDialog project={project} /><LibraryPanel /></main>;
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
