import { useState } from 'react';

export function ExportDialog(): React.JSX.Element {
  const [aspect, setAspect] = useState('16:9');
  const [resolution, setResolution] = useState('1920x1080');
  const [transparent, setTransparent] = useState(false);
  const [directory, setDirectory] = useState('');
  return <section aria-label="导出"><h2>手动导出</h2><label>画幅<select value={aspect} onChange={(e) => setAspect(e.target.value)}><option>16:9</option><option>9:16</option></select></label><label>分辨率<select value={resolution} onChange={(e) => setResolution(e.target.value)}><option>1280x720</option><option>1920x1080</option><option>2560x1440</option><option>3840x2160</option></select></label><label><input type="checkbox" checked={transparent} onChange={(e) => setTransparent(e.target.checked)} />透明背景</label><label>输出目录<input value={directory} onChange={(e) => setDirectory(e.target.value)} /></label><button disabled={!directory}>开始导出</button></section>;
}
