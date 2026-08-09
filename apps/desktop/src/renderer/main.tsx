import { StrictMode, useRef, useState } from 'react';
import { useEffect } from 'react';
import { createRoot } from 'react-dom/client';
import { markClipUserEdited, setClipLayout, setClipStyle, type Project, type Resource } from '@ai-video/domain';
import { HomePage } from './HomePage.js';
import { Workbench } from './Workbench.js';
import { SettingsDialog } from './SettingsDialog.js';
import { createProjectHistory, type ProjectHistory } from './projectHistory.js';
import { startProjectAutoSave } from './projectAutoSave.js';
import type { ModelRecord } from '../main/modelRepository.js';
import { type AppView, openProject as navOpenProject, closeProject as navCloseProject } from './appNavigation.js';
import './theme.css';

function App(): React.JSX.Element {
  const [view, setView] = useState<AppView>('home'); const [project, setProject] = useState<Project>(); const [selectedClipId, setSelectedClipId] = useState<string>(); const [models, setModels] = useState<ModelRecord[]>([]); const [modelOnline, setModelOnline] = useState<boolean>(); const [refreshToken, setRefreshToken] = useState(0); const [historyRevision, setHistoryRevision] = useState(0);
  const [cardStyles, setCardStyles] = useState<Resource[]>([]); const [backgrounds, setBackgrounds] = useState<Resource[]>([]); const [graphics, setGraphics] = useState<Resource[]>([]);
  const saveQueue = useRef(Promise.resolve());
  const history = useRef<ProjectHistory>();
  const projectRef = useRef<Project>();
  const workspaceDataVersion = useRef(0);
  const openProject = (next: Project) => { const nextState = navOpenProject({ view, project }, next); setView(nextState.view); setProject(nextState.project!); history.current = createProjectHistory(next); window.dispatchEvent(new Event('project-opened')); window.dispatchEvent(new CustomEvent('project-name', { detail: next.media.path.split(/[\\/]/).at(-1) ?? next.id })); window.dispatchEvent(new CustomEvent('project-save-status', { detail: '已保存' })); setHistoryRevision((value) => value + 1); void window.aiVideo.window.enterWorkbench(); };
  const created = (next: Project) => { openProject(next); setRefreshToken((value) => value + 1); };
  const saveProject = (next: Project) => { window.dispatchEvent(new CustomEvent('project-save-status', { detail: '保存中' })); saveQueue.current = saveQueue.current.catch(() => undefined).then(() => window.aiVideo.projects.save(next)).then(() => window.dispatchEvent(new CustomEvent('project-save-status', { detail: '已保存' }))).catch(() => window.dispatchEvent(new CustomEvent('project-save-status', { detail: '保存失败' }))); };
  useEffect(() => { projectRef.current = project; }, [project]);
  useEffect(() => startProjectAutoSave(() => projectRef.current, saveProject), []);
  useEffect(() => { const closeProject = () => { if (!project || view !== 'workbench') return; saveProject(project); void saveQueue.current.finally(() => { const nextState = navCloseProject({ view, project }); history.current = undefined; setProject(nextState.project); setView(nextState.view); setSelectedClipId(undefined); window.dispatchEvent(new Event('project-closed')); setHistoryRevision((value) => value + 1); void window.aiVideo.window.exitWorkbench(); }); }; window.addEventListener('project-close-request', closeProject); return () => window.removeEventListener('project-close-request', closeProject); }, [project, view]);
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
  const replaceSelectedResource = (resourceId: string) => project && selected && updateProject(setClipStyle(project, selected.id, resourceId));
  if (view === 'home') {
    return (
      <>
        <HomePage onOpenProject={openProject} />
        <SettingsDialog onWorkspaceChanged={workspaceChanged} onModelsChanged={setModels} showTrigger={false} />
      </>
    );
  }
  return (
    <Workbench
      project={project}
      updateProject={updateProject}
      selectedClipId={selectedClipId}
      setSelectedClipId={setSelectedClipId}
      historyRevision={historyRevision}
      modelStatus={modelStatus}
      modelColor={modelColor}
      selected={selected}
      selectedText={selectedText}
      selectedTextStyle={selectedTextStyle}
      updateSelectedTextStyle={updateSelectedTextStyle}
      updateSelectedScale={updateSelectedScale}
      cardStyles={cardStyles}
      backgrounds={backgrounds}
      graphics={graphics}
      replaceSelectedResource={replaceSelectedResource}
      workspaceChanged={workspaceChanged}
      setModels={setModels}
    />
  );
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
