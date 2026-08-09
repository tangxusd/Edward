import { useEffect, useMemo, useRef, useState } from 'react';
import type { AiEditPlan, AiPlanApplicationMode, AiPlanClip, Project } from '@ai-video/domain';
import type { ModelRecord } from '../main/modelRepository.js';

/* ─── helpers ─────────────────────────────────────────────────────────── */

function fmtTime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

function trackLabel(track: AiPlanClip['track']): string {
  return { subtitles: '字幕', cards: '卡片', graphics: '图表' }[track] ?? track;
}

function clipContentPreview(clip: AiPlanClip): string {
  if (typeof clip.content === 'string') return clip.content;
  if (clip.content && typeof clip.content === 'object') {
    const obj = clip.content as Record<string, unknown>;
    if (typeof obj.text === 'string') return obj.text;
    if (typeof obj.label === 'string') return obj.label;
    if (typeof obj.title === 'string') return obj.title;
    return JSON.stringify(obj).slice(0, 40);
  }
  return '—';
}

/* ─── progress step data ──────────────────────────────────────────────── */

interface ProgressStep {
  index: number;
  label: string;
  bar?: boolean;
}

const GENERATION_STEPS: ProgressStep[] = [
  { index: 1, label: '读取媒体信息' },
  { index: 2, label: '本地转写并对齐字幕', bar: true },
  { index: 3, label: '调用 GPT-4.1 进行语义分析' },
  { index: 4, label: '生成多轨时间线' },
];

/* ─── component ───────────────────────────────────────────────────────── */

interface GenerationProgress {
  step: number;       // 0-based index into GENERATION_STEPS
  percent: number;    // 0-100 for the current step
}

export function AiAnalysisPanel({
  project,
  onApplied,
}: {
  project?: Project;
  onApplied?: (project: Project) => void;
}): React.JSX.Element {
  /* model list */
  const [models, setModels] = useState<ModelRecord[]>([]);
  const [modelId, setModelId] = useState('');

  /* plan state */
  const [plan, setPlan] = useState<AiEditPlan | null>(null);
  const [applicationMode, setApplicationMode] = useState<AiPlanApplicationMode>('unmodified-only');

  /* saved plans */
  const [savedPlans, setSavedPlans] = useState<Array<{ id: string; plan: AiEditPlan }>>([]);

  /* status / error / busy */
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  /* generation progress display */
  const [genProgress, setGenProgress] = useState<GenerationProgress | null>(null);

  /* for simulated progress */
  const progressTimer = useRef<ReturnType<typeof setInterval> | null>(null);

  /* ---------- load models ---------- */
  useEffect(() => {
    let disposed = false;
    const reload = () => {
      void window.aiVideo.models.list().then((records) => {
        if (!disposed) {
          setModels(records);
          setModelId(records[0]?.id ?? '');
        }
      });
    };
    reload();
    window.addEventListener('workspace-changed', reload);
    return () => {
      disposed = true;
      window.removeEventListener('workspace-changed', reload);
    };
  }, []);

  /* ---------- load saved plans ---------- */
  const reloadSavedPlans = async () => {
    if (!project) { setSavedPlans([]); return; }
    setSavedPlans(await window.aiVideo.analysis.list(project.id));
  };
  useEffect(() => { void reloadSavedPlans(); }, [project?.id]);

  /* ---------- simulate progress ---------- */
  const startProgressSimulation = () => {
    stopProgressSimulation();
    setGenProgress({ step: 0, percent: 0 });
    let step = 0;
    let pct = 0;
    progressTimer.current = setInterval(() => {
      pct += Math.random() * 8 + 2;
      if (pct >= 100) {
        pct = 0;
        step += 1;
        if (step >= GENERATION_STEPS.length) {
          stopProgressSimulation();
          return;
        }
      }
      setGenProgress({ step, percent: Math.min(pct, 100) });
    }, 600);
  };

  const stopProgressSimulation = () => {
    if (progressTimer.current) {
      clearInterval(progressTimer.current);
      progressTimer.current = null;
    }
  };

  /* ---------- cleanup on unmount ---------- */
  useEffect(() => () => stopProgressSimulation(), []);

  /* ---------- generate ---------- */
  const generate = async () => {
    if (!project || !modelId || busy) return;
    setBusy(true);
    setError('');
    setStatus('');
    setPlan(null);
    startProgressSimulation();
    try {
      const result = await window.aiVideo.analysis.generate(project.id, modelId);
      stopProgressSimulation();
      setGenProgress(null);
      setPlan(result);
      setStatus(`已生成 ${result.clips.length} 个片段`);
      await reloadSavedPlans();
    } catch (cause) {
      stopProgressSimulation();
      setGenProgress(null);
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setBusy(false);
    }
  };

  /* ---------- apply ---------- */
  const apply = async () => {
    if (!project || !plan || busy) return;
    setBusy(true);
    setError('');
    try {
      const next = await window.aiVideo.analysis.apply(project.id, plan, applicationMode);
      onApplied?.(next);
      setStatus('已应用到时间线');
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setBusy(false);
    }
  };

  const cancel = () => {
    setPlan(null);
    setGenProgress(null);
    setStatus('');
    setError('');
  };

  /* ---------- derived data ---------- */
  const summary = useMemo(() => {
    if (!plan) return null;
    const clips = plan.clips;
    return {
      subtitles: clips.filter((c) => c.track === 'subtitles').length,
      cards: clips.filter((c) => c.track === 'cards').length,
      graphics: clips.filter((c) => c.track === 'graphics').length,
    };
  }, [plan]);

  /* ---------- render ---------- */
  return (
    <section aria-label="AI时间线分析" className="ai-panel">
      {/* ── header ── */}
      <div className="ai-panel-header">
        <h2>
          {genProgress
            ? '正在生成剪辑时间线'
            : plan
              ? 'AI 时间线方案已生成'
              : 'AI 时间线分析'}
        </h2>
        {(plan || genProgress) && (
          <button className="ai-panel-close" onClick={cancel} aria-label="关闭" disabled={busy}>
            ×
          </button>
        )}
      </div>

      {/* ── model selector (idle) ── */}
      {!plan && !genProgress && (
        <>
          <div className="ai-panel-model-row">
            <label className="ai-panel-label-sm">分析模型</label>
            <select
              aria-label="分析模型"
              value={modelId}
              onChange={(e) => setModelId(e.target.value)}
              disabled={busy}
            >
              {models.length === 0 ? (
                <option value="">暂无模型</option>
              ) : (
                models.map((m) => (
                  <option key={m.id} value={m.id}>
                    {m.name}
                  </option>
                ))
              )}
            </select>
            <button
              className="ai-panel-gen-btn"
              disabled={!project || !modelId || busy}
              onClick={() => void generate()}
            >
              开始AI分析
            </button>
          </div>

          {/* saved plans */}
          <div className="ai-panel-saved-row">
            <label className="ai-panel-label-sm">已保存AI计划</label>
            <select
              aria-label="已保存AI计划"
              value=""
              disabled={!savedPlans.length || busy}
              onChange={(e) => {
                const sel = savedPlans.find((s) => s.id === e.target.value);
                if (sel) setPlan(sel.plan);
              }}
            >
              <option value="">
                {savedPlans.length ? '选择历史计划' : '暂无历史计划'}
              </option>
              {savedPlans.map((s) => (
                <option key={s.id} value={s.id}>
                  {s.plan.summary}
                </option>
              ))}
            </select>
          </div>
        </>
      )}

      {/* ── generation progress ── */}
      {genProgress && (
        <div className="ai-panel-progress">
          <p className="ai-panel-sub">
            项目已创建。原始媒体仅在本地处理；云端模型只接收文本与时间戳。
          </p>
          {GENERATION_STEPS.map((step, idx) => {
            const isDone = idx < genProgress.step;
            const isCurrent = idx === genProgress.step;
            const dotClass =
              isDone
                ? 'ai-step-dot done'
                : isCurrent
                  ? 'ai-step-dot running'
                  : 'ai-step-dot';
            const dotContent = isDone ? '✓' : String(step.index);
            const stateText = isDone
              ? '已完成'
              : isCurrent
                ? `${Math.round(genProgress.percent)}%`
                : '等待中';
            return (
              <div className="ai-step" key={step.index}>
                <i className={dotClass}>{dotContent}</i>
                <div>
                  <span>{step.label}</span>
                  {step.bar && isCurrent && (
                    <div className="ai-step-bar">
                      <i style={{ width: `${Math.round(genProgress.percent)}%` }} />
                    </div>
                  )}
                </div>
                <span className="ai-step-state">{stateText}</span>
              </div>
            );
          })}
          <button
            className="ai-panel-cancel-btn"
            onClick={cancel}
            disabled={busy}
          >
            取消生成
          </button>
          <p className="ai-panel-note">
            取消不会删除项目或导入文件，可稍后从工作台重新生成。
          </p>
        </div>
      )}

      {/* ── plan apply UI ── */}
      {plan && !genProgress && (
        <div className="ai-panel-plan">
          <p className="ai-panel-sub">
            AI 原始分析结果将保留。应用后仍可在工作台中手动修改文字、资源、位置与时间。
          </p>

          {/* summary grid */}
          {summary && (
            <section className="ai-panel-summary">
              <div className="ai-summary-item">
                字幕片段
                <div className="ai-summary-num">{summary.subtitles}</div>
              </div>
              <div className="ai-summary-item">
                卡片组件
                <div className="ai-summary-num">{summary.cards}</div>
              </div>
              <div className="ai-summary-item">
                图表组件
                <div className="ai-summary-num">{summary.graphics}</div>
              </div>
            </section>
          )}

          {/* timeline table */}
          <section className="ai-panel-list">
            <div className="ai-list-row ai-list-th">
              <span>时间</span>
              <span>内容</span>
              <span>组件</span>
              <span>资源</span>
            </div>
            {plan.clips.map((clip, i) => (
              <div className="ai-list-row" key={i}>
                <span className="ai-list-time">{fmtTime(clip.start)}</span>
                <span className="ai-list-content">{clipContentPreview(clip)}</span>
                <span className="ai-list-track">{trackLabel(clip.track)}</span>
                <span className="ai-list-style">{clip.styleId}</span>
              </div>
            ))}
          </section>

          {/* footer buttons */}
          <footer className="ai-panel-footer">
            <button className="ai-panel-cancel-btn" onClick={cancel} disabled={busy}>
              暂不应用
            </button>
            <div className="ai-panel-footer-right">
              <label className="ai-panel-label-sm">应用方式</label>
              <select
                aria-label="AI应用方式"
                value={applicationMode}
                onChange={(e) =>
                  setApplicationMode(e.target.value as AiPlanApplicationMode)
                }
                disabled={busy}
              >
                <option value="unmodified-only">仅覆盖未修改</option>
                <option value="new-only">仅添加新</option>
                <option value="replace-all">全部替换</option>
              </select>
              <button
                className="ai-panel-apply-btn"
                disabled={!project || busy}
                onClick={() => void apply()}
              >
                应用到时间线
              </button>
            </div>
          </footer>
        </div>
      )}

      {/* ── status / error ── */}
      {status && !genProgress && !plan && (
        <p className="ai-panel-status" role="status">
          {status}
        </p>
      )}
      {error && <p className="ai-panel-error" role="alert">{error}</p>}
    </section>
  );
}