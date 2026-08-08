import { useEffect, useLayoutEffect, useRef, useState, type KeyboardEvent } from 'react';
import type { ModelRecord } from '../main/modelRepository.js';
import { ModelSettings } from './ModelSettings.js';
import { WorkspaceSettings } from './WorkspaceSettings.js';

type Props = {
  onWorkspaceChanged?: () => void;
  onModelsChanged?: (models: ModelRecord[]) => void;
};

export function SettingsDialog({ onWorkspaceChanged, onModelsChanged }: Props): React.JSX.Element {
  const [open, setOpen] = useState(false);
  const [projectOpen, setProjectOpen] = useState(false);
  const [projectName, setProjectName] = useState('');
  const [saveStatus, setSaveStatus] = useState('');
  const [tab, setTab] = useState<'general' | 'models'>('general');
  const dialogRef = useRef<HTMLDivElement>(null);
  const triggerRef = useRef<HTMLButtonElement>(null);

  useLayoutEffect(() => {
    if (!open) return;
    dialogRef.current?.querySelector<HTMLElement>('[role="tab"][aria-selected="true"]')?.focus();
  }, [open, tab]);
  useEffect(() => {
    const opened = () => setProjectOpen(true);
    const closed = () => { setProjectOpen(false); setProjectName(''); setSaveStatus(''); };
    const named = (event: Event) => setProjectName(String((event as CustomEvent<string>).detail));
    const saved = (event: Event) => setSaveStatus(String((event as CustomEvent<string>).detail));
    window.addEventListener('project-opened', opened);
    window.addEventListener('project-closed', closed);
    window.addEventListener('project-name', named);
    window.addEventListener('project-save-status', saved);
    return () => { window.removeEventListener('project-opened', opened); window.removeEventListener('project-closed', closed); window.removeEventListener('project-name', named); window.removeEventListener('project-save-status', saved); };
  }, []);

  const close = () => {
    setOpen(false);
    requestAnimationFrame(() => triggerRef.current?.focus());
  };
  const activateTab = (next: 'general' | 'models') => {
    setTab(next);
  };
  const moveTab = (event: KeyboardEvent<HTMLButtonElement>) => {
    if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    if (event.key === 'Home') activateTab('general');
    else if (event.key === 'End') activateTab('models');
    else activateTab(tab === 'general' ? 'models' : 'general');
  };
  const trapFocus = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key !== 'Tab') return;
    const focusable = Array.from(dialogRef.current?.querySelectorAll<HTMLElement>('button:not([disabled]):not([tabindex="-1"]), input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [href], [tabindex]:not([tabindex="-1"])') ?? []);
    if (focusable.length === 0) return;
    const currentIndex = focusable.indexOf(document.activeElement as HTMLElement);
    const nextIndex = event.shiftKey
      ? (currentIndex <= 0 ? focusable.length - 1 : currentIndex - 1)
      : (currentIndex < 0 || currentIndex === focusable.length - 1 ? 0 : currentIndex + 1);
    event.preventDefault();
    focusable[nextIndex]?.focus();
  };

  return <>{projectOpen ? <output aria-label="项目名称" className="project-name">{projectName}</output> : null}{projectOpen ? <output aria-label="保存状态" className="save-status">{saveStatus}</output> : null}<button ref={triggerRef} type="button" className="settings-trigger" onClick={() => setOpen(true)}>设置</button>{projectOpen ? <button type="button" className="project-close-trigger" onClick={() => window.dispatchEvent(new Event('project-close-request'))}>关闭项目</button> : null}{open ? <div ref={dialogRef} role="dialog" aria-label="设置" aria-modal="true" onKeyDown={trapFocus}><section><header><h2>设置</h2><button type="button" aria-label="关闭设置" onClick={close}>关闭</button></header><div role="tablist" aria-label="设置分类"><button type="button" id="settings-tab-general" role="tab" tabIndex={tab === 'general' ? 0 : -1} aria-controls="settings-panel-general" aria-selected={tab === 'general'} onClick={() => activateTab('general')} onKeyDown={moveTab}>通用</button><button type="button" id="settings-tab-models" role="tab" tabIndex={tab === 'models' ? 0 : -1} aria-controls="settings-panel-models" aria-selected={tab === 'models'} onClick={() => activateTab('models')} onKeyDown={moveTab}>AI 模型</button></div><div id={`settings-panel-${tab}`} role="tabpanel" aria-labelledby={`settings-tab-${tab}`}>{tab === 'general' ? <WorkspaceSettings onChanged={onWorkspaceChanged} /> : <ModelSettings onModelsChanged={onModelsChanged} />}</div></section></div> : null}</>;
}
