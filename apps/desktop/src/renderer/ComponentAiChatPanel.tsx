import { useEffect, useState } from 'react';
import type { Project, TimelineClip } from '@ai-video/domain';
import type { ModelRecord } from '../main/modelRepository.js';
import { describeComponentChanges, mergeComponentContent } from './componentContentMerge.js';

type Message = { role: 'user' | 'assistant'; content: string };

export function ComponentAiChatPanel({ project, clip, onProjectChange, onApply }: { project: Project; clip: TimelineClip; onProjectChange: (project: Project) => void; onApply: (content: unknown) => void }): React.JSX.Element {
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [modelId, setModelId] = useState('');
  const [message, setMessage] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const conversation = project.componentConversations?.[clip.id];
  let mergedDraft: unknown;
  let draftError = '';
  if (conversation) {
    try { mergedDraft = mergeComponentContent(clip.content, conversation.draftContent); }
    catch (cause) { draftError = cause instanceof Error ? cause.message : String(cause); }
  }
  const changes = conversation && !draftError ? describeComponentChanges(clip.content, mergedDraft) : [];
  useEffect(() => { void window.aiVideo.models.list().then((next) => { setModels(next); setModelId((current) => current || next[0]?.id || ''); }); }, []);
  const send = async () => {
    if (!message.trim() || !modelId || busy) return;
    setBusy(true); setError('');
    try {
      const result = await window.aiVideo.componentChat.send(project.id, clip.id, modelId, message.trim(), conversation?.messages ?? []);
      const messages: Message[] = [...(conversation?.messages ?? []), { role: 'user', content: message.trim() }, { role: 'assistant', content: result.reply }];
      onProjectChange({ ...project, componentConversations: { ...project.componentConversations, [clip.id]: { modelId, messages, draftContent: result.draftContent, updatedAt: new Date().toISOString() } } });
      setMessage('');
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  return <section aria-label="AI修改组件"><h2>AI 修改组件</h2><label>对话模型<select aria-label="组件对话模型" value={modelId} onChange={(event) => setModelId(event.target.value)} disabled={busy}>{models.length ? models.map((model) => <option key={model.id} value={model.id}>{model.name}</option>) : <option value="">暂无模型</option>}</select></label><div aria-label="组件AI对话记录">{conversation?.messages.map((item, index) => <p key={`${item.role}-${index}`}><strong>{item.role === 'user' ? '你' : 'AI'}</strong>{item.content}</p>)}</div><label>修改指令<textarea aria-label="组件修改指令" value={message} onChange={(event) => setMessage(event.target.value)} /></label><button disabled={!message.trim() || !modelId || busy} onClick={() => void send()}>发送给AI</button>{conversation ? <p aria-label="组件草案状态">{draftError ? `草案格式不兼容：${draftError}` : '草案已通过格式校验'}</p> : null}{changes.length ? <ul aria-label="组件草案变更">{changes.map((change) => <li key={change}>{change}</li>)}</ul> : null}{conversation ? <button disabled={busy || !!draftError} onClick={() => onApply(mergedDraft)}>应用修改</button> : null}{error ? <p role="alert">{error}</p> : null}</section>;
}
