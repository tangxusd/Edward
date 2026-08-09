import { useCallback, useState } from 'react';
import { markClipUserEdited, type Project, type Resource } from '@ai-video/domain';
import { ResourceChoicePanel } from './ResourceChoicePanel.js';
import { ComponentAiChatPanel } from './ComponentAiChatPanel.js';
import { AiAnalysisPanel } from './AiAnalysisPanel.js';
import { SubtitleStylePanel } from './SubtitleStylePanel.js';
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

  const handleTextChange = useCallback(
    (event: React.ChangeEvent<HTMLTextAreaElement>) => {
      if (project && selected) {
        updateProject(
          markClipUserEdited(project, selected.id, {
            ...(selected.content as object),
            text: event.target.value,
          }),
        );
      }
    },
    [project, selected, updateProject],
  );

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
            {selected ? (
              <div className="rp-properties-box">
                {/* Position */}
                <div className="rp-property-line">
                  <span className="rp-property-label">位置</span>
                  <span className="rp-property-value">
                    X {(selected.layout as { x?: number })?.x ?? 0}　Y{' '}
                    {(selected.layout as { y?: number })?.y ?? 0}
                  </span>
                </div>
                {/* Scale */}
                <div className="rp-property-line">
                  <span className="rp-property-label">缩放</span>
                  <span className="rp-property-value">
                    {selected.layout?.scale ?? 100}%
                  </span>
                </div>
                {/* Animation */}
                <div className="rp-property-line">
                  <span className="rp-property-label">入场动画</span>
                  <span className="rp-property-value">无</span>
                </div>
                {/* Keyframes */}
                <div className="rp-property-line">
                  <span className="rp-property-label">关键帧</span>
                  <span className="rp-property-value rp-keyframes-btn">＋</span>
                </div>
              </div>
            ) : (
              <p className="rp-empty-hint">未选择资源</p>
            )}

            {/* Text content editor (when clip has text) */}
            {selected && selectedText ? (
              <label className="rp-text-editor">
                文字内容
                <textarea
                  aria-label="文字内容"
                  value={selectedText}
                  onChange={handleTextChange}
                />
              </label>
            ) : null}

            {/* Scale input (when clip has layout) */}
            {selected?.layout ? (
              <fieldset className="rp-scale-fieldset" aria-label="缩放属性">
                <label>
                  缩放
                  <input
                    aria-label="缩放"
                    type="number"
                    min="1"
                    max="1000"
                    value={selected.layout.scale ?? 100}
                    onChange={(event) =>
                      updateSelectedScale(Number(event.target.value))
                    }
                  />
                  %
                </label>
              </fieldset>
            ) : null}

            {/* Subtitle style (when subtitles) */}
            {selected?.id.includes('subtitles') ? (
              <SubtitleStylePanel
                project={project}
                onChange={updateProject}
              />
            ) : null}

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