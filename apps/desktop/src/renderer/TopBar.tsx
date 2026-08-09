import { useState } from 'react';

export type ModelStatusItem = {
  name: string;
  online: boolean;
};

type Props = {
  projectName?: string;
  saveStatus?: 'saved' | 'saving' | 'failed';
  models?: ModelStatusItem[];
  /** @deprecated use models instead */
  modelStatus?: string;
  /** @deprecated use models instead */
  modelColor?: string;
  onExport?: () => void;
  onSettings?: () => void;
};

const saveStatusLabel: Record<string, string> = {
  saved: '已保存',
  saving: '正在保存',
  failed: '保存失败',
};

const saveStatusColor: Record<string, string> = {
  saved: '#24d39f',
  saving: '#d4a72c',
  failed: '#d14343',
};

const onlineColor = '#43d18d';
const offlineColor = '#cc5a5a';

export function TopBar({
  projectName = '未命名项目',
  saveStatus = 'saved',
  models,
  modelStatus,
  modelColor,
  onExport,
  onSettings,
}: Props): React.JSX.Element {
  const [menuOpen, setMenuOpen] = useState(false);

  // Resolve model list: use `models` array if provided, else fall back to legacy single-model props
  const resolvedModels: ModelStatusItem[] = models && models.length > 0
    ? models
    : modelStatus
      ? [{ name: modelStatus, online: modelColor === '#28d69e' || modelColor === '#24b47e' }]
      : [];

  const primary = resolvedModels[0];

  return (
    <header className="workbench-topbar">
      <div className="workbench-topbar-left">
        <b>Edward</b>
        <span className="topbar-save-status" style={{ color: saveStatusColor[saveStatus] }}>
          {saveStatusLabel[saveStatus]}
        </span>
      </div>
      <div className="workbench-topbar-center" title={projectName}>
        <span className="topbar-project-name">{projectName}</span>
      </div>
      <div className="workbench-topbar-right">
        {/* 模型状态下拉菜单 */}
        <div className="topbar-model-wrap">
          <button
            className="topbar-model-btn"
            onClick={() => setMenuOpen(!menuOpen)}
            aria-label="模型状态"
          >
            {primary ? (
              <>
                <span className="topbar-model-dot" style={{ background: primary.online ? onlineColor : offlineColor }} />
                {primary.name}　{primary.online ? '在线' : '离线'}
              </>
            ) : (
              '未配置模型'
            )}
            ▾
          </button>
          {menuOpen && resolvedModels.length > 1 ? (
            <div className="topbar-model-dropdown">
              {resolvedModels.map((m) => (
                <div key={m.name} className="topbar-model-option">
                  <span className="topbar-model-dot" style={{ background: m.online ? onlineColor : offlineColor }} />
                  {m.name}　{m.online ? '在线' : '离线'}
                </div>
              ))}
            </div>
          ) : null}
        </div>

        <button className="topbar-export-btn" onClick={onExport}>
          导出
        </button>
        <button className="topbar-settings-btn" onClick={onSettings} aria-label="设置">
          设置
        </button>
      </div>
    </header>
  );
}
