import { useState } from 'react';
import { distributeHorizontally, resizeWithAspectRatio, snap, type Rect } from './timelineMath.js';

type ActivePointer =
  | { id: string; mode: 'drag'; offsetX: number; offsetY: number }
  | { id: string; mode: 'resize'; startX: number; startWidth: number };

const canvasBounds: Rect = { x: 120, y: 180, width: 720, height: 180 };
const cardColors = ['#4f7cff', '#24b47e', '#d9922e'];

export function PreviewCanvas(): React.JSX.Element {
  const [cards, setCards] = useState<Record<string, Rect>>({ 'card-1': { x: 300, y: 180, width: 320, height: 180 } });
  const [cardCount, setCardCount] = useState<1 | 2 | 3>(1);
  const [active, setActive] = useState<ActivePointer>();

  const setLayout = (count: 1 | 2 | 3) => {
    setCardCount(count);
    if (count === 1) {
      setCards((current) => ({ 'card-1': current['card-1'] ?? { x: 300, y: 180, width: 320, height: 180 } }));
      return;
    }
    const next = distributeHorizontally(canvasBounds, count, 24);
    setCards(Object.fromEntries(next.map((rect, index) => [`card-${index + 1}`, rect])));
  };

  const startDrag = (event: React.PointerEvent<HTMLDivElement>, id: string) => {
    const rect = cards[id];
    if (!rect) return;
    event.currentTarget.setPointerCapture(event.pointerId);
    const canvas = event.currentTarget.parentElement?.getBoundingClientRect();
    if (!canvas) return;
    setActive({ id, mode: 'drag', offsetX: event.clientX - canvas.left - rect.x, offsetY: event.clientY - canvas.top - rect.y });
  };

  const startResize = (event: React.PointerEvent<HTMLButtonElement>, id: string) => {
    event.stopPropagation();
    event.currentTarget.setPointerCapture(event.pointerId);
    const rect = cards[id];
    if (rect) setActive({ id, mode: 'resize', startX: event.clientX, startWidth: rect.width });
  };

  const moveActive = (event: React.PointerEvent<HTMLElement>) => {
    if (!active) return;
    const canvas = event.currentTarget.getBoundingClientRect();
    setCards((current) => {
      const rect = current[active.id];
      if (!rect) return current;
      if (active.mode === 'resize') {
        const nextWidth = Math.max(80, active.startWidth + event.clientX - active.startX);
        return { ...current, [active.id]: resizeWithAspectRatio(rect, nextWidth, false) };
      }
      const nextX = Math.max(0, Math.min(canvas.width - rect.width, event.clientX - canvas.left - active.offsetX));
      const nextY = Math.max(0, Math.min(canvas.height - rect.height, event.clientY - canvas.top - active.offsetY));
      return { ...current, [active.id]: { ...rect, x: snap(nextX, [canvas.width / 2 - rect.width / 2]), y: snap(nextY, [canvas.height / 2 - rect.height / 2]) } };
    });
  };

  return <section aria-label="预览画布" onPointerMove={moveActive} onPointerUp={() => setActive(undefined)} onPointerCancel={() => setActive(undefined)} style={{ position: 'relative', aspectRatio: '16 / 9', background: '#151923', overflow: 'hidden' }}><i aria-hidden="true" style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, borderLeft: '1px dashed #6f86b0' }} /><i aria-hidden="true" style={{ position: 'absolute', top: '50%', left: 0, right: 0, borderTop: '1px dashed #6f86b0' }} /><div role="toolbar" aria-label="卡片布局"><button onClick={() => setLayout(1)} aria-pressed={cardCount === 1}>单卡</button><button onClick={() => setLayout(2)} aria-pressed={cardCount === 2}>双卡</button><button onClick={() => setLayout(3)} aria-pressed={cardCount === 3}>三卡</button></div>{Object.entries(cards).filter(([id]) => Number(id.slice(-1)) <= cardCount).map(([id, rect], index) => <div key={id} aria-label="可拖拽卡片" onPointerDown={(event) => startDrag(event, id)} style={{ position: 'absolute', left: rect.x, top: rect.y, width: rect.width, height: rect.height, border: `1px solid ${cardColors[index]}`, color: 'white', touchAction: 'none', userSelect: 'none' }}>卡片 {index + 1}<button aria-label="缩放卡片" onPointerDown={(event) => startResize(event, id)}>缩放</button></div>)}</section>;
}
