import { StrictMode } from 'react';
import { useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { markClipUserEdited, setClipStyle, type Project, type Resource } from '@ai-video/domain';
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
import type { ModelRecord } from '../main/modelRepository.js';

function App(): React.JSX.Element {
  const [project, setProject] = useState<Project>(); const [selectedClipId, setSelectedClipId] = useState<string>(); const [models, setModels] = useState<ModelRecord[]>([]); const [refreshToken, setRefreshToken] = useState(0);
  const [cardStyles, setCardStyles] = useState<Resource[]>([]); const [backgrounds, setBackgrounds] = useState<Resource[]>([]); const [graphics, setGraphics] = useState<Resource[]>([]);
  const saveQueue = useRef(Promise.resolve());
  const created = (next: Project) => { setProject(next); setRefreshToken((value) => value + 1); };
  const updateProject = (next: Project) => { setProject(next); saveQueue.current = saveQueue.current.catch(() => undefined).then(() => window.aiVideo.projects.save(next)); };
  const selected = project && selectedClipId ? Object.values(project.tracks).flatMap((track) => track.clips).find((clip) => clip.id === selectedClipId) : undefined;
  const selectedText = typeof selected?.content === 'object' && selected.content !== null && 'text' in selected.content ? String(selected.content.text) : '';
  useEffect(() => { void window.aiVideo.library.list('card-style').then(setCardStyles); void window.aiVideo.library.list('background').then(setBackgrounds); Promise.all([window.aiVideo.library.list('timeline-style'), window.aiVideo.library.list('chart-style')]).then(([timelines, charts]) => setGraphics([...timelines, ...charts])); }, []);
  useEffect(() => { void window.aiVideo.models.list().then(setModels); }, []);
  return <main><header><h1>AI 剪视频工具</h1><output aria-label="当前模型状态" style={{ color: models[0] ? '#24b47e' : '#d14343' }}>{models[0] ? `${models[0].name} 在线` : '未配置模型'}</output></header><WorkspaceSettings /><ProjectList onOpen={setProject} refreshToken={refreshToken} /><NewProjectDialog onCreated={created} /><PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} /><aside aria-label="资源检查器">{selectedClipId ?? '未选择资源'}{selected && selectedText ? <label>文字内容<textarea aria-label="文字内容" value={selectedText} onChange={(event) => project && updateProject(markClipUserEdited(project, selected.id, { ...(selected.content as object), text: event.target.value }))} /></label> : null}{selected?.id.includes('cards') ? <div aria-label="卡片资源">{cardStyles.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}{selected?.id.includes('background') ? <div aria-label="背景资源">{backgrounds.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}{selected?.id.includes('graphics') ? <div aria-label="图形资源">{graphics.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}</aside><Timeline project={project} onChange={updateProject} onSelect={setSelectedClipId} /><SubtitleStylePanel project={project} onChange={updateProject} /><ModelSettings onModelsChanged={setModels} /><AiAnalysisPanel project={project} onApplied={updateProject} /><ExportDialog project={project} /><LibraryPanel /></main>;
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
