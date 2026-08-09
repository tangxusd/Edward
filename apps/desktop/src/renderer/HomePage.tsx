import { useEffect, useState } from 'react';
import { createProject, type Project } from '@ai-video/domain';

type Props = {
  onOpenProject: (project: Project) => void;
};

export function HomePage({ onOpenProject }: Props): React.JSX.Element {
  const [projects, setProjects] = useState<Project[]>([]);
  const [showNewDialog, setShowNewDialog] = useState(false);
  const [refreshToken, setRefreshToken] = useState(0);

  useEffect(() => {
    void window.aiVideo.projects.list().then(setProjects);
  }, [refreshToken]);

  const handleCreated = (project: ReturnType<typeof createProject>) => {
    setShowNewDialog(false);
    setRefreshToken((v) => v + 1);
    onOpenProject(project);
  };

  return (
    <div className="home-page">
      <header className="home-topbar">
        <b>Edward</b>
        <span>
          <button className="home-btn" onClick={() => window.dispatchEvent(new Event('settings-request'))}>设置</button>
          <button className="home-btn home-avatar" aria-label="账户" onClick={() => window.dispatchEvent(new Event('account-request'))}>E</button>
        </span>
      </header>
      <div className="home-shell">
        <aside className="home-ad">
          <b className="home-ad-title">广告位</b>
          <small className="home-ad-desc">满高展示区域</small>
          <button className="home-upgrade" onClick={() => window.dispatchEvent(new Event('resource-upgrade-request'))}>资源库升级</button>
        </aside>
        <main className="home-main-content">
          <div className="home-head">
            <div>
              <h3 className="home-head-title">项目</h3>
              <small className="home-head-sub">创建新项目，或继续编辑历史项目</small>
            </div>
            <div className="home-head-actions">
              <button className="home-search">搜索项目</button>
              <button className="home-new-btn" onClick={() => setShowNewDialog(true)}>新建项目</button>
            </div>
          </div>
          <div className="home-cards">
            {projects.map((project) => (
              <article className="home-card" key={project.id} onClick={() => onOpenProject(project)}>
                <div className="home-card-thumb" />
                <div className="home-card-meta">
                  {project.media.path.split(/[/\\]/).at(-1) ?? project.id}
                  <div className="home-card-sub">{project.id}</div>
                </div>
              </article>
            ))}
            <article className="home-card-empty" onClick={() => setShowNewDialog(true)}>
              ＋<br />新建项目
            </article>
          </div>
        </main>
      </div>
      {showNewDialog ? <NewProjectModal onCreated={handleCreated} onClose={() => setShowNewDialog(false)} /> : null}
    </div>
  );
}

function NewProjectModal({ onCreated, onClose }: { onCreated: (project: ReturnType<typeof createProject>) => void; onClose: () => void }): React.JSX.Element {
  const [name, setName] = useState('未命名项目');
  const [mediaPath, setMediaPath] = useState('');
  const [scriptPath, setScriptPath] = useState('');
  const [error, setError] = useState('');

  const chooseMedia = async () => {
    const selected = await window.aiVideo.workspace.chooseFile();
    if (selected) setMediaPath(selected);
  };

  const chooseScript = async () => {
    const selected = await window.aiVideo.workspace.chooseFile();
    if (selected) setScriptPath(selected);
  };

  const create = async () => {
    if (!mediaPath) { setError('请选择音频或视频文件'); return; }
    const mediaKind = /\.(mp4|mov|mkv|webm)$/i.test(mediaPath) ? 'video' : 'audio';
    const project = await window.aiVideo.projects.create(createProject({ scriptPath: scriptPath || undefined, mediaPath, mediaKind, aspectRatio: '16:9' }));
    onCreated(project);
    setError('');
  };

  return (
    <div className="home-modal-overlay" onClick={onClose}>
      <main className="home-modal" onClick={(e) => e.stopPropagation()}>
        <header className="home-modal-header">
          <h2>新建项目</h2>
          <button className="home-modal-close" onClick={onClose}>×</button>
        </header>
        <p className="home-modal-sub">必须导入一条音频或视频主媒体。文案文稿可选；未导入时自动使用本地转写文本。</p>
        <div className="home-modal-row">
          <label className="home-modal-label">项目名称</label>
          <input className="home-modal-input" value={name} onChange={(e) => setName(e.target.value)} />
        </div>
        <div className="home-modal-row">
          <label className="home-modal-label"><span className="home-modal-req">*</span> 音频或视频</label>
          <div className="home-modal-drop">
            <button className="home-modal-sel" onClick={() => void chooseMedia()}>选择文件</button>
            <p className="home-modal-hint">{mediaPath || '支持常见音频与视频格式。每个项目仅允许一条主媒体。'}</p>
          </div>
        </div>
        <div className="home-modal-row">
          <label className="home-modal-label">文案文稿（可选）</label>
          <div className="home-modal-drop">
            <button className="home-modal-sel" onClick={() => void chooseScript()}>选择文件</button>
            <p className="home-modal-hint">{scriptPath || '支持 TXT、MD、DOCX、PDF 等文本格式。'}</p>
          </div>
        </div>
        {error ? <p className="home-modal-error" role="alert">{error}</p> : null}
        <footer className="home-modal-footer">
          <button className="home-modal-cancel" onClick={onClose}>取消</button>
          <button className="home-modal-create" onClick={() => void create()}>创建项目</button>
        </footer>
      </main>
    </div>
  );
}