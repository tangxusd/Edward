import { useEffect, useState } from 'react';
import type { ModelRecord } from '../main/modelRepository.js';

type Message = { role: 'user' | 'assistant'; content: string };

/**
 * GlobalAiCreate — 全高 AI 创作面板
 *
 * 当没有选中任何组件时展示，提供完整的 AI 对话界面。
 * 视觉规格参考 global-ai-create-v2-fixed-buttons.html
 */
export function GlobalAiCreate(): React.JSX.Element {
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [modelId, setModelId] = useState('');
  const [message, setMessage] = useState('');
  const [messages, setMessages] = useState<Message[]>([]);
  const [busy, setBusy] = useState(false);
  const [modelOpen, setModelOpen] = useState(false);

  /* 加载模型列表 */
  useEffect(() => {
    let disposed = false;
    void window.aiVideo.models.list().then((records) => {
      if (!disposed) {
        setModels(records);
        setModelId((current) => current || records[0]?.id || '');
      }
    });
    return () => { disposed = true; };
  }, []);

  const selectedModel = models.find((m) => m.id === modelId);

  const send = async () => {
    if (!message.trim() || !modelId || busy) return;
    const userMsg: Message = { role: 'user', content: message.trim() };
    setMessages((prev) => [...prev, userMsg]);
    setMessage('');
    setBusy(true);
    /* 模拟 AI 回复 — 后续接入真实 API */
    await new Promise((resolve) => setTimeout(resolve, 600));
    const botMsg: Message = { role: 'assistant', content: '收到您的请求。AI 创作功能正在开发中，请稍候。' };
    setMessages((prev) => [...prev, botMsg]);
    setBusy(false);
  };

  return (
    <section className="global-ai-create" aria-label="全局AI创作">
      {/* 头部：标题 + 模型选择器 */}
      <header className="ga-header">
        <b>AI 创作</b>
        <div className="ga-model-wrap">
          <button
            type="button"
            className="ga-model-btn"
            onClick={() => setModelOpen((o) => !o)}
            aria-haspopup="listbox"
            aria-expanded={modelOpen}
          >
            {selectedModel?.name ?? '选择模型'} ▾
          </button>
          {modelOpen && (
            <ul className="ga-model-dropdown" role="listbox" aria-label="AI模型">
              {models.length === 0 && (
                <li className="ga-model-empty" role="option">暂无模型</li>
              )}
              {models.map((m) => (
                <li
                  key={m.id}
                  role="option"
                  aria-selected={m.id === modelId}
                  className={`ga-model-option${m.id === modelId ? ' active' : ''}`}
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
      <div className="ga-history" aria-label="AI对话记录">
        {messages.length === 0 && (
          <p className="ga-empty-hint">开始一段 AI 创作对话</p>
        )}
        {messages.map((msg, i) => (
          <div
            key={i}
            className={`ga-msg ${msg.role === 'user' ? 'ga-msg-user' : 'ga-msg-bot'}`}
          >
            {msg.content}
          </div>
        ))}
        {busy && <div className="ga-msg ga-msg-bot ga-msg-typing">AI 正在思考…</div>}
      </div>

      {/* 输入区 */}
      <div className="ga-input-area">
        <button type="button" className="ga-add-btn" aria-label="添加附件">＋</button>
        <textarea
          className="ga-field"
          placeholder="输入对项目的要求…"
          value={message}
          onChange={(e) => setMessage(e.target.value)}
          onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); void send(); } }}
          disabled={busy}
          rows={2}
        />
        <button
          type="button"
          className="ga-send-btn"
          disabled={!message.trim() || !modelId || busy}
          onClick={() => void send()}
        >
          发送
        </button>
      </div>

      <p className="ga-note">+ 导入文件 · 拖拽上边缘调整输入框高度</p>
    </section>
  );
}