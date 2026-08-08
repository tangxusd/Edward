import { StrictMode } from 'react';
import { useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { markClipUserEdited, setClipLayout, setClipStyle, type Project, type Resource } from '@ai-video/domain';
import { LibraryPanel } from './LibraryPanel.js';
import { NewProjectDialog } from './NewProjectDialog.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import { ExportDialog } from './ExportDialog.js';
import { SettingsDialog } from './SettingsDialog.js';
import { ProjectList } from './ProjectList.js';
import { ResourceChoicePanel } from './ResourceChoicePanel.js';
import { ComponentAiChatPanel } from './ComponentAiChatPanel.js';
import { Timeline } from './Timeline.js';
import { AiAnalysisPanel } from './AiAnalysisPanel.js';
import { createProjectHistory, type ProjectHistory } from './projectHistory.js';
import { startProjectAutoSave } from './projectAutoSave.js';
import type { ModelRecord } from '../main/modelRepository.js';
import './theme.css';

function App(): React.JSX.Element {
  const [project, setProject] = useState<Project>(); const [selectedClipId, setSelectedClipId] = useState<string>(); const [models, setModels] = useState<ModelRecord[]>([]); const [modelOnline, setModelOnline] = useState<boolean>(); const [refreshToken, setRefreshToken] = useState(0); const [historyRevision, setHistoryRevision] = useState(0);
  const [cardStyles, setCardStyles] = useState<Resource[]>([]); const [backgrounds, setBackgrounds] = useState<Resource[]>([]); const [graphics, setGraphics] = useState<Resource[]>([]);
  const [leftPanelWidth, setLeftPanelWidth] = useState(440); const [rightPanelWidth, setRightPanelWidth] = useState(440);
  const saveQueue = useRef(Promise.resolve());
  const history = useRef<ProjectHistory>();
  const projectRef = useRef<Project>();
  const workspaceDataVersion = useRef(0);
  const openProject = (next: Project) => { history.current = createProjectHistory(next); setProject(next); window.dispatchEvent(new Event('project-opened')); window.dispatchEvent(new CustomEvent('project-name', { detail: next.media.path.split(/[\\/]/).at(-1) ?? next.id })); window.dispatchEvent(new CustomEvent('project-save-status', { detail: '已保存' })); setHistoryRevision((value) => value + 1); };
  const created = (next: Project) => { openProject(next); setRefreshToken((value) => value + 1); };
  const saveProject = (next: Project) => { window.dispatchEvent(new CustomEvent('project-save-status', { detail: '保存中' })); saveQueue.current = saveQueue.current.catch(() => undefined).then(() => window.aiVideo.projects.save(next)).then(() => window.dispatchEvent(new CustomEvent('project-save-status', { detail: '已保存' }))).catch(() => window.dispatchEvent(new CustomEvent('project-save-status', { detail: '保存失败' }))); };
  useEffect(() => { projectRef.current = project; }, [project]);
  useEffect(() => startProjectAutoSave(() => projectRef.current, saveProject), []);
  useEffect(() => { const closeProject = () => { if (!project) return; saveProject(project); void saveQueue.current.finally(() => { history.current = undefined; setProject(undefined); setSelectedClipId(undefined); window.dispatchEvent(new Event('project-closed')); setHistoryRevision((value) => value + 1); }); }; window.addEventListener('project-close-request', closeProject); return () => window.removeEventListener('project-close-request', closeProject); }, [project]);
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
  const updateSelectedScale = (value: number) => {
    if (!project || !selected?.layout || !Number.isFinite(value)) return;
    const scale = Math.max(1, Math.min(1_000, value));
    const baseWidth = selected.layout.baseWidth ?? selected.layout.width;
    const baseHeight = selected.layout.baseHeight ?? selected.layout.height;
    updateProject(markClipUserEdited(setClipLayout(project, selected.id, { ...selected.layout, baseWidth, baseHeight, scale, width: baseWidth * scale / 100, height: baseHeight * scale / 100 }), selected.id));
  };
  const workspaceChanged = () => { history.current = undefined; setProject(undefined); setSelectedClipId(undefined); window.dispatchEvent(new Event('project-closed')); setHistoryRevision((value) => value + 1); setModels([]); setModelOnline(undefined); setCardStyles([]); setBackgrounds([]); setGraphics([]); setRefreshToken((value) => value + 1); reloadWorkspaceData(); };
  useEffect(() => { document.documentElement.style.setProperty('--left-panel-width', `${leftPanelWidth}px`); document.documentElement.style.setProperty('--right-panel-width', `${rightPanelWidth}px`); }, [leftPanelWidth, rightPanelWidth]);
  useEffect(() => { const down = (event: PointerEvent) => { const side = Math.abs(event.clientX - leftPanelWidth) < 8 ? 'left' : Math.abs(event.clientX - (window.innerWidth - rightPanelWidth)) < 8 ? 'right' : undefined; if (!side) return; const startX = event.clientX; const startWidth = side === 'left' ? leftPanelWidth : rightPanelWidth; const other = side === 'left' ? rightPanelWidth : leftPanelWidth; const move = (next: PointerEvent) => { const delta = side === 'left' ? next.clientX - startX : startX - next.clientX; const maximum = Math.min(640, window.innerWidth - other - 520); const width = Math.max(280, Math.min(maximum, startWidth + delta)); if (side === 'left') setLeftPanelWidth(width); else setRightPanelWidth(width); }; const stop = () => { window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', stop); }; window.addEventListener('pointermove', move); window.addEventListener('pointerup', stop, { once: true }); event.preventDefault(); }; window.addEventListener('pointerdown', down); return () => window.removeEventListener('pointerdown', down); }, [leftPanelWidth, rightPanelWidth]);
  const replaceSelectedResource = (resourceId: string) => project && selected && updateProject(setClipStyle(project, selected.id, resourceId));
  return <main>
    <header data-history-revision={historyRevision}>
      <h1>AI 剪视频工具</h1>
      <button aria-label="撤销" title="撤销" type="button" disabled={!history.current?.canUndo} onClick={undo}>撤销</button>
      <button aria-label="重做" title="重做" type="button" disabled={!history.current?.canRedo} onClick={redo}>重做</button>
      <output aria-label="当前模型状态" style={{ color: modelColor }}>{modelStatus}</output>
    </header>
    <SettingsDialog onWorkspaceChanged={workspaceChanged} onModelsChanged={setModels} />
    {project ? <LibraryPanel /> : <><ProjectList onOpen={openProject} refreshToken={refreshToken} /><NewProjectDialog onCreated={created} /></>}
    <PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} selectedClipId={selectedClipId} />
    <aside aria-label="资源检查器">
      {selectedClipId ?? '未选择资源'}
      {selected && selectedText ? <label>文字内容<textarea aria-label="文字内容" value={selectedText} onChange={(event) => project && updateProject(markClipUserEdited(project, selected.id, { ...(selected.content as object), text: event.target.value }))} /></label> : null}
      {selected?.layout ? <fieldset aria-label="缩放属性"><label>缩放<input aria-label="缩放" type="number" min="1" max="1000" value={selected.layout.scale ?? 100} onChange={(event) => updateSelectedScale(Number(event.target.value))} />%</label></fieldset> : null}
      {selected?.id.includes('subtitles') ? <fieldset aria-label="局部字幕样式"><label>局部字体<input aria-label="局部字体" value={selectedTextStyle.fontFamily ?? project?.subtitleStyle.fontFamily ?? ''} onChange={(event) => updateSelectedTextStyle('fontFamily', event.target.value)} /></label><label>局部字号<input aria-label="局部字号" type="number" value={selectedTextStyle.fontSize ?? project?.subtitleStyle.fontSize ?? 48} onChange={(event) => updateSelectedTextStyle('fontSize', Number(event.target.value))} /></label><label>局部颜色<input aria-label="局部颜色" type="color" value={selectedTextStyle.color ?? project?.subtitleStyle.color ?? '#ffffff'} onChange={(event) => updateSelectedTextStyle('color', event.target.value)} /></label><label>局部文字背景<input aria-label="局部文字背景" value={selectedTextStyle.background ?? project?.subtitleStyle.background ?? ''} onChange={(event) => updateSelectedTextStyle('background', event.target.value)} /></label></fieldset> : null}
      {selected?.id.includes('cards') ? <ResourceChoicePanel label="卡片" resources={cardStyles} onSelect={replaceSelectedResource} /> : null}
      {selected?.id.includes('background') ? <ResourceChoicePanel label="背景" resources={backgrounds} onSelect={replaceSelectedResource} /> : null}
      {selected?.id.includes('graphics') ? <ResourceChoicePanel label="图形" resources={graphics} onSelect={replaceSelectedResource} /> : null}
      {selected && (selected.id.includes('cards') || selected.id.includes('graphics') || selected.id.includes('subtitles')) ? <ComponentAiChatPanel project={project!} clip={selected} onProjectChange={updateProject} onApply={(content) => updateProject(markClipUserEdited(project!, selected.id, content))} /> : null}
      <AiAnalysisPanel project={project} onApplied={updateProject} />
      <SubtitleStylePanel project={project} onChange={updateProject} />
    </aside>
    <Timeline project={project} onChange={updateProject} onSelect={setSelectedClipId} />
    <ExportDialog project={project} />
  </main>;
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
