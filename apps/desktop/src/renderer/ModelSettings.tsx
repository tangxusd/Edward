import { useEffect, useState } from 'react';
import type { ModelRecord } from '../main/modelRepository.js';

export function ModelSettings(): React.JSX.Element {
  const [name, setName] = useState(''); const [baseUrl, setBaseUrl] = useState(''); const [modelId, setModelId] = useState(''); const [credentialRef, setCredentialRef] = useState(''); const [credentialValue, setCredentialValue] = useState('');
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [error, setError] = useState('');
  useEffect(() => { void window.aiVideo.models.list().then(setModels); }, []);
  const save = async () => {
    setError('');
    try {
      const saved = await window.aiVideo.models.save({ id: `model-${crypto.randomUUID()}`, name, baseUrl, modelId, credentialRef }, credentialValue);
      setModels((current) => [...current.filter((item) => item.id !== saved.id), saved]);
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    }
  };
  return <section aria-label="模型设置"><h2>云端模型</h2><label>名称<input value={name} onChange={(e) => setName(e.target.value)} /></label><label>API 地址<input value={baseUrl} onChange={(e) => setBaseUrl(e.target.value)} /></label><label>模型<input value={modelId} onChange={(e) => setModelId(e.target.value)} /></label><label>凭证引用<input value={credentialRef} onChange={(e) => setCredentialRef(e.target.value)} /></label><label>模型凭证<input type="password" value={credentialValue} onChange={(e) => setCredentialValue(e.target.value)} /></label><button disabled={!name || !baseUrl || !modelId || !credentialRef || !credentialValue} onClick={() => void save()}>保存模型</button>{error ? <p role="alert">{error}</p> : null}<ul aria-label="已保存模型">{models.map((model) => <li key={model.id}>{model.name}<small>{model.modelId}</small></li>)}</ul></section>;
}
