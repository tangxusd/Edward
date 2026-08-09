import { useState } from 'react';
import { createProject } from '@ai-video/domain';
import { ProjectList } from './ProjectList.js';
import { NewProjectDialog } from './NewProjectDialog.js';

type Props = {
  onOpenProject: (project: ReturnType<typeof createProject>) => void;
};

export function HomePage({ onOpenProject }: Props): React.JSX.Element {
  const [refreshToken, setRefreshToken] = useState(0);

  const created = (project: ReturnType<typeof createProject>) => {
    setRefreshToken((v) => v + 1);
    onOpenProject(project);
  };

  return (
    <div className="home-page">
      <aside className="home-ad-column">
        <div className="home-ad-placeholder">广告位</div>
        <button type="button" className="resource-upgrade-btn" onClick={() => window.dispatchEvent(new Event('resource-upgrade-request'))}>
          资源库升级
        </button>
      </aside>
      <main className="home-main">
        <header className="home-header">
          <h1>Edward</h1>
          <span className="home-version">0.1.1</span>
          <div className="home-header-right">
            <button type="button" className="settings-trigger" onClick={() => window.dispatchEvent(new Event('settings-request'))}>
              设置
            </button>
            <button type="button" className="account-trigger" aria-label="账户" onClick={() => window.dispatchEvent(new Event('account-request'))}>
              <span className="account-avatar" />
            </button>
          </div>
        </header>
        <div className="home-content">
          <ProjectList onOpen={onOpenProject} refreshToken={refreshToken} />
          <NewProjectDialog onCreated={created} />
        </div>
      </main>
    </div>
  );
}