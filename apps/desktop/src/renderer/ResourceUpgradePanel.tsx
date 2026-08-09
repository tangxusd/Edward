import { useState, type KeyboardEvent } from 'react';

export type ResourceUpgradeCategory = {
  key: string;
  label: string;
  items: { id: string; name: string }[];
};

type Props = {
  categories: ResourceUpgradeCategory[];
  onClose: () => void;
  onUpgradeAll?: (categoryKey: string) => void;
  onUpgradeItem?: (categoryKey: string, itemId: string) => void;
};

export function ResourceUpgradePanel({ categories, onClose, onUpgradeAll, onUpgradeItem }: Props): React.JSX.Element {
  const [active, setActive] = useState(categories[0]?.key ?? '');

  const current = categories.find((c) => c.key === active) ?? categories[0];

  const handleKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === 'Escape') {
      event.preventDefault();
      onClose();
    }
  };

  return (
    <div className="resource-upgrade-overlay" onClick={onClose}>
      <div
        role="dialog"
        aria-label="资源库升级"
        aria-modal="true"
        className="resource-upgrade-panel"
        onKeyDown={handleKeyDown}
        onClick={(e) => e.stopPropagation()}
      >
        <header className="resource-upgrade-header">
          <h2>资源库升级</h2>
          <button type="button" className="resource-upgrade-close" aria-label="关闭" onClick={onClose}>×</button>
        </header>

        <div className="resource-upgrade-shell">
          <nav className="resource-upgrade-nav">
            {categories.map((cat) => (
              <div
                key={cat.key}
                className={`resource-upgrade-cat${cat.key === active ? ' active' : ''}`}
                onClick={() => setActive(cat.key)}
                role="button"
                tabIndex={0}
                onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); setActive(cat.key); } }}
              >
                {cat.label}
              </div>
            ))}
          </nav>

          {current ? (
            <section className="resource-upgrade-section">
              <div className="resource-upgrade-top">
                <b>{current.label}</b>
                <button
                  className="resource-upgrade-btn"
                  onClick={() => onUpgradeAll?.(current.key)}
                  disabled={current.items.length === 0}
                >
                  升级全部{current.label}
                </button>
              </div>
              <div className="resource-upgrade-grid">
                {current.items.map((item) => (
                  <div
                    key={item.id}
                    className="resource-upgrade-card"
                    onClick={() => onUpgradeItem?.(current.key, item.id)}
                    role="button"
                    tabIndex={0}
                    onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); onUpgradeItem?.(current.key, item.id); } }}
                  >
                    {item.name}
                  </div>
                ))}
                {current.items.length === 0 ? (
                  <div className="resource-upgrade-card" style={{ display: 'grid', placeItems: 'center', color: '#777', cursor: 'default' }}>
                    暂无{current.label}资源
                  </div>
                ) : null}
              </div>
            </section>
          ) : null}
        </div>
      </div>
    </div>
  );
}
