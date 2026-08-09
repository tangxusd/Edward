import { useState } from 'react';
import { LibraryPanel } from './LibraryPanel.js';

const TABS = ['媒体', '文本', '音频', '卡片', '图表', '背景', '标注', '数字'] as const;

export function LeftPanel(): React.JSX.Element {
  const [activeTab, setActiveTab] = useState<string>('媒体');

  return (
    <aside className="left-panel">
      <div className="left-panel-tabs">
        {TABS.map((tab) => (
          <button
            key={tab}
            className={`left-panel-tab${activeTab === tab ? ' active' : ''}`}
            onClick={() => setActiveTab(tab)}
          >
            {tab}
          </button>
        ))}
      </div>
      <div className="left-panel-content">
        {activeTab === '媒体' ? (
          <div className="left-panel-import-box">
            <span className="left-panel-import-label">导入素材</span>
            <div className="left-panel-import-placeholder" />
          </div>
        ) : null}
        <LibraryPanel tab={activeTab} />
      </div>
    </aside>
  );
}