import { useEffect, useState } from 'react';
import type { ModelRecord } from '../main/modelRepository.js';

type ModelFormState = {
  id?: string;
  name: string;
  baseUrl: string;
  modelId: string;
  credentialRef: string;
  credentialValue: string;
};

const emptyForm: ModelFormState = { name: '', baseUrl: '', modelId: '', credentialRef: '', credentialValue: '' };

function isFormValid(f: ModelFormState): boolean {
  return !!(f.name && f.baseUrl && f.modelId && f.credentialRef && f.credentialValue);
}

export function ModelSettings({ onModelsChanged }: { onModelsChanged?: (models: ModelRecord[]) => void }): React.JSX.Element {
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [error, setError] = useState('');
  const [showForm, setShowForm] = useState(false);
  const [form, setForm] = useState<ModelFormState>(emptyForm);
  const [activeModelId, setActiveModelId] = useState<string | null>(null);

  useEffect(() => {
    let disposed = false;
    const reload = () => {
      void window.aiVideo.models.list().then((records) => {
        if (!disposed) setModels(records);
      });
    };
    reload();
    window.addEventListener('workspace-changed', reload);
    return () => { disposed = true; window.removeEventListener('workspace-changed', reload); };
  }, []);

  const save = async () => {
    setError('');
    try {
      const saved = await window.aiVideo.models.save(
        { id: form.id ?? `model-${crypto.randomUUID()}`, name: form.name, baseUrl: form.baseUrl, modelId: form.modelId, credentialRef: form.credentialRef },
        form.credentialValue,
      );
      setModels((current) => {
        const next = [saved, ...current.filter((item) => item.id !== saved.id)];
        onModelsChanged?.(next);
        return next;
      });
      setShowForm(false);
      setForm(emptyForm);
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    }
  };

  const startAdd = () => {
    setForm(emptyForm);
    setError('');
    setShowForm(true);
  };

  const cancelForm = () => {
    setShowForm(false);
    setForm(emptyForm);
    setError('');
  };

  const setActive = (id: string) => {
    setActiveModelId(id);
  };

  return (
    <section aria-label="模型设置">
      <div className="model-settings-head">
        <div>
          <div className="model-settings-head-title">大模型列表</div>
          <div className="model-settings-head-desc">添加 OpenAI 兼容接口模型，凭证仅保存在本机。</div>
        </div>
        <button type="button" className="model-settings-add" onClick={startAdd}>添加大模型</button>
      </div>

      {showForm ? (
        <div className="model-settings-form">
          <label>
            名称
            <input value={form.name} onChange={(e) => setForm({ ...form, name: e.target.value })} placeholder="例如：主力模型" />
          </label>
          <label>
            API 地址
            <input value={form.baseUrl} onChange={(e) => setForm({ ...form, baseUrl: e.target.value })} placeholder="https://api.openai.com/v1" />
          </label>
          <label>
            模型
            <input value={form.modelId} onChange={(e) => setForm({ ...form, modelId: e.target.value })} placeholder="gpt-4.1" />
          </label>
          <label>
            凭证引用
            <input value={form.credentialRef} onChange={(e) => setForm({ ...form, credentialRef: e.target.value })} placeholder="例如：OPENAI_API_KEY" />
          </label>
          <label>
            模型凭证
            <input type="password" value={form.credentialValue} onChange={(e) => setForm({ ...form, credentialValue: e.target.value })} placeholder="sk-..." />
          </label>
          {error ? <p className="model-settings-error" role="alert">{error}</p> : null}
          <div className="model-settings-form-actions">
            <button type="button" className="model-settings-cancel-btn" onClick={cancelForm}>取消</button>
            <button type="button" className="model-settings-save-btn" disabled={!isFormValid(form)} onClick={() => void save()}>保存模型</button>
          </div>
        </div>
      ) : null}

      <div className="model-settings-table">
        <div className="model-settings-row model-settings-header">
          <span>名称</span>
          <span>模型标识</span>
          <span>状态</span>
          <span>操作</span>
        </div>
        {models.length === 0 ? (
          <div className="model-settings-row">
            <span className="model-settings-empty" style={{ gridColumn: '1 / -1' }}>暂无已配置模型</span>
          </div>
        ) : (
          models.map((model) => (
            <div className="model-settings-row" key={model.id}>
              <span>{model.name}</span>
              <span>{model.modelId}</span>
              <span className={activeModelId === model.id ? 'model-settings-pill' : 'model-settings-pill-offline'}>
                {activeModelId === model.id ? '在线' : '未测试'}
              </span>
              <span className="model-settings-actions">
                <button
                  type="button"
                  className={`model-settings-btn${activeModelId === model.id ? ' active' : ''}`}
                  onClick={() => setActive(model.id)}
                >
                  {activeModelId === model.id ? '使用中' : '使用'}
                </button>
                <button type="button" className="model-settings-btn">测试</button>
                <button type="button" className="model-settings-btn">编辑</button>
                <button type="button" className="model-settings-btn">删除</button>
              </span>
            </div>
          ))
        )}
      </div>

      <p className="model-settings-note">“使用”设为全局默认模型；工作台中的组件 AI 对话仍可单独选择已配置模型。</p>
    </section>
  );
}