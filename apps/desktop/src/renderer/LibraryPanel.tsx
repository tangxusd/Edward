import { useEffect, useState } from 'react';
import type { Resource } from '@ai-video/domain';

const types: Array<{ value: Resource['type'] | 'all'; label: string }> = [
  { value: 'all', label: '全部' }, { value: 'background', label: '背景' },
  { value: 'card-style', label: '卡片' }, { value: 'timeline-style', label: '时间轴' },
  { value: 'chart-style', label: '曲线图' },
];

export function LibraryPanel(): React.JSX.Element {
  const [type, setType] = useState<Resource['type'] | 'all'>('all');
  const [resources, setResources] = useState<Resource[]>([]);
  useEffect(() => { void window.aiVideo.library.list(type === 'all' ? undefined : type).then(setResources); }, [type]);
  return <section aria-label="资源库"><h2>资源库</h2><select aria-label="资源类型" value={type} onChange={(event) => setType(event.target.value as typeof type)}>{types.map((item) => <option key={item.value} value={item.value}>{item.label}</option>)}</select><div>{resources.length === 0 ? <p>暂无已导入资源</p> : resources.map((resource) => <article key={resource.id}><button onClick={() => void window.aiVideo.library.toggleFavorite(resource.id).then(() => window.aiVideo.library.list(type === 'all' ? undefined : type).then(setResources))}>{resource.favorite ? '取消收藏' : '收藏'}</button>{resource.thumbnailPath ? <img src={`file://${resource.thumbnailPath}`} alt={resource.name} /> : null}<strong>{resource.name}</strong><small>{resource.category}</small></article>)}</div></section>;
}
