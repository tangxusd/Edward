import { useState } from 'react';

export function WorkspaceSettings(): React.JSX.Element {
  const [root, setRoot] = useState(''); const [saved, setSaved] = useState('');
  const chooseDirectory = async () => { const selected = await window.aiVideo.workspace.chooseDirectory(); if (selected) setRoot(selected); };
  return <section aria-label="工作目录"><h2>项目工作目录</h2><input aria-label="工作目录路径" value={root} onChange={(e) => setRoot(e.target.value)} /><button type="button" onClick={() => void chooseDirectory()}>选择目录</button><button disabled={!root} onClick={() => void window.aiVideo.workspace.setRoot(root).then(setSaved)}>应用</button>{saved ? <p>已设置：{saved}</p> : null}</section>;
}
