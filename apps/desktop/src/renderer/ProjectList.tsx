import { useEffect, useState } from 'react';
import type { Project } from '@ai-video/domain';

export function ProjectList({ onOpen, refreshToken = 0 }: { onOpen: (project: Project) => void; refreshToken?: number }): React.JSX.Element {
  const [projects, setProjects] = useState<Project[]>([]);
  useEffect(() => { void window.aiVideo.projects.list().then(setProjects); }, [refreshToken]);
  return <section aria-label="项目列表"><h2>最近项目</h2>{projects.length === 0 ? <p>暂无项目</p> : projects.map((project) => <article key={project.id}><strong>{project.id}</strong><small>{project.media.path}</small><button onClick={() => void window.aiVideo.projects.open(project.id).then(onOpen)}>打开</button></article>)}</section>;
}
