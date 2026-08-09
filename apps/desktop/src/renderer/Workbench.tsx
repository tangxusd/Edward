import { markClipUserEdited, type Project, type Resource } from '@ai-video/domain';
import { LibraryPanel } from './LibraryPanel.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { ResourceChoicePanel } from './ResourceChoicePanel.js';
import { ComponentAiChatPanel } from './ComponentAiChatPanel.js';
import { Timeline } from './Timeline.js';
import { AiAnalysisPanel } from './AiAnalysisPanel.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import { SettingsDialog } from './SettingsDialog.js';
import { ExportDialog } from './ExportDialog.js';
import type { ModelRecord } from '../main/modelRepository.js';

type Props = {
  project?: Project;
  updateProject: (next: Project) => void;
  selectedClipId?: string;
  setSelectedClipId: (id?: string) => void;
  historyRevision: number;
  modelStatus: string;
  modelColor: string;
  selected: Project['tracks'][keyof Project['tracks']]['clips'][0] | undefined;
  selectedText: string;
  selectedTextStyle: Record<string, string | number | undefined>;
  updateSelectedTextStyle: (field: 'fontFamily' | 'fontSize' | 'color' | 'background', value: string | number) => void;
  updateSelectedScale: (value: number) => void;
  cardStyles: Resource[];
  backgrounds: Resource[];
  graphics: Resource[];
  replaceSelectedResource: (resourceId: string) => void;
  workspaceChanged: () => void;
  setModels: (models: ModelRecord[]) => void;
};

export function Workbench({
  project,
  updateProject,
  selectedClipId,
  setSelectedClipId,
  historyRevision,
  modelStatus,
  modelColor,
  selected,
  selectedText,
  selectedTextStyle,
  updateSelectedTextStyle,
  updateSelectedScale,
  cardStyles,
  backgrounds,
  graphics,
  replaceSelectedResource,
  workspaceChanged,
  setModels,
}: Props): React.JSX.Element {
  return (
    <div className="workbench-container">
      <header className="workbench-topbar" data-history-revision={historyRevision}>
        <div className="workbench-topbar-left">
          <h1>Edward</h1>
          <span className="header-version">0.1.1</span>
        </div>
        <div className="workbench-topbar-center" />
        <div className="workbench-topbar-right">
          <output aria-label="当前模型状态" style={{ color: modelColor }}>{modelStatus}</output>
        </div>
      </header>
      <div className="workbench-grid">
        <aside className="left-panel">
          <LibraryPanel />
        </aside>
        <div className="divider-left" />
        <main className="center-canvas">
          <PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} selectedClipId={selectedClipId} />
        </main>
        <div className="divider-right" />
        <aside className="right-panel">
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
        <div className="divider-horizontal" />
        <section className="timeline-panel">
          <Timeline project={project} onChange={updateProject} onSelect={setSelectedClipId} />
        </section>
      </div>
      <SettingsDialog onWorkspaceChanged={workspaceChanged} onModelsChanged={setModels} />
      <ExportDialog project={project} />
    </div>
  );
}