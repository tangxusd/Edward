import { useEffect, useLayoutEffect, useRef, useState, type KeyboardEvent } from 'react';
import type { Project } from '@ai-video/domain';
import type { ExportProgress } from '@ai-video/media';

const qualityCrf = { high: 18, balanced: 22, compact: 28 } as const;

export function ExportDialog({ project }: { project?: Project }): React.JSX.Element {
  const [aspect, setAspect] = useState('16:9');
  const [resolution, setResolution] = useState('1920x1080');
  const [customWidth, setCustomWidth] = useState(1920);
  const [customHeight, setCustomHeight] = useState(1080);
  const [quality, setQuality] = useState<keyof typeof qualityCrf>('balanced');
  const [transparent, setTransparent] = useState(false);
  const [directory, setDirectory] = useState('');
  const [jobId, setJobId] = useState<string>();
  const [progress, setProgress] = useState<ExportProgress>();
  const [error, setError] = useState('');
  const [open, setOpen] = useState(false);
  const dialogRef = useRef<HTMLDivElement>(null);
  const triggerRef = useRef<HTMLButtonElement>(null);

  useEffect(() => setAspect(project?.aspectRatio ?? '16:9'), [project?.id, project?.aspectRatio]);

  useLayoutEffect(() => {
    if (!open) return;
    dialogRef.current?.querySelector<HTMLElement>('button:not([disabled])')?.focus();
  }, [open]);

  useEffect(() => window.aiVideo.export.onProgress((event) => {
    if (event.jobId === jobId) setProgress(event.progress);
  }), [jobId]);

  const start = async () => {
    if (!project || !directory || jobId) return;
    const dimensions = resolution === 'custom' ? [customWidth, customHeight] : resolution.split('x').map(Number);
    const [selectedWidth, selectedHeight] = dimensions;
    const width = resolution === 'custom' || aspect === '16:9' ? selectedWidth : selectedHeight;
    const height = resolution === 'custom' || aspect === '16:9' ? selectedHeight : selectedWidth;
    if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
      setError('自定义分辨率必须为正整数');
      return;
    }
    const separator = directory.includes('\\') ? '\\' : '/';
    const output = `${directory.replace(/[\\/]+$/, '')}${separator}${project.id}.${transparent ? 'mov' : 'mp4'}`;
    setError('');
    setProgress({});
    try {
      const subtitleOverlays = project.tracks.subtitles.clips.flatMap((clip) => {
        if (typeof clip.content !== 'object' || clip.content === null || !('text' in clip.content)) return [];
        const textStyle = 'textStyle' in clip.content && typeof clip.content.textStyle === 'object' && clip.content.textStyle !== null ? clip.content.textStyle as { color?: string; fontSize?: number } : {};
        return [{ text: String(clip.content.text), start: clip.start, duration: clip.duration, color: textStyle.color ?? project.subtitleStyle.color, fontSize: textStyle.fontSize ?? project.subtitleStyle.fontSize, background: textStyle.background ?? project.subtitleStyle.background }];
      });
      const cardOverlays = project.tracks.cards.clips.flatMap((clip) => typeof clip.content === 'object' && clip.content !== null && 'text' in clip.content ? [{ text: String(clip.content.text), start: clip.start, duration: clip.duration, color: project.subtitleStyle.color, fontSize: project.subtitleStyle.fontSize, background: project.subtitleStyle.background, x: clip.layout?.x, y: clip.layout?.y }] : []);
      const overlays = [...subtitleOverlays, ...cardOverlays];
      const nextJobId = await window.aiVideo.export.start({ input: project.media.path, output, width, height, transparent, mediaKind: project.media.kind, crf: qualityCrf[quality], overlays });
      setJobId(nextJobId);
    } catch (cause) {
      setProgress(undefined);
      setError(cause instanceof Error ? cause.message : String(cause));
    }
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

  const chooseDirectory = async () => {
    const selected = await window.aiVideo.workspace.chooseDirectory();
    if (selected) setDirectory(selected);
  };

  const close = () => {
    setOpen(false);
    requestAnimationFrame(() => triggerRef.current?.focus());
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
  const handleKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === 'Escape') {
      event.preventDefault();
      close();
      return;
    }
    trapFocus(event);
  };

  const running = Boolean(jobId);
  const status = progress?.status === 'completed' ? '导出完成' : progress?.status === 'failed' ? `导出失败：${progress.error ?? '未知错误'}` : running ? '正在导出' : '';
  const progressValue = progress?.status === 'completed' ? 100 : progress?.percent;
  return <><button ref={triggerRef} type="button" className="export-trigger" onClick={() => setOpen(true)}>导出</button>{open ? <div ref={dialogRef} role="dialog" aria-label="导出" aria-modal="true" onKeyDown={handleKeyDown}><section><header><h2>手动导出</h2><button type="button" aria-label="关闭导出" onClick={close}>关闭</button></header><label>画幅<select value={aspect} onChange={(e) => setAspect(e.target.value)}><option>16:9</option><option>9:16</option></select></label><label>分辨率<select value={resolution} onChange={(e) => setResolution(e.target.value)}><option>1280x720</option><option>1920x1080</option><option>2560x1440</option><option>3840x2160</option><option value="custom">自定义</option></select></label>{resolution === 'custom' ? <><label>自定义宽度<input aria-label="自定义宽度" type="number" min="1" value={customWidth} onChange={(event) => setCustomWidth(Number(event.target.value))} /></label><label>自定义高度<input aria-label="自定义高度" type="number" min="1" value={customHeight} onChange={(event) => setCustomHeight(Number(event.target.value))} /></label></> : null}<label>质量<select aria-label="质量" disabled={transparent} value={quality} onChange={(event) => setQuality(event.target.value as keyof typeof qualityCrf)}><option value="high">高质量</option><option value="balanced">均衡</option><option value="compact">紧凑</option></select></label><label><input type="checkbox" checked={transparent} onChange={(e) => setTransparent(e.target.checked)} />透明叠加层（ProRes 4444 MOV）</label><label>输出目录<input value={directory} onChange={(e) => setDirectory(e.target.value)} /></label><button type="button" disabled={running} onClick={() => void chooseDirectory()}>选择目录</button><button disabled={!project || !directory || running} onClick={() => void start()}>开始导出</button>{running ? <button onClick={() => void cancel()}>取消导出</button> : null}{status ? <p role="status">{status}</p> : null}{error ? <p role="alert">{error}</p> : null}<progress max="100" value={progressValue} aria-label="导出进度" />{progressValue === undefined ? null : <output aria-label="导出百分比">{progressValue}%</output>}</section></div> : null}</>;
}
