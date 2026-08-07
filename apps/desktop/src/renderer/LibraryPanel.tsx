import { useEffect, useState } from 'react';
import type { Resource } from '@ai-video/domain';
import { toLocalFileUrl } from './fileUrl.js';

const types: Array<{ value: Resource['type'] | 'all'; label: string }> = [
  { value: 'all', label: '全部' }, { value: 'background', label: '背景' },
  { value: 'card-style', label: '卡片' }, { value: 'timeline-style', label: '时间轴' },
  { value: 'chart-style', label: '曲线图' },
];

export function LibraryPanel(): React.JSX.Element {
  const [type, setType] = useState<Resource['type'] | 'all'>('all');
  const [resources, setResources] = useState<Resource[]>([]);
  const [query, setQuery] = useState('');
  const [packagePath, setPackagePath] = useState('');
  const [error, setError] = useState('');
  const [importing, setImporting] = useState(false);
  const refresh = () => window.aiVideo.library.list(type === 'all' ? undefined : type).then(setResources);
  const visibleResources = resources.filter((resource) => `${resource.name} ${resource.category}`.toLocaleLowerCase().includes(query.trim().toLocaleLowerCase()));
  useEffect(() => { void refresh(); }, [type]);
  const importPackage = async () => {
    if (!packagePath || importing) return;
    setError('');
    setImporting(true);
    try {
      await window.aiVideo.library.importStylePackage(packagePath);
      setPackagePath('');
      await refresh();
    } catch (cause) {
      setError(`导入失败：${cause instanceof Error ? cause.message : String(cause)}`);
    } finally {
      setImporting(false);
    }
  };
  const choosePackage = async () => { const selected = await window.aiVideo.workspace.chooseFile(); if (selected) setPackagePath(selected); };
  return <section aria-label="资源库"><h2>资源库</h2><label>风格包路径<input aria-label="风格包路径" value={packagePath} onChange={(event) => setPackagePath(event.target.value)} /></label><button type="button" onClick={() => void choosePackage()}>选择风格包</button><button disabled={!packagePath || importing} onClick={() => void importPackage()}>导入风格包</button>{error ? <p role="alert">{error}</p> : null}<select aria-label="资源类型" value={type} onChange={(event) => setType(event.target.value as typeof type)}>{types.map((item) => <option key={item.value} value={item.value}>{item.label}</option>)}</select><label>搜索资源<input aria-label="搜索资源" value={query} onChange={(event) => setQuery(event.target.value)} /></label><div>{visibleResources.length === 0 ? <p>暂无已导入资源</p> : visibleResources.map((resource) => <article key={resource.id}><button onClick={() => void window.aiVideo.library.toggleFavorite(resource.id).then(refresh)}>{resource.favorite ? '取消收藏' : '收藏'}</button><button aria-label={`移出 ${resource.name}`} onClick={() => { if (window.confirm(`从资源库移除“${resource.name}”？`)) void window.aiVideo.library.remove(resource.id).then(refresh); }}>{'移出'}</button>{resource.thumbnailPath ? <img src={toLocalFileUrl(resource.thumbnailPath)} alt={resource.name} /> : null}<strong>{resource.name}</strong><label>分类<input aria-label={`分类 ${resource.name}`} defaultValue={resource.category} onBlur={(event) => { const category = event.target.value.trim(); if (category && category !== resource.category) void window.aiVideo.library.updateCategory(resource.id, category).then(refresh); }} /></label></article>)}</div></section>;
}
