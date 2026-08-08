import { StrictMode } from 'react';
import { useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { markClipUserEdited, setClipStyle, type Project, type Resource } from '@ai-video/domain';
import { LibraryPanel } from './LibraryPanel.js';
import { NewProjectDialog } from './NewProjectDialog.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import { ExportDialog } from './ExportDialog.js';
import { SettingsDialog } from './SettingsDialog.js';
import { ProjectList } from './ProjectList.js';
import { Timeline } from './Timeline.js';
import { AiAnalysisPanel } from './AiAnalysisPanel.js';
import { createProjectHistory, type ProjectHistory } from './projectHistory.js';
import type { ModelRecord } from '../main/modelRepository.js';

function App(): React.JSX.Element {
  const [project, setProject] = useState<Project>(); const [selectedClipId, setSelectedClipId] = useState<string>(); const [models, setModels] = useState<ModelRecord[]>([]); const [modelOnline, setModelOnline] = useState<boolean>(); const [refreshToken, setRefreshToken] = useState(0); const [historyRevision, setHistoryRevision] = useState(0);
  const [cardStyles, setCardStyles] = useState<Resource[]>([]); const [backgrounds, setBackgrounds] = useState<Resource[]>([]); const [graphics, setGraphics] = useState<Resource[]>([]);
  const saveQueue = useRef(Promise.resolve());
  const history = useRef<ProjectHistory>();
  const workspaceDataVersion = useRef(0);
  const openProject = (next: Project) => { history.current = createProjectHistory(next); setProject(next); setHistoryRevision((value) => value + 1); };
  const created = (next: Project) => { openProject(next); setRefreshToken((value) => value + 1); };
  const saveProject = (next: Project) => { saveQueue.current = saveQueue.current.catch(() => undefined).then(() => window.aiVideo.projects.save(next)); };
  const updateProject = (next: Project) => { history.current?.record(next); setProject(next); setHistoryRevision((value) => value + 1); saveProject(next); };
  const undo = () => { const previous = history.current?.undo(); if (!previous) return; setProject(previous); setHistoryRevision((value) => value + 1); saveProject(previous); };
  const redo = () => { const next = history.current?.redo(); if (!next) return; setProject(next); setHistoryRevision((value) => value + 1); saveProject(next); };
  const selected = project && selectedClipId ? Object.values(project.tracks).flatMap((track) => track.clips).find((clip) => clip.id === selectedClipId) : undefined;
  const selectedText = typeof selected?.content === 'object' && selected.content !== null && 'text' in selected.content ? String(selected.content.text) : '';
  const reloadWorkspaceData = () => { const version = workspaceDataVersion.current + 1; workspaceDataVersion.current = version; void window.aiVideo.library.list('card-style').then((resources) => { if (workspaceDataVersion.current === version) setCardStyles(resources); }); void window.aiVideo.library.list('background').then((resources) => { if (workspaceDataVersion.current === version) setBackgrounds(resources); }); void Promise.all([window.aiVideo.library.list('timeline-style'), window.aiVideo.library.list('chart-style')]).then(([timelines, charts]) => { if (workspaceDataVersion.current === version) setGraphics([...timelines, ...charts]); }); void window.aiVideo.models.list().then((nextModels) => { if (workspaceDataVersion.current === version) setModels(nextModels); }); };
  useEffect(() => { reloadWorkspaceData(); }, []);
  useEffect(() => { if (!models[0]) { setModelOnline(undefined); return; } void window.aiVideo.models.status(models[0].id).then(({ online }) => setModelOnline(online)).catch(() => setModelOnline(false)); }, [models]);
  const modelStatus = !models[0] ? '未配置模型' : modelOnline === undefined ? `${models[0].name} 检查中` : `${models[0].name} ${modelOnline ? '在线' : '离线'}`;
  const modelColor = !models[0] || modelOnline === false ? '#d14343' : modelOnline ? '#24b47e' : '#59636e';
  const selectedTextStyle = typeof selected?.content === 'object' && selected.content !== null && 'textStyle' in selected.content ? (selected.content.textStyle as { fontFamily?: string; fontSize?: number; color?: string; background?: string }) : {};
  const updateSelectedTextStyle = (field: keyof typeof selectedTextStyle, value: string | number) => project && selected && updateProject(markClipUserEdited(project, selected.id, { ...(selected.content as Record<string, unknown>), textStyle: { ...selectedTextStyle, [field]: value } }));
  const workspaceChanged = () => { history.current = undefined; setProject(undefined); setSelectedClipId(undefined); setHistoryRevision((value) => value + 1); setModels([]); setModelOnline(undefined); setCardStyles([]); setBackgrounds([]); setGraphics([]); setRefreshToken((value) => value + 1); reloadWorkspaceData(); };
  return <main><header data-history-revision={historyRevision}><h1>AI 剪视频工具</h1><button aria-label="撤销" title="撤销" type="button" disabled={!history.current?.canUndo} onClick={undo}>撤销</button><button aria-label="重做" title="重做" type="button" disabled={!history.current?.canRedo} onClick={redo}>重做</button><output aria-label="当前模型状态" style={{ color: modelColor }}>{modelStatus}</output></header><SettingsDialog onWorkspaceChanged={workspaceChanged} onModelsChanged={setModels} /><ProjectList onOpen={openProject} refreshToken={refreshToken} /><NewProjectDialog onCreated={created} /><PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} /><aside aria-label="资源检查器">{selectedClipId ?? '未选择资源'}{selected && selectedText ? <label>文字内容<textarea aria-label="文字内容" value={selectedText} onChange={(event) => project && updateProject(markClipUserEdited(project, selected.id, { ...(selected.content as object), text: event.target.value }))} /></label> : null}{selected?.id.includes('subtitles') ? <fieldset aria-label="局部字幕样式"><label>局部字体<input aria-label="局部字体" value={selectedTextStyle.fontFamily ?? project?.subtitleStyle.fontFamily ?? ''} onChange={(event) => updateSelectedTextStyle('fontFamily', event.target.value)} /></label><label>局部字号<input aria-label="局部字号" type="number" value={selectedTextStyle.fontSize ?? project?.subtitleStyle.fontSize ?? 48} onChange={(event) => updateSelectedTextStyle('fontSize', Number(event.target.value))} /></label><label>局部颜色<input aria-label="局部颜色" type="color" value={selectedTextStyle.color ?? project?.subtitleStyle.color ?? '#ffffff'} onChange={(event) => updateSelectedTextStyle('color', event.target.value)} /></label><label>局部文字背景<input aria-label="局部文字背景" value={selectedTextStyle.background ?? project?.subtitleStyle.background ?? ''} onChange={(event) => updateSelectedTextStyle('background', event.target.value)} /></label></fieldset> : null}{selected?.id.includes('cards') ? <div aria-label="卡片资源">{cardStyles.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}{selected?.id.includes('background') ? <div aria-label="背景资源">{backgrounds.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}{selected?.id.includes('graphics') ? <div aria-label="图形资源">{graphics.map((resource) => <button key={resource.id} onClick={() => project && updateProject(setClipStyle(project, selected.id, resource.id))}>{resource.name}</button>)}</div> : null}</aside><Timeline project={project} onChange={updateProject} onSelect={setSelectedClipId} /><SubtitleStylePanel project={project} onChange={updateProject} /><AiAnalysisPanel project={project} onApplied={updateProject} /><ExportDialog project={project} /><LibraryPanel /></main>;
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
