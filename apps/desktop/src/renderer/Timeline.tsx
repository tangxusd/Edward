import { useEffect, useRef, useState } from 'react';
import { copyClip, deleteClip, markClipUserEdited, moveClip, resizeClip, type Project, type TimelineClip, type TrackId } from '@ai-video/domain';

const pixelsPerSecond = 60;
const tracks: Array<{ id: TrackId; label: string }> = [
  { id: 'graphics', label: '图形' }, { id: 'cards', label: '卡片' }, { id: 'subtitles', label: '字幕' }, { id: 'background', label: '背景' }, { id: 'mainMedia', label: '主媒体' },
];

type ActiveClip = { trackId: TrackId; clipId: string; mode: 'move' | 'resize'; startX: number; startStart: number; startDuration: number };

export function Timeline({ project, onChange, onSelect }: { project?: Project; onChange?: (project: Project) => void; onSelect?: (clipId: string) => void }): React.JSX.Element {
  const [draft, setDraft] = useState(project);
  const [active, setActive] = useState<ActiveClip>();
  const activeRef = useRef<ActiveClip | undefined>(undefined);
  const draftRef = useRef(project);

  useEffect(() => { draftRef.current = project; setDraft(project); }, [project]);

  const startClipPointer = (event: React.MouseEvent<HTMLDivElement>, trackId: TrackId, clip: TimelineClip) => {
    onSelect?.(clip.id);
    const next = { trackId, clipId: clip.id, mode: 'move' as const, startX: event.clientX, startStart: clip.start, startDuration: Math.max(clip.duration, 0.5) };
    activeRef.current = next;
    setActive(next);
  };

  const startResizePointer = (event: React.MouseEvent<HTMLButtonElement>, trackId: TrackId, clip: TimelineClip) => {
    event.stopPropagation();
    const next = { trackId, clipId: clip.id, mode: 'resize' as const, startX: event.clientX, startStart: clip.start, startDuration: Math.max(clip.duration, 0.5) };
    activeRef.current = next;
    setActive(next);
  };

  const updateAtX = (clientX: number) => {
    const currentActive = activeRef.current;
    if (!currentActive) return;
    const deltaSeconds = (clientX - currentActive.startX) / pixelsPerSecond;
    setDraft((current) => {
      if (!current) return current;
      const changed = currentActive.mode === 'move' ? moveClip(current, currentActive.clipId, Math.max(0, currentActive.startStart + deltaSeconds)) : resizeClip(current, currentActive.clipId, Math.max(0.1, currentActive.startDuration + deltaSeconds));
      const next = markClipUserEdited(changed, currentActive.clipId);
      draftRef.current = next;
      return next;
    });
  };

  const moveClipPointer = (event: React.MouseEvent<HTMLElement>) => updateAtX(event.clientX);

  const finishClipPointer = () => {
    if (activeRef.current && draftRef.current) onChange?.(draftRef.current);
    activeRef.current = undefined;
    setActive(undefined);
  };

  useEffect(() => {
    const move = (event: MouseEvent) => { if (event.buttons === 1) updateAtX(event.clientX); };
    const up = () => finishClipPointer();
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
    return () => { window.removeEventListener('mousemove', move); window.removeEventListener('mouseup', up); };
  });

  const mutateClip = (operation: (current: Project) => Project, clipId: string) => {
    if (!draft) return;
    const next = operation(draft);
    setDraft(next);
    onChange?.(next);
    if (active?.clipId === clipId) setActive(undefined);
  };

  return <section aria-label="多轨时间线" onMouseMove={moveClipPointer} onMouseUp={finishClipPointer} onMouseLeave={finishClipPointer}><h2>时间线</h2>{tracks.map(({ id, label }) => <div key={id} aria-label={`${label}轨道`}><strong>{label}</strong><div style={{ minHeight: 44, position: 'relative' }}>{draft?.tracks[id].clips.map((clip) => <div key={clip.id} role="button" tabIndex={0} onMouseDown={(event) => startClipPointer(event, id, clip)} onMouseMove={moveClipPointer} style={{ display: 'inline-flex', position: 'relative', marginLeft: `${clip.start * pixelsPerSecond}px`, width: `${Math.max(40, clip.duration * pixelsPerSecond)}px`, minHeight: 36, alignItems: 'center', justifyContent: 'space-between', background: '#26334d', color: 'white', touchAction: 'none', userSelect: 'none' }}><span>{clip.id}</span><button aria-label={`调整 ${clip.id} 时长`} onMouseDown={(event) => startResizePointer(event, id, clip)}>↔</button><button aria-label={`复制 ${clip.id}`} onClick={() => mutateClip((current) => copyClip(current, clip.id), clip.id)}>复制</button><button aria-label={`删除 ${clip.id}`} onClick={() => mutateClip((current) => deleteClip(current, clip.id), clip.id)}>删除</button></div>)}</div></div>)}</section>;
}
