import type { Project, TrackId } from '@ai-video/domain';

const tracks: Array<{ id: TrackId; label: string }> = [
  { id: 'mainMedia', label: '主媒体' }, { id: 'background', label: '背景' }, { id: 'subtitles', label: '字幕' }, { id: 'cards', label: '卡片' }, { id: 'graphics', label: '图形' },
];

export function Timeline({ project }: { project?: Project }): React.JSX.Element {
  return <section aria-label="多轨时间线"><h2>时间线</h2>{tracks.map(({ id, label }) => <div key={id} aria-label={`${label}轨道`}><strong>{label}</strong><div>{project?.tracks[id].clips.map((clip) => <button key={clip.id} style={{ marginLeft: `${clip.start * 10}px`, width: `${Math.max(40, clip.duration * 10)}px` }}>{clip.id}</button>)}</div></div>)}</section>;
}
