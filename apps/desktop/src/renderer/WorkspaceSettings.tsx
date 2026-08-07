import { useEffect, useState } from 'react';

export function WorkspaceSettings({ onChanged }: { onChanged?: () => void }): React.JSX.Element {
  const [root, setRoot] = useState(''); const [saved, setSaved] = useState(''); const [error, setError] = useState('');
  useEffect(() => { void window.aiVideo.workspace.getRoot().then(setRoot); }, []);
  const chooseDirectory = async () => { const selected = await window.aiVideo.workspace.chooseDirectory(); if (selected) { setRoot(selected); setError(''); } };
  const apply = async () => { setError(''); try { const nextRoot = await window.aiVideo.workspace.setRoot(root); setSaved(nextRoot); window.dispatchEvent(new Event('workspace-changed')); onChanged?.(); } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); } };
  return <section aria-label="工作目录"><h2>项目工作目录</h2><input aria-label="工作目录路径" value={root} onChange={(e) => { setRoot(e.target.value); setError(''); }} /><button type="button" onClick={() => void chooseDirectory()}>选择目录</button><button disabled={!root} onClick={() => void apply()}>应用</button>{saved ? <p>已设置：{saved}</p> : null}{error ? <p role="alert">{error}</p> : null}</section>;
}
