import { useCallback } from 'react';
import type { Project, Resource } from '@ai-video/domain';
import { TopBar } from './TopBar.js';
import { LeftPanel } from './LeftPanel.js';
import { PreviewCanvas } from './PreviewCanvas.js';
import { RightPanel } from './RightPanel.js';
import { Timeline } from './Timeline.js';
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
  const handleExport = useCallback(() => {
    document.querySelector<HTMLButtonElement>('.export-trigger')?.click();
  }, []);

  return (
    <div className="workbench-container">
      <TopBar modelStatus={modelStatus} modelColor={modelColor} onExport={handleExport} onSettings={() => window.dispatchEvent(new Event('settings-request'))} />
      <div className="workbench-grid">
        <LeftPanel />
        <div className="divider-left" />
        <main className="center-canvas">
          <PreviewCanvas project={project} onChange={updateProject} onSelect={setSelectedClipId} selectedClipId={selectedClipId} />
        </main>
        <div className="divider-right" />
        <RightPanel
          project={project}
          updateProject={updateProject}
          selectedClipId={selectedClipId}
          selected={selected}
          selectedText={selectedText}
          selectedTextStyle={selectedTextStyle}
          updateSelectedTextStyle={updateSelectedTextStyle}
          updateSelectedScale={updateSelectedScale}
          cardStyles={cardStyles}
          backgrounds={backgrounds}
          graphics={graphics}
          replaceSelectedResource={replaceSelectedResource}
          setModels={setModels}
        />
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