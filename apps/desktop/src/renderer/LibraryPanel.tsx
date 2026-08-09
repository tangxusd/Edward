import { useEffect, useState } from 'react';
import type { Resource } from '@ai-video/domain';
import { toLocalFileUrl } from './fileUrl.js';

/* ------------------------------------------------------------------ */
/*  Tab-to-resource-type mapping                                       */
/* ------------------------------------------------------------------ */
const RESOURCE_TYPE_MAP: Record<string, Resource['type'] | undefined> = {
  '卡片': 'card-style',
  '图表': 'chart-style',
  '背景': 'background',
};

/* ------------------------------------------------------------------ */
/*  Shared helpers                                                     */
/* ------------------------------------------------------------------ */
function starIcon(favorited: boolean) {
  return favorited ? '★' : '☆';
}

function thumbnailUrl(resource: Resource): string | undefined {
  return resource.thumbnailPath ? toLocalFileUrl(resource.thumbnailPath) : undefined;
}

/* ------------------------------------------------------------------ */
/*  Resource grid — shared by 卡片／图表／背景                          */
/* ------------------------------------------------------------------ */
function ResourceGrid({
  resources,
  query,
  onQueryChange,
  onFavorite,
  onRemove,
  onAdd,
  categoryLabel,
}: {
  resources: Resource[];
  query: string;
  onQueryChange: (v: string) => void;
  onFavorite: (id: string) => void;
  onRemove: (id: string) => void;
  onAdd: (id: string) => void;
  categoryLabel: string;
}) {
  const filtered = resources.filter((r) =>
    r.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <>
      <input
        className="library-search"
        placeholder={`搜索${categoryLabel}`}
        value={query}
        onChange={(e) => onQueryChange(e.target.value)}
      />
      <section className="library-grid">
        {filtered.length === 0 && (
          <p className="library-empty">暂无已导入资源</p>
        )}
        {filtered.map((r) => (
          <article key={r.id} className="library-item">
            <button
              className="library-item-delete"
              onClick={() => {
                if (window.confirm(`从资源库移除「${r.name}」？`))
                  onRemove(r.id);
              }}
            >
              ×
            </button>
            <div
              className="library-item-thumb"
              style={
                thumbnailUrl(r)
                  ? { backgroundImage: `url(${thumbnailUrl(r)})`, backgroundSize: 'cover', backgroundPosition: 'center' }
                  : { background: 'linear-gradient(135deg, #356e9e, #84527b)' }
              }
            >
              {!thumbnailUrl(r) && <span className="library-thumb-label">{r.name.charAt(0)}</span>}
            </div>
            <div className="library-item-name">{r.name}</div>
            <div className="library-item-bottom">
              <span>{categoryLabel}</span>
              <span className="library-item-actions">
                <button
                  className={`library-icon-btn${r.favorite ? ' library-star' : ''}`}
                  onClick={() => onFavorite(r.id)}
                >
                  {starIcon(r.favorite)}
                </button>
                <button className="library-icon-btn" onClick={() => onAdd(r.id)}>
                  ＋
                </button>
              </span>
            </div>
          </article>
        ))}
      </section>
    </>
  );
}

/* ------------------------------------------------------------------ */
/*  Tab: 媒体 — show all resources                                     */
/* ------------------------------------------------------------------ */
function MediaLibrary({
  resources,
  query,
  onQueryChange,
  onFavorite,
  onRemove,
  onAdd,
}: {
  resources: Resource[];
  query: string;
  onQueryChange: (v: string) => void;
  onFavorite: (id: string) => void;
  onRemove: (id: string) => void;
  onAdd: (id: string) => void;
}) {
  const filtered = resources.filter((r) =>
    r.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <>
      <input
        className="library-search"
        placeholder="搜索素材"
        value={query}
        onChange={(e) => onQueryChange(e.target.value)}
      />
      <section className="library-grid">
        {filtered.length === 0 && (
          <p className="library-empty">暂无已导入资源</p>
        )}
        {filtered.map((r) => (
          <article key={r.id} className="library-item">
            <button
              className="library-item-delete"
              onClick={() => {
                if (window.confirm(`从资源库移除「${r.name}」？`))
                  onRemove(r.id);
              }}
            >
              ×
            </button>
            <div
              className="library-item-thumb"
              style={
                thumbnailUrl(r)
                  ? { backgroundImage: `url(${thumbnailUrl(r)})`, backgroundSize: 'cover', backgroundPosition: 'center' }
                  : { background: 'linear-gradient(135deg, #356e9e, #84527b)' }
              }
            >
              {!thumbnailUrl(r) && <span className="library-thumb-label">{r.name.charAt(0)}</span>}
            </div>
            <div className="library-item-name">{r.name}</div>
            <div className="library-item-bottom">
              <span>{r.category}</span>
              <span className="library-item-actions">
                <button
                  className={`library-icon-btn${r.favorite ? ' library-star' : ''}`}
                  onClick={() => onFavorite(r.id)}
                >
                  {starIcon(r.favorite)}
                </button>
                <button className="library-icon-btn" onClick={() => onAdd(r.id)}>
                  ＋
                </button>
              </span>
            </div>
          </article>
        ))}
      </section>
    </>
  );
}

/* ------------------------------------------------------------------ */
/*  Tab: 文本 — 匹配 text-library-v1.html                               */
/* ------------------------------------------------------------------ */
const TEXT_PRESETS = [
  { label: '默认文本', style: 'font-size:17px' },
  { label: '标题', style: 'font-size:22px;font-weight:700' },
  { label: '说明文字', style: 'font-style:italic' },
  { label: '副标题', style: 'font-size:14px;color:#aaa' },
  { label: '引文', style: 'font-size:15px;border-left:2px solid #20d5cc;padding-left:8px' },
  { label: '大标题', style: 'font-size:28px;font-weight:700;letter-spacing:2px' },
];

function TextLibrary() {
  const [query, setQuery] = useState('');
  const filtered = TEXT_PRESETS.filter((t) =>
    t.label.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <>
      <button className="library-new-btn">新建文本</button>
      <input
        className="library-search"
        placeholder="搜索文本样式"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <section className="library-grid">
        {filtered.map((t) => (
          <article key={t.label} className="library-text-card">
            <div className="library-text-preview" style={parseStyle(t.style)}>
              {t.label}
            </div>
            <div className="library-text-sub">拖拽到时间线</div>
          </article>
        ))}
      </section>
    </>
  );
}

/* Parse inline style string into React CSSProperties */
function parseStyle(s: string): React.CSSProperties {
  const props: Record<string, string> = {};
  for (const part of s.split(';')) {
    const trimmed = part.trim();
    if (!trimmed) continue;
    const colonIdx = trimmed.indexOf(':');
    if (colonIdx === -1) continue;
    const key = trimmed.slice(0, colonIdx).trim();
    const val = trimmed.slice(colonIdx + 1).trim();
    // Convert kebab-case to camelCase
    const camel = key.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
    (props as any)[camel] = val;
  }
  return props as React.CSSProperties;
}

/* ------------------------------------------------------------------ */
/*  Tab: 音频 — 匹配 audio-library-v7-small-delete.html                */
/* ------------------------------------------------------------------ */
const AUDIO_SAMPLES = [
  { name: '苹果键盘敲击打字声', duration: '00:08' },
  { name: '轻快转场提示音', duration: '00:02' },
  { name: '环境氛围', duration: '00:10' },
  { name: '通知提示音', duration: '00:01' },
  { name: '风声背景', duration: '00:15' },
  { name: '水滴声', duration: '00:03' },
];

function AudioLibrary() {
  const [query, setQuery] = useState('');
  const [favorites, setFavorites] = useState<Set<string>>(new Set(['苹果键盘敲击打字声']));
  const filtered = AUDIO_SAMPLES.filter((a) =>
    a.name.toLowerCase().includes(query.toLowerCase()),
  );

  const toggleFav = (name: string) => {
    setFavorites((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };

  return (
    <>
      <input
        className="library-search"
        placeholder="搜索音效"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <section className="library-grid">
        {filtered.map((a) => {
          const fav = favorites.has(a.name);
          return (
            <article key={a.name} className="library-item">
              <button className="library-item-delete">×</button>
              <div className="library-item-name">{a.name}</div>
              <div className="library-item-bottom">
                <span>音效 · {a.duration}</span>
                <span className="library-item-actions">
                  <button
                    className={`library-icon-btn${fav ? ' library-star' : ''}`}
                    onClick={() => toggleFav(a.name)}
                  >
                    {fav ? '★' : '☆'}
                  </button>
                  <button className="library-icon-btn">＋</button>
                </span>
              </div>
            </article>
          );
        })}
      </section>
    </>
  );
}

/* ------------------------------------------------------------------ */
/*  Tab: 标注 — 匹配 annotation-library-v1.html                        */
/* ------------------------------------------------------------------ */
const ANNOTATION_PRESETS = [
  { name: '框选', symbol: '□' },
  { name: '引出标注', symbol: '↗' },
  { name: '箭头标注', symbol: '→' },
  { name: '高亮', symbol: '🖉' },
  { name: '下划线', symbol: '⎁' },
  { name: '圈选', symbol: '○' },
];

function AnnotationLibrary() {
  const [query, setQuery] = useState('');
  const filtered = ANNOTATION_PRESETS.filter((a) =>
    a.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <>
      <input
        className="library-search"
        placeholder="搜索标注"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <section className="library-grid">
        {filtered.map((a) => (
          <article key={a.name} className="library-item">
            <button className="library-item-delete">×</button>
            <div className="library-item-thumb library-thumb-symbol">
              {a.symbol}
            </div>
            <div className="library-item-name">{a.name}</div>
            <div className="library-item-bottom">
              <span>标注</span>
              <span className="library-item-actions">
                <button className="library-icon-btn">☆</button>
                <button className="library-icon-btn">＋</button>
              </span>
            </div>
          </article>
        ))}
      </section>
    </>
  );
}

/* ------------------------------------------------------------------ */
/*  Tab: 数字 — 匹配 number-library-v1.html                            */
/* ------------------------------------------------------------------ */
const NUMBER_PRESETS = [
  { name: '百分比高亮', display: '72%' },
  { name: '数字标题', display: '2026' },
  { name: '计数器', display: '1,280' },
  { name: '价格标签', display: '¥99' },
  { name: '统计数据', display: '10M' },
  { name: '年份', display: '2024' },
];

function NumberLibrary() {
  const [query, setQuery] = useState('');
  const filtered = NUMBER_PRESETS.filter((n) =>
    n.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <>
      <input
        className="library-search"
        placeholder="搜索数字组件"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <section className="library-grid">
        {filtered.map((n) => (
          <article key={n.name} className="library-item">
            <button className="library-item-delete">×</button>
            <div className="library-item-thumb library-thumb-number">
              {n.display}
            </div>
            <div className="library-item-name">{n.name}</div>
            <div className="library-item-bottom">
              <span>数字</span>
              <span className="library-item-actions">
                <button className="library-icon-btn">☆</button>
                <button className="library-icon-btn">＋</button>
              </span>
            </div>
          </article>
        ))}
      </section>
    </>
  );
}

/* ------------------------------------------------------------------ */
/*  Main LibraryPanel component                                        */
/* ------------------------------------------------------------------ */
interface LibraryPanelProps {
  tab: string;
}

export function LibraryPanel({ tab }: LibraryPanelProps): React.JSX.Element {
  const [resources, setResources] = useState<Resource[]>([]);
  const [query, setQuery] = useState('');

  const resourceType = RESOURCE_TYPE_MAP[tab];

  /* Load resources for resource-backed tabs */
  useEffect(() => {
    if (!resourceType) return;
    let disposed = false;
    const reload = () => {
      void window.aiVideo.library
        .list(resourceType)
        .then((next) => {
          if (!disposed) setResources(next);
        });
    };
    reload();
    window.addEventListener('workspace-changed', reload);
    return () => {
      disposed = true;
      window.removeEventListener('workspace-changed', reload);
    };
  }, [resourceType]);

  /* Load all resources for 媒体 tab */
  useEffect(() => {
    if (tab !== '媒体') return;
    let disposed = false;
    const reload = () => {
      void window.aiVideo.library
        .list()
        .then((next) => {
          if (!disposed) setResources(next);
        });
    };
    reload();
    window.addEventListener('workspace-changed', reload);
    return () => {
      disposed = true;
      window.removeEventListener('workspace-changed', reload);
    };
  }, [tab]);

  const refresh = () => {
    if (resourceType) {
      void window.aiVideo.library.list(resourceType).then(setResources);
    } else if (tab === '媒体') {
      void window.aiVideo.library.list().then(setResources);
    }
  };

  const handleFavorite = (id: string) => {
    void window.aiVideo.library.toggleFavorite(id).then(refresh);
  };

  const handleRemove = (id: string) => {
    void window.aiVideo.library.remove(id).then(refresh);
  };

  const handleAdd = (_id: string) => {
    // Placeholder — add to timeline
  };

  /* Render the correct tab view */
  switch (tab) {
    case '媒体':
      return (
        <div className="library-panel">
          <MediaLibrary
            resources={resources}
            query={query}
            onQueryChange={setQuery}
            onFavorite={handleFavorite}
            onRemove={handleRemove}
            onAdd={handleAdd}
          />
        </div>
      );

    case '文本':
      return (
        <div className="library-panel">
          <TextLibrary />
        </div>
      );

    case '音频':
      return (
        <div className="library-panel">
          <AudioLibrary />
        </div>
      );

    case '卡片':
      return (
        <div className="library-panel">
          <ResourceGrid
            resources={resources}
            query={query}
            onQueryChange={setQuery}
            onFavorite={handleFavorite}
            onRemove={handleRemove}
            onAdd={handleAdd}
            categoryLabel="卡片"
          />
        </div>
      );

    case '图表':
      return (
        <div className="library-panel">
          <ResourceGrid
            resources={resources}
            query={query}
            onQueryChange={setQuery}
            onFavorite={handleFavorite}
            onRemove={handleRemove}
            onAdd={handleAdd}
            categoryLabel="图表"
          />
        </div>
      );

    case '背景':
      return (
        <div className="library-panel">
          <ResourceGrid
            resources={resources}
            query={query}
            onQueryChange={setQuery}
            onFavorite={handleFavorite}
            onRemove={handleRemove}
            onAdd={handleAdd}
            categoryLabel="背景"
          />
        </div>
      );

    case '标注':
      return (
        <div className="library-panel">
          <AnnotationLibrary />
        </div>
      );

    case '数字':
      return (
        <div className="library-panel">
          <NumberLibrary />
        </div>
      );

    default:
      return (
        <div className="library-panel">
          <p className="library-empty">未知标签</p>
        </div>
      );
  }
}