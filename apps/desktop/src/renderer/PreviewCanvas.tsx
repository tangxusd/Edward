import { useEffect, useRef, useState } from 'react';
import { markClipUserEdited, setClipLayout, type Project } from '@ai-video/domain';
import { distributeHorizontally, resizeWithAspectRatio, snapRectToGuides, type Rect } from './timelineMath.js';
import { toLocalFileUrl } from './fileUrl.js';

const ASPECT_RATIOS = ['16:9', '9:16', '1:1', '4:3', '3:2', '21:9'] as const;
const PREVIEW_QUALITIES = ['原画', '清晰', '流畅'] as const;

type ActivePointer =
  | { id: string; mode: 'drag'; offsetX: number; offsetY: number }
  | { id: string; mode: 'resize'; startX: number; startWidth: number }
  | { id: string; mode: 'project-drag'; offsetX: number; offsetY: number; rect: Rect }
  | { id: string; mode: 'project-resize'; startX: number; rect: Rect };

const canvasBounds: Rect = { x: 120, y: 180, width: 720, height: 180 };
const cardColors = ['#4f7cff', '#24b47e', '#d9922e'];

function inferCardLayoutCount(cards: Project['tracks']['cards']['clips']): 1 | 2 | 3 {
  for (const count of [3, 2] as const) {
    const expected = distributeHorizontally(canvasBounds, count, 24);
    if (cards.length >= count && cards.slice(0, count).every((clip, index) => {
      const layout = clip.layout;
      const target = expected[index];
      return layout?.x === target?.x && layout.y === target.y && layout.width === target.width && layout.height === target.height;
    })) return count;
  }
  return 1;
}

export function PreviewCanvas({ project, onChange, onSelect, selectedClipId }: { project?: Project; onChange?: (project: Project) => void; onSelect?: (clipId?: string) => void; selectedClipId?: string }): React.JSX.Element {
  const [cards, setCards] = useState<Record<string, Rect>>({ 'card-1': { x: 300, y: 180, width: 320, height: 180 } });
  const [cardCount, setCardCount] = useState<1 | 2 | 3>(1);
  const [guides, setGuides] = useState<{ x: number[]; y: number[] }>({ x: [], y: [] });
  const [playing, setPlaying] = useState(false);
  const [showGuides, setShowGuides] = useState(true);
  const [aspectOpen, setAspectOpen] = useState(false);
  const [qualityOpen, setQualityOpen] = useState(false);
  const [previewQuality, setPreviewQuality] = useState<string>('原画');
  const cardsRef = useRef(cards);
  const activeRef = useRef<ActivePointer | undefined>(undefined);
  const canvasRef = useRef<HTMLElement | null>(null);
  const mediaRef = useRef<HTMLMediaElement | null>(null);
  const projectRef = useRef(project);
  const onChangeRef = useRef(onChange);
  const projectCards = project?.tracks.cards.clips ?? [];

  useEffect(() => { projectRef.current = project; onChangeRef.current = onChange; }, [project, onChange]);
  useEffect(() => { setCardCount(inferCardLayoutCount(projectCards)); }, [project]);

  const updateCards = (update: (current: Record<string, Rect>) => Record<string, Rect>) => {
    setCards((current) => {
      const next = update(current);
      cardsRef.current = next;
      return next;
    });
  };

  const setLayout = (count: 1 | 2 | 3) => {
    setCardCount(count);
    const projectRects = distributeHorizontally(canvasBounds, count as 2 | 3, 24);
    if (project && projectCards.length >= count) {
      const nextProject = projectCards.slice(0, count).reduce(
        (current, clip, index) => markClipUserEdited(setClipLayout(current, clip.id, projectRects[index]!), clip.id),
        project,
      );
      onChange?.(nextProject);
    }
    if (count === 1) {
      updateCards((current) => ({ 'card-1': current['card-1'] ?? { x: 300, y: 180, width: 320, height: 180 } }));
      return;
    }
    updateCards(() => Object.fromEntries(projectRects.map((rect, index) => [`card-${index + 1}`, rect])));
  };

  const startDrag = (event: React.MouseEvent<HTMLDivElement>, id: string) => {
    const rect = cardsRef.current[id];
    const canvas = canvasRef.current?.getBoundingClientRect();
    if (!rect || !canvas) return;
    activeRef.current = { id, mode: 'drag', offsetX: event.clientX - canvas.left - rect.x, offsetY: event.clientY - canvas.top - rect.y };
  };

  const startResize = (event: React.MouseEvent<HTMLButtonElement>, id: string) => {
    event.stopPropagation();
    const rect = cardsRef.current[id];
    if (rect) activeRef.current = { id, mode: 'resize', startX: event.clientX, startWidth: rect.width };
  };

  const startProjectDrag = (event: React.MouseEvent<HTMLDivElement>, id: string, rect: Rect) => {
    const canvas = canvasRef.current?.getBoundingClientRect();
    if (!canvas) return;
    activeRef.current = { id, mode: 'project-drag', rect, offsetX: event.clientX - canvas.left - rect.x, offsetY: event.clientY - canvas.top - rect.y };
    const currentProject = project;
    if (currentProject) onChange?.(markClipUserEdited(setClipLayout(currentProject, id, rect), id));
  };

  const persistProjectLayout = (id: string, rect: Rect) => {
    if (project) onChange?.(markClipUserEdited(setClipLayout(project, id, rect), id));
  };

  const startProjectResize = (event: React.MouseEvent<HTMLButtonElement>, id: string, rect: Rect) => {
    event.stopPropagation();
    activeRef.current = { id, mode: 'project-resize', startX: event.clientX, rect };
  };

  const updateAt = (clientX: number, clientY: number) => {
    const active = activeRef.current;
    const canvas = canvasRef.current?.getBoundingClientRect();
    if (!active || !canvas) return;
    const currentProject = projectRef.current;
    if (active.mode === 'project-drag' && currentProject) {
      const nextX = Math.max(0, Math.min(canvas.width - active.rect.width, clientX - canvas.left - active.offsetX));
      const nextY = Math.max(0, Math.min(canvas.height - active.rect.height, clientY - canvas.top - active.offsetY));
      const peers = projectVisualClips.map(({ clip }, index) => ({ id: clip.id, rect: clip.layout ?? projectVisualRects[index] ?? active.rect })).filter((peer) => peer.id !== active.id).map((peer) => peer.rect);
      const snapped = snapRectToGuides({ ...active.rect, x: nextX, y: nextY }, { x: 0, y: 0, width: canvas.width, height: canvas.height }, peers);
      setGuides(snapped.guides);
      onChangeRef.current?.(markClipUserEdited(setClipLayout(currentProject, active.id, snapped.rect), active.id));
      return;
    }
    if (active.mode === 'project-resize' && currentProject) {
      const layout = resizeWithAspectRatio(active.rect, Math.max(80, active.rect.width + clientX - active.startX), false);
      onChangeRef.current?.(markClipUserEdited(setClipLayout(currentProject, active.id, layout), active.id));
      return;
    }
    updateCards((current) => {
      const rect = current[active.id];
      if (!rect) return current;
      if (active.mode === 'resize') {
        return { ...current, [active.id]: resizeWithAspectRatio(rect, Math.max(80, active.startWidth + clientX - active.startX), false) };
      }
      const dragOffset = active as { offsetX: number; offsetY: number };
      const nextX = Math.max(0, Math.min(canvas.width - rect.width, clientX - canvas.left - dragOffset.offsetX));
      const nextY = Math.max(0, Math.min(canvas.height - rect.height, clientY - canvas.top - dragOffset.offsetY));
      const snapped = snapRectToGuides({ ...rect, x: nextX, y: nextY }, { x: 0, y: 0, width: canvas.width, height: canvas.height }, Object.entries(current).filter(([id]) => id !== active.id).map(([, peer]) => peer));
      setGuides(snapped.guides);
      return { ...current, [active.id]: snapped.rect };
    });
  };

  const finish = () => { activeRef.current = undefined; setGuides({ x: [], y: [] }); };

  const togglePlayback = async () => {
    const media = mediaRef.current;
    if (!media) return;
    if (media.paused) {
      try {
        await media.play();
        setPlaying(true);
      } catch {
        setPlaying(false);
      }
    } else {
      media.pause();
      setPlaying(false);
    }
  };

  useEffect(() => {
    const move = (event: MouseEvent) => { if (activeRef.current) updateAt(event.clientX, event.clientY); };
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', finish);
    return () => { window.removeEventListener('mousemove', move); window.removeEventListener('mouseup', finish); };
  });

  const background = project?.tracks.background.clips[0];
  const resourceId = typeof background?.content === 'object' && background.content !== null && 'resourceId' in background.content ? String(background.content.resourceId) : undefined;
  const projectVisualClips = [...projectCards.map((clip) => ({ clip, kind: '卡片' })), ...(project?.tracks.graphics.clips ?? []).map((clip) => ({ clip, kind: '图形' })), ...(project?.tracks.subtitles.clips ?? []).map((clip) => ({ clip, kind: '字幕' }))];
  const projectVisualRects = projectVisualClips.map(({ clip }, index) => clip.layout ?? distributeHorizontally(canvasBounds, Math.min(3, Math.max(2, projectVisualClips.length)) as 2 | 3, 24)[index]);
  const clipText = (content: unknown): string => typeof content === 'object' && content !== null && 'text' in content ? String(content.text) : '';
  const graphicText = (content: unknown): string => typeof content === 'object' && content !== null && 'type' in content ? `图形：${String(content.type)}` : '图形';

  return (
    <section
      ref={canvasRef}
      aria-label="预览画布"
      onClick={(event) => {
        const target = event.target as HTMLElement;
        if (!target.closest('[aria-label^="项目"], button, select')) onSelect?.();
      }}
      onMouseMove={(event) => updateAt(event.clientX, event.clientY)}
      onMouseUp={finish}
      className="preview-canvas"
    >
      {/* 参考线下拉菜单 */}
      <div className="preview-guide-menu">
        <button
          className="preview-ref-btn"
          onClick={() => setShowGuides(!showGuides)}
        >
          参考线 ▾
        </button>
        {showGuides ? (
          <div className="preview-guide-dropdown">
            <button onClick={() => setShowGuides(true)}>水平参考线</button>
            <button onClick={() => setShowGuides(true)}>垂直参考线</button>
            <button onClick={() => setShowGuides(true)}>对角参考线</button>
            <hr />
            <button onClick={() => {/* 锁定 */}}>锁定参考线</button>
            <button onClick={() => setShowGuides(false)}>隐藏参考线</button>
          </div>
        ) : null}
      </div>

      {/* 16:9 预览帧 */}
      <div className="preview-frame">
        {/* 顶部标尺 */}
        <div className="preview-ruler-top">0　480　960　1440　1920</div>
        {/* 左侧标尺 */}
        <div className="preview-ruler-left">1080<br />720<br />360<br />0</div>
        {/* 参考线 */}
        {showGuides && <div className="preview-guide-line" />}

        {/* 实际画布内容 */}
        <div className="preview-canvas-inner" style={{ position: 'absolute', inset: 0 }}>
          {resourceId ? (
            <div aria-label={`预览背景 ${resourceId}`} style={{ position: 'absolute', inset: 0, background: '#243b53' }} />
          ) : null}
          {project?.media.kind === 'video' ? (
            <video
              ref={mediaRef as React.RefObject<HTMLVideoElement>}
              aria-label="主视频预览"
              src={toLocalFileUrl(project.media.path)}
              onEnded={() => setPlaying(false)}
              style={{ position: 'absolute', inset: 0, width: '100%', height: '100%', objectFit: 'contain' }}
            />
          ) : project?.media.kind === 'audio' ? (
            <audio
              ref={mediaRef as React.RefObject<HTMLAudioElement>}
              aria-label="主音频预览"
              src={toLocalFileUrl(project.media.path)}
              onEnded={() => setPlaying(false)}
            />
          ) : null}

          {/* 中心线 */}
          <i aria-hidden="true" style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, borderLeft: '1px dashed #6f86b0' }} />
          <i aria-hidden="true" style={{ position: 'absolute', top: '50%', left: 0, right: 0, borderTop: '1px dashed #6f86b0' }} />

          {/* 对齐参考线 */}
          {guides.x.map((x) => (
            <i key={`x-${x}`} aria-label="垂直对齐参考线" style={{ position: 'absolute', left: x, top: 0, bottom: 0, borderLeft: '1px solid #24b47e', pointerEvents: 'none' }} />
          ))}
          {guides.y.map((y) => (
            <i key={`y-${y}`} aria-label="水平对齐参考线" style={{ position: 'absolute', top: y, left: 0, right: 0, borderTop: '1px solid #24b47e', pointerEvents: 'none' }} />
          ))}



          {/* 项目可视片段 */}
          {projectVisualClips.map(({ clip, kind }, index) => {
            const rect = projectVisualRects[index];
            const localTextStyle = typeof clip.content === 'object' && clip.content !== null && 'textStyle' in clip.content
              ? clip.content.textStyle as { fontFamily?: string; fontSize?: number; color?: string; background?: string }
              : {};
            const subtitleStyle = kind === '字幕' ? { ...project?.subtitleStyle, ...localTextStyle } : undefined;
            return rect ? (
              <div
                key={clip.id}
                aria-label={`项目${kind} ${clip.id}`}
                onMouseDown={(event) => startProjectDrag(event, clip.id, rect)}
                onMouseMove={(event) => updateAt(event.clientX, event.clientY)}
                onClick={() => { persistProjectLayout(clip.id, rect); onSelect?.(clip.id); }}
                style={{
                  position: 'absolute',
                  left: rect.x,
                  top: rect.y,
                  width: rect.width,
                  height: rect.height,
                  border: `${clip.id === selectedClipId ? 2 : 1}px solid ${clip.id === selectedClipId ? '#ffffff' : cardColors[index % cardColors.length]}`,
                  color: subtitleStyle?.color ?? 'white',
                  background: subtitleStyle?.background,
                  fontFamily: subtitleStyle?.fontFamily,
                  fontSize: subtitleStyle?.fontSize,
                  touchAction: 'none',
                  userSelect: 'none',
                }}
              >
                {kind === '卡片' || kind === '字幕' ? clipText(clip.content) : graphicText(clip.content)}
                <button aria-label={`${kind}缩放控件 ${clip.id}`} onMouseDown={(event) => startProjectResize(event, clip.id, rect)}>
                  缩放
                </button>
              </div>
            ) : null;
          })}
        </div>
      </div>

      {/* 控制栏 */}
      <div className="preview-controls">
        <span className="preview-timecode">00:00:00:00 / 00:00:03:00</span>
        <button
          aria-label={playing ? '暂停预览' : '播放预览'}
          onClick={togglePlayback}
          className="preview-play-btn"
        >
          {playing ? '⏸' : '▶'}
        </button>
        {/* 画幅切换下拉菜单 */}
        <div className="preview-aspect-wrap">
          <button
            aria-label="预览画幅"
            className="preview-aspect-btn"
            onClick={() => { setAspectOpen(!aspectOpen); setQualityOpen(false); }}
          >
            {project?.aspectRatio ?? '16:9'} ▾
          </button>
          {aspectOpen ? (
            <div className="preview-aspect-dropdown">
              {ASPECT_RATIOS.map((ratio) => (
                <div
                  key={ratio}
                  className={`preview-aspect-option${ratio === (project?.aspectRatio ?? '16:9') ? ' active' : ''}`}
                  onClick={() => {
                    if (project) onChange?.({ ...project, aspectRatio: ratio as Project['aspectRatio'] });
                    setAspectOpen(false);
                  }}
                  role="button"
                  tabIndex={0}
                  onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); if (project) onChange?.({ ...project, aspectRatio: ratio as Project['aspectRatio'] }); setAspectOpen(false); } }}
                >
                  {ratio}
                  {ratio === (project?.aspectRatio ?? '16:9') ? <span className="preview-menu-dot">●</span> : null}
                </div>
              ))}
            </div>
          ) : null}
        </div>

        {/* 预览清晰度下拉菜单 */}
        <div className="preview-quality-wrap">
          <button
            aria-label="预览清晰度"
            className="preview-quality-btn"
            onClick={() => { setQualityOpen(!qualityOpen); setAspectOpen(false); }}
          >
            {previewQuality} ▾
          </button>
          {qualityOpen ? (
            <div className="preview-quality-dropdown">
              {PREVIEW_QUALITIES.map((q) => (
                <div
                  key={q}
                  className={`preview-quality-option${q === previewQuality ? ' active' : ''}`}
                  onClick={() => { setPreviewQuality(q); setQualityOpen(false); }}
                  role="button"
                  tabIndex={0}
                  onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); setPreviewQuality(q); setQualityOpen(false); } }}
                >
                  {q}
                  {q === previewQuality ? <span className="preview-menu-dot">●</span> : null}
                </div>
              ))}
            </div>
          ) : null}
        </div>
      </div>
    </section>
  );
}