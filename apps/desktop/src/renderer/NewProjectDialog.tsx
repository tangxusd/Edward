import { useState } from 'react';
import { createProject } from '@ai-video/domain';

export function NewProjectDialog({ onCreated }: { onCreated: (project: ReturnType<typeof createProject>) => void }): React.JSX.Element {
  const [scriptPath, setScriptPath] = useState('');
  const [mediaPath, setMediaPath] = useState('');
  const [aspectRatio, setAspectRatio] = useState<'16:9' | '9:16'>('16:9');
  const [error, setError] = useState('');
  const create = async (): Promise<void> => {
    if (!scriptPath || !mediaPath) { setError('请同时添加文案文稿和音频或视频'); return; }
    const mediaKind = /\.(mp4|mov|mkv|webm)$/i.test(mediaPath) ? 'video' : 'audio';
    const project = await window.aiVideo.projects.create(createProject({ scriptPath, mediaPath, mediaKind, aspectRatio }));
    onCreated(project);
    setError('');
  };
  return <section aria-label="新建项目"><h2>新建项目</h2><label>文案文稿<input value={scriptPath} onChange={(event) => setScriptPath(event.target.value)} /></label><label>音频或视频<input value={mediaPath} onChange={(event) => setMediaPath(event.target.value)} /></label><label>项目画幅<select aria-label="项目画幅" value={aspectRatio} onChange={(event) => setAspectRatio(event.target.value as '16:9' | '9:16')}><option>16:9</option><option>9:16</option></select></label><button onClick={() => void create()}>创建项目</button>{error ? <p role="alert">{error}</p> : null}</section>;
}
