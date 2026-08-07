import { useState } from 'react';

export function ModelSettings(): React.JSX.Element {
  const [name, setName] = useState(''); const [baseUrl, setBaseUrl] = useState(''); const [modelId, setModelId] = useState(''); const [credentialRef, setCredentialRef] = useState('');
  return <section aria-label="模型设置"><h2>云端模型</h2><label>名称<input value={name} onChange={(e) => setName(e.target.value)} /></label><label>API 地址<input value={baseUrl} onChange={(e) => setBaseUrl(e.target.value)} /></label><label>模型<input value={modelId} onChange={(e) => setModelId(e.target.value)} /></label><label>凭证引用<input value={credentialRef} onChange={(e) => setCredentialRef(e.target.value)} /></label><button disabled={!name || !baseUrl || !modelId || !credentialRef}>保存模型</button></section>;
}
