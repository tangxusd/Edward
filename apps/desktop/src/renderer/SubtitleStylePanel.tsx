import { useState } from 'react';
import type { SubtitleStyle } from '@ai-video/domain';

const initial: SubtitleStyle = { fontFamily: 'Arial', fontSize: 48, color: '#ffffff', background: 'rgba(0,0,0,.55)' };

export function SubtitleStylePanel(): React.JSX.Element {
  const [style, setStyle] = useState(initial);
  return <section aria-label="字幕样式"><h2>字幕样式</h2><label>字体<input value={style.fontFamily} onChange={(event) => setStyle({ ...style, fontFamily: event.target.value })} /></label><label>字号<input type="number" value={style.fontSize} onChange={(event) => setStyle({ ...style, fontSize: Number(event.target.value) })} /></label><label>颜色<input type="color" value={style.color} onChange={(event) => setStyle({ ...style, color: event.target.value })} /></label><label>文字背景<input value={style.background} onChange={(event) => setStyle({ ...style, background: event.target.value })} /></label></section>;
}
