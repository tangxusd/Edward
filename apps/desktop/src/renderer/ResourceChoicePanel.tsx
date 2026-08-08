import type { Resource } from '@ai-video/domain';
import { toLocalFileUrl } from './fileUrl.js';

export function ResourceChoicePanel({ label, resources, onSelect }: { label: string; resources: Resource[]; onSelect: (resourceId: string) => void }): React.JSX.Element {
  return <div aria-label={`${label}资源`} className="resource-choice-panel">{resources.length === 0 ? <p>暂无同类资源</p> : resources.map((resource) => <button key={resource.id} type="button" aria-label={`使用${label}资源 ${resource.name}`} onClick={() => onSelect(resource.id)}>{resource.thumbnailPath ? <img src={toLocalFileUrl(resource.thumbnailPath)} alt="" /> : <span aria-hidden="true" />}{resource.name}</button>)}</div>;
}
