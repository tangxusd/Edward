import { useEffect, useState } from 'react';
import type { Project, TimelineClip } from '@ai-video/domain';
import type { ModelRecord } from '../main/modelRepository.js';
import { describeComponentChanges, mergeComponentContent } from './componentContentMerge.js';

type Message = { role: 'user' | 'assistant'; content: string };

/**
 * ComponentAiChatPanel — 组件级 AI 对话面板
 *
 * 匹配视觉确认稿 workbench-right-panel-v5.html 中的 AI 区设计。
 * 固定在右栏下半部，使用 .rp-ai-* CSS 类。
 */
export function ComponentAiChatPanel({
  project,
  clip,
  onProjectChange,
  onApply,
}: {
  project: Project;
  clip: TimelineClip;
  onProjectChange: (project: Project) => void;
  onApply: (content: unknown) => void;
}): React.JSX.Element {
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [modelId, setModelId] = useState('');
  const [message, setMessage] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [modelOpen, setModelOpen] = useState(false);

  const conversation = project.componentConversations?.[clip.id];
  let mergedDraft: unknown;
  let draftError = '';
  if (conversation) {
    try { mergedDraft = mergeComponentContent(clip.content, conversation.draftContent); }
    catch (cause) { draftError = cause instanceof Error ? cause.message : String(cause); }
  }
  const changes = conversation && !draftError ? describeComponentChanges(clip.content, mergedDraft) : [];

  useEffect(() => {
    void window.aiVideo.models.list().then((next) => {
      setModels(next);
      setModelId((current) => current || conversation?.modelId || next[0]?.id || '');
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const send = async () => {
    if (!message.trim() || !modelId || busy) return;
    setBusy(true);
    setError('');
    try {
      const result = await window.aiVideo.componentChat.send(
        project.id, clip.id, modelId, message.trim(),
        conversation?.messages ?? [],
      );
      const messages: Message[] = [
        ...(conversation?.messages ?? []),
        { role: 'user', content: message.trim() },
        { role: 'assistant', content: result.reply },
      ];
      onProjectChange({
        ...project,
        componentConversations: {
          ...project.componentConversations,
          [clip.id]: {
            modelId,
            messages,
            draftContent: result.draftContent,
            updatedAt: new Date().toISOString(),
          },
        },
      });
      setMessage('');
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setBusy(false);
    }
  };

  return (
    <section className="right-panel-ai" aria-label="AI修改组件">
      {/* 头部：标题 + 模型选择器 */}
      <header className="rp-ai-header">
        <b>AI 对话 · {clip.styleId || '组件'}</b>
        <div className="rp-ai-model-wrap">
          <button
            type="button"
            className="rp-ai-model-selector"
            onClick={() => setModelOpen((o) => !o)}
            aria-haspopup="listbox"
            aria-expanded={modelOpen}
          >
            {models.find((m) => m.id === modelId)?.name ?? '选择模型'} ▾
          </button>
          {modelOpen && (
            <ul className="rp-ai-model-dropdown" role="listbox" aria-label="组件对话模型">
              {models.length === 0 && (
                <li className="rp-ai-model-empty" role="option">暂无模型</li>
              )}
              {models.map((m) => (
                <li
                  key={m.id}
                  role="option"
                  aria-selected={m.id === modelId}
                  className={`rp-ai-model-option${m.id === modelId ? ' active' : ''}`}
                  onClick={() => { setModelId(m.id); setModelOpen(false); }}
                >
                  {m.name}
                </li>
              ))}
            </ul>
          )}
        </div>
      </header>

      {/* 对话历史 */}
      <div className="rp-ai-history" aria-label="组件AI对话记录">
        {conversation?.messages.map((item, index) => (
          <div
            key={`${item.role}-${index}`}
            className={`rp-ai-msg ${item.role === 'user' ? 'rp-ai-msg-user' : 'rp-ai-msg-bot'}`}
          >
            {item.content}
          </div>
        ))}
        {busy && <div className="rp-ai-msg rp-ai-msg-bot rp-ai-msg-typing">AI 正在修改…</div>}
      </div>

      {/* 输入区 */}
      <div className="rp-ai-input">
        <textarea
          className="rp-ai-field"
          placeholder="输入对组件的修改要求…"
          value={message}
          onChange={(e) => setMessage(e.target.value)}
          onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); void send(); } }}
          disabled={busy}
          rows={1}
        />
        <button
          type="button"
          className="rp-ai-send"
          disabled={!message.trim() || !modelId || busy}
          onClick={() => void send()}
        >
          发送
        </button>
      </div>

      {/* 状态提示 */}
      {conversation && (
        <p className="rp-ai-note" aria-label="组件草案状态">
          {draftError
            ? `草案格式不兼容：${draftError}`
            : '草案已通过格式校验'}
        </p>
      )}

      {/* 变更列表 */}
      {changes.length > 0 && (
        <ul className="rp-ai-changes" aria-label="组件草案变更">
          {changes.map((change) => (
            <li key={change} className="rp-ai-change-item">{change}</li>
          ))}
        </ul>
      )}

      {/* 操作按钮 */}
      {conversation && (
        <div className="rp-ai-actions">
          <button
            type="button"
            className="rp-ai-apply-btn"
            disabled={busy || !!draftError}
            onClick={() => onApply(mergedDraft)}
          >
            应用修改
          </button>
        </div>
      )}

      {/* 错误提示 */}
      {error && <p className="rp-ai-error" role="alert">{error}</p>}
    </section>
  );
}