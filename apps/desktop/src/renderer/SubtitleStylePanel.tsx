import { useEffect, useState } from 'react';
import type { Project, SubtitleStyle } from '@ai-video/domain';

const initial: SubtitleStyle = { fontFamily: 'Arial', fontSize: 48, color: '#ffffff', background: 'rgba(0,0,0,.55)' };

export function SubtitleStylePanel({ project, onChange }: { project?: Project; onChange?: (project: Project) => void }): React.JSX.Element {
  const [style, setStyle] = useState<SubtitleStyle>(project?.subtitleStyle ?? initial);
  useEffect(() => setStyle(project?.subtitleStyle ?? initial), [project]);
  const update = (next: SubtitleStyle) => {
    setStyle(next);
    if (project) onChange?.({ ...project, subtitleStyle: next });
  };
  return <section aria-label="字幕样式"><h2>字幕样式</h2><label>字体<input value={style.fontFamily} onChange={(event) => update({ ...style, fontFamily: event.target.value })} /></label><label>字号<input type="number" value={style.fontSize} onChange={(event) => update({ ...style, fontSize: Number(event.target.value) })} /></label><label>颜色<input type="color" value={style.color} onChange={(event) => update({ ...style, color: event.target.value })} /></label><label>文字背景<input value={style.background} onChange={(event) => update({ ...style, background: event.target.value })} /></label></section>;
}
