import { useEffect, useState } from 'react';
import type { AiEditPlan, Project } from '@ai-video/domain';
import type { ModelRecord } from '../main/modelRepository.js';

export function AiAnalysisPanel({ project, onApplied }: { project?: Project; onApplied?: (project: Project) => void }): React.JSX.Element {
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [modelId, setModelId] = useState('');
  const [planText, setPlanText] = useState('');
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  useEffect(() => { void window.aiVideo.models.list().then((records) => { setModels(records); setModelId(records[0]?.id ?? ''); }); }, []);

  const generate = async () => {
    if (!project || !modelId || busy) return;
    setBusy(true); setError(''); setStatus('');
    try {
      const plan = await window.aiVideo.analysis.generate(project.id, modelId);
      setPlanText(JSON.stringify(plan, null, 2));
      setStatus(`已生成 ${plan.clips.length} 个片段`);
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally { setBusy(false); }
  };

  const apply = async () => {
    if (!project || !planText || busy) return;
    setBusy(true); setError('');
    try {
      const plan = JSON.parse(planText) as AiEditPlan;
      const next = await window.aiVideo.analysis.apply(project.id, plan);
      onApplied?.(next);
      setStatus('已应用到时间线');
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally { setBusy(false); }
  };

  return <section aria-label="AI时间线分析"><h2>AI 时间线分析</h2><label>分析模型<select aria-label="分析模型" value={modelId} onChange={(event) => setModelId(event.target.value)} disabled={busy}>{models.length === 0 ? <option value="">暂无模型</option> : models.map((model) => <option key={model.id} value={model.id}>{model.name}</option>)}</select></label><button disabled={!project || !modelId || busy} onClick={() => void generate()}>开始AI分析</button><textarea aria-label="AI编辑计划" value={planText} onChange={(event) => setPlanText(event.target.value)} placeholder="AI 计划将在这里显示" rows={8} /><button disabled={!project || !planText || busy} onClick={() => void apply()}>应用到时间线</button>{status ? <p role="status">{status}</p> : null}{error ? <p role="alert">{error}</p> : null}</section>;
}
