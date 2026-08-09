import { useEffect, useState } from 'react';

export function WorkspaceSettings({ onChanged }: { onChanged?: () => void }): React.JSX.Element {
  const [projectPath, setProjectPath] = useState('');
  const [resourcePath, setResourcePath] = useState('');
  const [previewQuality, setPreviewQuality] = useState<'lossless' | 'clear' | 'smooth'>('lossless');
  const [proxyPath, setProxyPath] = useState('');
  const [cachePath, setCachePath] = useState('');
  const [renderPath, setRenderPath] = useState('');

  useEffect(() => {
    void window.aiVideo.workspace.getRoot().then((r) => {
      setProjectPath(r);
      setResourcePath(r);
      setProxyPath(r);
      setCachePath(r);
      setRenderPath(r);
    });
  }, []);

  const chooseDir = async (setter: (p: string) => void) => {
    const dir = await window.aiVideo.workspace.chooseDirectory();
    if (dir) setter(dir);
  };

  const save = async () => {
    await window.aiVideo.workspace.setRoot(projectPath);
    onChanged?.();
  };

  return (
    <section aria-label="项目设置">
      <div className="settings-row">
        <div>
          项目默认位置
          <div className="settings-row-desc">新项目在此位置新建文件夹</div>
        </div>
        <div>
          <span className="settings-path">{projectPath}</span>
          <button className="settings-btn" onClick={() => void chooseDir(setProjectPath)}>选择</button>
        </div>
      </div>

      <div className="settings-row">
        <div>
          资源位置
          <div className="settings-row-desc">下载资源库的保存位置</div>
        </div>
        <div>
          <span className="settings-path">{resourcePath}</span>
          <button className="settings-btn" onClick={() => void chooseDir(setResourcePath)}>选择</button>
          <button className="settings-btn settings-btn-upgrade">资源库升级</button>
        </div>
      </div>

      <div className="settings-row">
        <div>
          预览设置
          <div className="settings-row-desc">清晰/流畅将生成代理，导出仍使用原画或指定画质</div>
        </div>
        <div className="settings-radio-group">
          <button
            className={`settings-radio${previewQuality === 'lossless' ? ' settings-radio-sel' : ''}`}
            onClick={() => setPreviewQuality('lossless')}
          >
            原画
          </button>
          <button
            className={`settings-radio${previewQuality === 'clear' ? ' settings-radio-sel' : ''}`}
            onClick={() => setPreviewQuality('clear')}
          >
            清晰
          </button>
          <button
            className={`settings-radio${previewQuality === 'smooth' ? ' settings-radio-sel' : ''}`}
            onClick={() => setPreviewQuality('smooth')}
          >
            流畅
          </button>
        </div>
      </div>

      <div className="settings-row">
        <div>
          代理位置
          <div className="settings-row-desc">仅清晰或流畅预览时使用</div>
        </div>
        <div>
          <span className="settings-path">{proxyPath}</span>
          <button className="settings-btn" onClick={() => void chooseDir(setProxyPath)}>选择</button>
        </div>
      </div>

      <div className="settings-row">
        <div>缓存位置</div>
        <div>
          <span className="settings-path">{cachePath}</span>
          <button className="settings-btn" onClick={() => void chooseDir(setCachePath)}>选择</button>
        </div>
      </div>

      <div className="settings-row">
        <div>
          渲染位置
          <div className="settings-row-desc">临时渲染文件保存位置</div>
        </div>
        <div>
          <span className="settings-path">{renderPath}</span>
          <button className="settings-btn" onClick={() => void chooseDir(setRenderPath)}>选择</button>
        </div>
      </div>

      <footer className="settings-footer">
        <button className="settings-save-btn" onClick={() => void save()}>保存设置</button>
      </footer>
    </section>
  );
}