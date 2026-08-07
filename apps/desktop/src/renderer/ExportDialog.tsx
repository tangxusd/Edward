import { useEffect, useState } from 'react';
import type { Project } from '@ai-video/domain';
import type { ExportProgress } from '@ai-video/media';

export function ExportDialog({ project }: { project?: Project }): React.JSX.Element {
  const [aspect, setAspect] = useState('16:9');
  const [resolution, setResolution] = useState('1920x1080');
  const [transparent, setTransparent] = useState(false);
  const [directory, setDirectory] = useState('');
  const [jobId, setJobId] = useState<string>();
  const [progress, setProgress] = useState<ExportProgress>();

  useEffect(() => window.aiVideo.export.onProgress((event) => {
    if (event.jobId === jobId) setProgress(event.progress);
  }), [jobId]);

  const start = async () => {
    if (!project || !directory || jobId) return;
    const [selectedWidth, selectedHeight] = resolution.split('x').map(Number);
    const portrait = aspect === '9:16';
    const width = portrait ? selectedHeight : selectedWidth;
    const height = portrait ? selectedWidth : selectedHeight;
    const separator = directory.includes('\\') ? '\\' : '/';
    const output = `${directory.replace(/[\\/]+$/, '')}${separator}${project.id}.${transparent ? 'mov' : 'mp4'}`;
    setProgress({});
    const nextJobId = await window.aiVideo.export.start({ input: project.media.path, output, width, height, transparent });
    setJobId(nextJobId);
  };

  useEffect(() => {
    if (!jobId || !progress?.status) return;
    setJobId(undefined);
  }, [jobId, progress?.status]);

  const cancel = async () => {
    if (!jobId) return;
    await window.aiVideo.export.cancel(jobId);
    setJobId(undefined);
    setProgress({ status: 'failed', error: '已取消导出' });
  };

  const running = Boolean(jobId);
  const status = progress?.status === 'completed' ? '导出完成' : progress?.status === 'failed' ? `导出失败：${progress.error ?? '未知错误'}` : running ? '正在导出' : '';
  return <section aria-label="导出"><h2>手动导出</h2><label>画幅<select value={aspect} onChange={(e) => setAspect(e.target.value)}><option>16:9</option><option>9:16</option></select></label><label>分辨率<select value={resolution} onChange={(e) => setResolution(e.target.value)}><option>1280x720</option><option>1920x1080</option><option>2560x1440</option><option>3840x2160</option></select></label><label><input type="checkbox" checked={transparent} onChange={(e) => setTransparent(e.target.checked)} />透明背景</label><label>输出目录<input value={directory} onChange={(e) => setDirectory(e.target.value)} /></label><button disabled={!project || !directory || running} onClick={() => void start()}>开始导出</button>{running ? <button onClick={() => void cancel()}>取消导出</button> : null}{status ? <p role="status">{status}</p> : null}<progress max="1" value={progress?.status === 'completed' ? 1 : undefined} aria-label="导出进度" /></section>;
}
