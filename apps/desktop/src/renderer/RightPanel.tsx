import { useState } from 'react';
import { markClipUserEdited, type Project, type Resource } from '@ai-video/domain';
import { ResourceChoicePanel } from './ResourceChoicePanel.js';
import { ComponentAiChatPanel } from './ComponentAiChatPanel.js';
import { AiAnalysisPanel } from './AiAnalysisPanel.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
import {
  identifyComponentKind,
  CardInspector,
  GraphicInspector,
  TextInspector,
  BackgroundInspector,
} from './componentContract.js';
import type { ModelRecord } from '../main/modelRepository.js';

type Props = {
  project?: Project;
  updateProject: (next: Project) => void;
  selectedClipId?: string;
  selected: Project['tracks'][keyof Project['tracks']]['clips'][0] | undefined;
  selectedText: string;
  selectedTextStyle: Record<string, string | number | undefined>;
  updateSelectedTextStyle: (field: 'fontFamily' | 'fontSize' | 'color' | 'background', value: string | number) => void;
  updateSelectedScale: (value: number) => void;
  cardStyles: Resource[];
  backgrounds: Resource[];
  graphics: Resource[];
  replaceSelectedResource: (resourceId: string) => void;
  setModels: (models: ModelRecord[]) => void;
};

type TabId = 'properties' | 'resources';

export function RightPanel({
  project,
  updateProject,
  selectedClipId,
  selected,
  selectedText,
  selectedTextStyle,
  updateSelectedTextStyle,
  updateSelectedScale,
  cardStyles,
  backgrounds,
  graphics,
  replaceSelectedResource,
  setModels,
}: Props): React.JSX.Element {
  const [activeTab, setActiveTab] = useState<TabId>('properties');

  /** 根据选中的 clip 确定组件种类，渲染对应检查器 */
  const renderInspector = (): React.JSX.Element | null => {
    if (!selected || !project) return <p className="rp-empty-hint">未选择资源</p>;

    const kind = identifyComponentKind(selected.id, project);

    switch (kind) {
      case 'card':
        return (
          <CardInspector
            clip={selected}
            project={project}
            updateProject={updateProject}
            updateSelectedScale={updateSelectedScale}
          />
        );
      case 'graphic':
        return (
          <GraphicInspector
            clip={selected}
            project={project}
            updateProject={updateProject}
            updateSelectedScale={updateSelectedScale}
          />
        );
      case 'text':
        return (
          <TextInspector
            clip={selected}
            selectedText={selectedText}
            selectedTextStyle={selectedTextStyle}
            updateSelectedTextStyle={updateSelectedTextStyle as (field: string, value: string | number) => void}
          />
        );
      case 'subtitle':
        return (
          <>
            <SubtitleStylePanel
              project={project}
              onChange={updateProject}
            />
          </>
        );
      case 'background':
        return (
          <BackgroundInspector
            clip={selected}
          />
        );
      default:
        // 兜底：显示基础属性
        return (
          <div className="rp-inspector">
            <div className="rp-inspector-head">
              <h3 className="rp-inspector-title">组件属性</h3>
            </div>
            <div className="rp-inspector-section">
              <div className="rp-inspector-param">
                <span className="rp-param-label">位置</span>
                <div className="rp-param-xy">
                  <span className="rp-param-num">X {(selected.layout as { x?: number })?.x ?? 0}</span>
                  <span />
                  <span className="rp-param-num">Y {(selected.layout as { y?: number })?.y ?? 0}</span>
                </div>
                <span />
                <button type="button" className="rp-key-btn">◇</button>
              </div>
              <div className="rp-inspector-param">
                <span className="rp-param-label">缩放</span>
                <span className="rp-param-num">{selected.layout?.scale ?? 100}%</span>
                <span />
                <button type="button" className="rp-key-btn">◇</button>
              </div>
            </div>
          </div>
        );
    }
  };

  return (
    <aside className="right-panel">
      {/* Tabs: 属性 / 资源 */}
      <div className="right-panel-tabs">
        <button
          type="button"
          className={`right-panel-tab${activeTab === 'properties' ? ' active' : ''}`}
          onClick={() => setActiveTab('properties')}
        >
          属性
        </button>
        <button
          type="button"
          className={`right-panel-tab${activeTab === 'resources' ? ' active' : ''}`}
          onClick={() => setActiveTab('resources')}
        >
          资源
        </button>
      </div>

      {/* Scrollable content area (properties / resources) */}
      <div className="right-panel-content">
        {activeTab === 'properties' ? (
          <section className="right-panel-properties">
            {renderInspector()}

            {/* Component AI chat (when cards/graphics/subtitles) */}
            {selected &&
            (selected.id.includes('cards') ||
              selected.id.includes('graphics') ||
              selected.id.includes('subtitles')) ? (
              <ComponentAiChatPanel
                project={project!}
                clip={selected}
                onProjectChange={updateProject}
                onApply={(content) =>
                  updateProject(
                    markClipUserEdited(project!, selected.id, content),
                  )
                }
              />
            ) : null}

            {/* AI analysis panel */}
            <AiAnalysisPanel
              project={project}
              onApplied={updateProject}
            />
          </section>
        ) : (
          <section className="right-panel-resources">
            {selected?.id.includes('cards') ? (
              <ResourceChoicePanel
                label="卡片"
                resources={cardStyles}
                onSelect={replaceSelectedResource}
              />
            ) : null}
            {selected?.id.includes('background') ? (
              <ResourceChoicePanel
                label="背景"
                resources={backgrounds}
                onSelect={replaceSelectedResource}
              />
            ) : null}
            {selected?.id.includes('graphics') ? (
              <ResourceChoicePanel
                label="图形"
                resources={graphics}
                onSelect={replaceSelectedResource}
              />
            ) : null}
          </section>
        )}
      </div>

      {/* AI Chat Section (bottom half, fixed) */}
      <section className="right-panel-ai">
        <header className="rp-ai-header">
          <b>AI 对话 · 图表</b>
          <span className="rp-ai-model-selector">GPT-4.1 ▾</span>
        </header>

        <div className="rp-ai-history">
          <div className="rp-ai-msg rp-ai-msg-user">
            把第三季度数据改为 42、56、67。
          </div>
          <div className="rp-ai-msg rp-ai-msg-bot">
            已生成数据更新建议。请点击"应用"后写入图表。
          </div>
        </div>

        <div className="rp-ai-input">
          <div className="rp-ai-field">输入对组件的修改要求…</div>
          <button type="button" className="rp-ai-send">
            发送
          </button>
        </div>

        <p className="rp-ai-note">
          无对话历史时不保存；切换回该组件后恢复独立历史。
        </p>
      </section>
    </aside>
  );
}