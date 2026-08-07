import { useEffect, useRef, useState } from 'react';
import { markClipUserEdited, setClipLayout, type Project } from '@ai-video/domain';
import { distributeHorizontally, resizeWithAspectRatio, snap, type Rect } from './timelineMath.js';

type ActivePointer =
  | { id: string; mode: 'drag'; offsetX: number; offsetY: number }
  | { id: string; mode: 'resize'; startX: number; startWidth: number }
  | { id: string; mode: 'project-drag'; offsetX: number; offsetY: number; rect: Rect }
  | { id: string; mode: 'project-resize'; startX: number; rect: Rect };

const canvasBounds: Rect = { x: 120, y: 180, width: 720, height: 180 };
const cardColors = ['#4f7cff', '#24b47e', '#d9922e'];

export function PreviewCanvas({ project, onChange, onSelect }: { project?: Project; onChange?: (project: Project) => void; onSelect?: (clipId: string) => void }): React.JSX.Element {
  const [cards, setCards] = useState<Record<string, Rect>>({ 'card-1': { x: 300, y: 180, width: 320, height: 180 } });
  const [cardCount, setCardCount] = useState<1 | 2 | 3>(1);
  const cardsRef = useRef(cards);
  const activeRef = useRef<ActivePointer | undefined>(undefined);
  const canvasRef = useRef<HTMLElement | null>(null);
  const projectRef = useRef(project);
  const onChangeRef = useRef(onChange);

  useEffect(() => { projectRef.current = project; onChangeRef.current = onChange; }, [project, onChange]);

  const updateCards = (update: (current: Record<string, Rect>) => Record<string, Rect>) => {
    setCards((current) => {
      const next = update(current);
      cardsRef.current = next;
      return next;
    });
  };

  const setLayout = (count: 1 | 2 | 3) => {
    setCardCount(count);
    if (count === 1) {
      updateCards((current) => ({ 'card-1': current['card-1'] ?? { x: 300, y: 180, width: 320, height: 180 } }));
      return;
    }
    const next = distributeHorizontally(canvasBounds, count, 24);
    updateCards(() => Object.fromEntries(next.map((rect, index) => [`card-${index + 1}`, rect])));
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
      const layout = { ...active.rect, x: snap(nextX, [canvas.width / 2 - active.rect.width / 2]), y: snap(nextY, [canvas.height / 2 - active.rect.height / 2]) };
      onChangeRef.current?.(markClipUserEdited(setClipLayout(currentProject, active.id, layout), active.id));
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
      const nextX = Math.max(0, Math.min(canvas.width - rect.width, clientX - canvas.left - active.offsetX));
      const nextY = Math.max(0, Math.min(canvas.height - rect.height, clientY - canvas.top - active.offsetY));
      return { ...current, [active.id]: { ...rect, x: snap(nextX, [canvas.width / 2 - rect.width / 2]), y: snap(nextY, [canvas.height / 2 - rect.height / 2]) } };
    });
  };

  const finish = () => { activeRef.current = undefined; };

  useEffect(() => {
    const move = (event: MouseEvent) => { if (activeRef.current) updateAt(event.clientX, event.clientY); };
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', finish);
    return () => { window.removeEventListener('mousemove', move); window.removeEventListener('mouseup', finish); };
  });

  const background = project?.tracks.background.clips[0];
  const resourceId = typeof background?.content === 'object' && background.content !== null && 'resourceId' in background.content ? String(background.content.resourceId) : undefined;
  const projectCards = project?.tracks.cards.clips ?? [];
  const projectCardRects = projectCards.map((clip, index) => clip.layout ?? distributeHorizontally(canvasBounds, Math.min(3, Math.max(2, projectCards.length)) as 2 | 3, 24)[index]);
  const clipText = (content: unknown): string => typeof content === 'object' && content !== null && 'text' in content ? String(content.text) : '';
  return <section ref={canvasRef} aria-label="预览画布" onMouseMove={(event) => updateAt(event.clientX, event.clientY)} onMouseUp={finish} style={{ position: 'relative', aspectRatio: '16 / 9', background: '#151923', overflow: 'hidden' }}>{resourceId ? <div aria-label={`预览背景 ${resourceId}`} style={{ position: 'absolute', inset: 0, background: '#243b53' }} /> : null}<i aria-hidden="true" style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, borderLeft: '1px dashed #6f86b0' }} /><i aria-hidden="true" style={{ position: 'absolute', top: '50%', left: 0, right: 0, borderTop: '1px dashed #6f86b0' }} /><div role="toolbar" aria-label="卡片布局"><button onClick={() => setLayout(1)} aria-pressed={cardCount === 1}>单卡</button><button onClick={() => setLayout(2)} aria-pressed={cardCount === 2}>双卡</button><button onClick={() => setLayout(3)} aria-pressed={cardCount === 3}>三卡</button></div>{projectCards.map((clip, index) => { const rect = projectCardRects[index]; return rect ? <div key={clip.id} aria-label={`项目卡片 ${clip.id}`} onMouseDown={(event) => startProjectDrag(event, clip.id, rect)} onMouseMove={(event) => updateAt(event.clientX, event.clientY)} onClick={() => { persistProjectLayout(clip.id, rect); onSelect?.(clip.id); }} style={{ position: 'absolute', left: rect.x, top: rect.y, width: rect.width, height: rect.height, border: `1px solid ${cardColors[index % cardColors.length]}`, color: 'white', touchAction: 'none', userSelect: 'none' }}>{clipText(clip.content)}<button aria-label={`卡片缩放控件 ${clip.id}`} onMouseDown={(event) => startProjectResize(event, clip.id, rect)}>缩放</button></div> : null; })}{projectCards.length === 0 ? null : null}</section>;
}
