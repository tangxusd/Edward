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
  const resolutionLabel = resolution === 'custom' ? `自定义 ${customWidth}×${customHeight}` : resolution;
  const aspectHint = aspect === '16:9' ? '横屏' : '竖屏';
  const [w, h] = resolution === 'custom' ? [customWidth, customHeight] : resolution.split('x').map(Number);
  const showWidth = aspect === '16:9' ? w : h;
  const showHeight = aspect === '16:9' ? h : w;
  const formatLabel = transparent ? 'MOV（ProRes 4444）' : 'MP4（HEVC）';

  return (
    <>
      <button ref={triggerRef} type="button" className="export-trigger" onClick={() => setOpen(true)}>导出</button>
      {open ? (
        <div className="export-overlay" onClick={close}>
          <div
            ref={dialogRef}
            role="dialog"
            aria-label="导出"
            aria-modal="true"
            className="export-panel"
            onKeyDown={handleKeyDown}
            onClick={(e) => e.stopPropagation()}
          >
            <header className="export-header">
              <h2>导出</h2>
              <button type="button" className="export-close" aria-label="关闭导出" onClick={close}>×</button>
            </header>

            <div className="export-row">
              <label>文件名</label>
              <div>
                <input className="export-input" value={project?.id ?? '未命名项目'} readOnly />
                <div className="export-hint">默认使用当前项目名称</div>
              </div>
            </div>

            <div className="export-row">
              <label>分辨率</label>
              <div>
                <select className="export-select" value={resolution} onChange={(e) => setResolution(e.target.value)}>
                  <option value="1280x720">720p</option>
                  <option value="1920x1080">1080p</option>
                  <option value="2560x1440">1440p</option>
                  <option value="3840x2160">4K</option>
                  <option value="custom">自定义</option>
                </select>
                <span className="export-hint">
                  {resolution === 'custom' ? (
                    <>自定义分辨率：<input className="export-inline-input" type="number" min="1" value={customWidth} onChange={(e) => setCustomWidth(Number(e.target.value))} aria-label="自定义宽度" /> × <input className="export-inline-input" type="number" min="1" value={customHeight} onChange={(e) => setCustomHeight(Number(e.target.value))} aria-label="自定义高度" /></>
                  ) : (
                    <>根据项目画幅自动识别：{aspectHint} {showWidth} × {showHeight}</>
                  )}
                </span>
              </div>
            </div>

            <div className="export-row">
              <label>画幅</label>
              <select className="export-select" value={aspect} onChange={(e) => setAspect(e.target.value)}>
                <option value="16:9">16:9</option>
                <option value="9:16">9:16</option>
              </select>
            </div>

            <div className="export-row">
              <label>质量</label>
              <div>
                <select className="export-select" disabled={transparent} value={quality} onChange={(e) => setQuality(e.target.value as keyof typeof qualityCrf)}>
                  <option value="high">高质量</option>
                  <option value="balanced">均衡</option>
                  <option value="compact">紧凑</option>
                </select>
              </div>
            </div>

            <div className="export-row">
              <label>格式</label>
              <div>
                <select className="export-select" value={transparent ? 'mov' : 'mp4'} onChange={(e) => setTransparent(e.target.value === 'mov')}>
                  <option value="mp4">MP4（HEVC）</option>
                  <option value="mov">MOV（ProRes 4444）</option>
                </select>
                <div className="export-hint">勾选透明叠加层时自动切换为 ProRes 4444 MOV</div>
              </div>
            </div>

            <div className="export-row">
              <label>导出位置</label>
              <div>
                <button className="export-choose" onClick={() => void chooseDirectory()}>选择文件夹</button>
                <div className="export-path">{directory || '尚未选择导出位置'}</div>
              </div>
            </div>

            <div className="export-row">
              <label>导出进度</label>
              <div>
                <div className="export-bar">
                  <div className="export-bar-fill" style={{ width: `${progressValue ?? 0}%` }} />
                </div>
                <div className="export-path">{status || '等待导出'}</div>
              </div>
            </div>

            {error ? <p className="export-error" role="alert">{error}</p> : null}

            <footer className="export-footer">
              <span className="export-hint">切换为 9:16 项目时自动显示竖屏预设</span>
              <div>
                <button className="export-cancel" onClick={() => void cancel()} disabled={!running}>取消</button>
                <button className="export-go" onClick={() => void start()} disabled={!project || !directory || running}>导出</button>
              </div>
            </footer>
          </div>
        </div>
      ) : null}
    </>
  );
}