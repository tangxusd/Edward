import { StrictMode } from 'react';
import { useRef, useState } from 'react';
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
import { AiAnalysisPanel } from './AiAnalysisPanel.js';

function App(): React.JSX.Element {
  const [project, setProject] = useState<Project>(); const [selectedClipId, setSelectedClipId] = useState<string>(); const [refreshToken, setRefreshToken] = useState(0);
  const saveQueue = useRef(Promise.resolve());
  const created = (next: Project) => { setProject(next); setRefreshToken((value) => value + 1); };
  const updateProject = (next: Project) => { setProject(next); saveQueue.current = saveQueue.current.catch(() => undefined).then(() => window.aiVideo.projects.save(next)); };
  return <main><h1>AI 剪视频工具</h1><WorkspaceSettings /><ProjectList onOpen={setProject} refreshToken={refreshToken} /><NewProjectDialog onCreated={created} /><PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} /><aside aria-label="资源检查器">{selectedClipId ?? '未选择资源'}</aside><Timeline project={project} onChange={updateProject} onSelect={setSelectedClipId} /><SubtitleStylePanel project={project} onChange={updateProject} /><ModelSettings /><AiAnalysisPanel project={project} onApplied={updateProject} /><ExportDialog project={project} /><LibraryPanel /></main>;
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
