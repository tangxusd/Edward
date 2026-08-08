import { useState, type KeyboardEvent } from 'react';
import type { ModelRecord } from '../main/modelRepository.js';
import { ModelSettings } from './ModelSettings.js';
import { WorkspaceSettings } from './WorkspaceSettings.js';

type Props = {
  onWorkspaceChanged?: () => void;
  onModelsChanged?: (models: ModelRecord[]) => void;
};

export function SettingsDialog({ onWorkspaceChanged, onModelsChanged }: Props): React.JSX.Element {
  const [open, setOpen] = useState(false);
  const [tab, setTab] = useState<'general' | 'models'>('general');
  const activateTab = (next: 'general' | 'models') => {
    setTab(next);
    requestAnimationFrame(() => document.getElementById(`settings-tab-${next}`)?.focus());
  };
  const moveTab = (event: KeyboardEvent<HTMLButtonElement>) => {
    if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
    event.preventDefault();
    activateTab(tab === 'general' ? 'models' : 'general');
  };

  return <><button type="button" className="settings-trigger" onClick={() => setOpen(true)}>设置</button>{open ? <div role="dialog" aria-label="设置" aria-modal="true"><section><header><h2>设置</h2><button type="button" aria-label="关闭设置" onClick={() => setOpen(false)}>关闭</button></header><div role="tablist" aria-label="设置分类"><button type="button" id="settings-tab-general" role="tab" aria-controls="settings-panel-general" aria-selected={tab === 'general'} onClick={() => activateTab('general')} onKeyDown={moveTab}>通用</button><button type="button" id="settings-tab-models" role="tab" aria-controls="settings-panel-models" aria-selected={tab === 'models'} onClick={() => activateTab('models')} onKeyDown={moveTab}>AI 模型</button></div><div id={`settings-panel-${tab}`} role="tabpanel" aria-labelledby={`settings-tab-${tab}`}>{tab === 'general' ? <WorkspaceSettings onChanged={onWorkspaceChanged} /> : <ModelSettings onModelsChanged={onModelsChanged} />}</div></section></div> : null}</>;
}
