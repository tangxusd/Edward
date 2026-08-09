type Props = {
  projectName?: string;
  saveStatus?: 'saved' | 'saving' | 'failed';
  modelStatus?: string;
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

export function TopBar({
  projectName = '未命名项目',
  saveStatus = 'saved',
  modelStatus = 'GPT-4.1 · 在线',
  modelColor = '#28d69e',
  onExport,
  onSettings,
}: Props): React.JSX.Element {
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
        <span className="topbar-model-status">
          <span className="topbar-model-dot" style={{ background: modelColor }} />
          {modelStatus}
        </span>
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