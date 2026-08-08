import { useState } from 'react';
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

  return <><button type="button" className="settings-trigger" onClick={() => setOpen(true)}>设置</button>{open ? <div role="dialog" aria-label="设置" aria-modal="true"><section><header><h2>设置</h2><button type="button" aria-label="关闭设置" onClick={() => setOpen(false)}>关闭</button></header><div role="tablist" aria-label="设置分类"><button type="button" role="tab" aria-selected={tab === 'general'} onClick={() => setTab('general')}>通用</button><button type="button" role="tab" aria-selected={tab === 'models'} onClick={() => setTab('models')}>AI 模型</button></div>{tab === 'general' ? <WorkspaceSettings onChanged={onWorkspaceChanged} /> : <ModelSettings onModelsChanged={onModelsChanged} />}</section></div> : null}</>;
}
