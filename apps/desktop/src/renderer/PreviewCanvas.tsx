import { useState } from 'react';
import { resizeWithAspectRatio, type Rect } from './timelineMath.js';

export function PreviewCanvas(): React.JSX.Element {
  const [rect, setRect] = useState<Rect>({ x: 300, y: 180, width: 320, height: 180 });
  return <section aria-label="预览画布" style={{ position: 'relative', aspectRatio: '16 / 9', background: '#151923', overflow: 'hidden' }}><i style={{ position: 'absolute', left: '50%', top: 0, bottom: 0, borderLeft: '1px dashed #6f86b0' }} /><i style={{ position: 'absolute', top: '50%', left: 0, right: 0, borderTop: '1px dashed #6f86b0' }} /><div aria-label="可拖拽卡片" style={{ position: 'absolute', left: rect.x, top: rect.y, width: rect.width, height: rect.height, border: '1px solid #4f7cff', color: 'white' }}>卡片<button aria-label="缩放卡片" onClick={() => setRect((current) => resizeWithAspectRatio(current, current.width + 20, false))}>缩放</button></div></section>;
}
