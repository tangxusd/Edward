/**
 * 组件检查器合约 — 定义各组件类型对应的属性面板
 */

import type { Project, TimelineClip } from '@ai-video/domain';

/** 组件种类 */
export type ComponentKind =
  | 'card'       // 卡片（变换/动画/关键帧）
  | 'chart'      // 图表（数据表/外观）
  | 'text'       // 文字（字体/字号/颜色/对齐）
  | 'subtitle'   // 字幕（统一样式）
  | 'background' // 背景
  | 'graphic'    // 图形
  | 'media'      // 主媒体
  | 'unknown';   // 未识别

/** 每个检查器接收的公共 props */
export type InspectorProps = {
  clip: TimelineClip;
  project?: Project;
  updateProject: (next: Project) => void;
  selectedText?: string;
  selectedTextStyle?: Record<string, string | number | undefined>;
  updateSelectedTextStyle?: (field: string, value: string | number) => void;
  updateSelectedScale?: (value: number) => void;
};

/**
 * 根据 clip 的 id 判断所属轨道，从而确定组件种类
 */
export function identifyComponentKind(clipId: string, project?: Project): ComponentKind {
  if (clipId.includes('subtitles')) return 'subtitle';
  if (clipId.includes('cards')) return 'card';
  if (clipId.includes('graphics')) return 'graphic';
  if (clipId.includes('background')) return 'background';
  if (clipId === 'main-media' || clipId.includes('main-media')) return 'media';
  // 如果 content 含有 text 字段，视为文字组件
  if (project) {
    const clip = findClipById(project, clipId);
    if (clip && typeof clip.content === 'object' && clip.content !== null && 'text' in clip.content) {
      return 'text';
    }
  }
  return 'unknown';
}

/** 在项目中按 id 查找 clip */
export function findClipById(project: Project, clipId: string): TimelineClip | undefined {
  for (const track of Object.values(project.tracks)) {
    const found = track.clips.find((c) => c.id === clipId);
    if (found) return found;
  }
  return undefined;
}

/**
 * 判断 clip 是否包含文字内容
 */
export function clipHasText(clip: TimelineClip): boolean {
  return typeof clip.content === 'object' && clip.content !== null && 'text' in clip.content;
}

/**
 * 获取 clip 的显示名称
 */
export function getClipDisplayName(clip: TimelineClip): string {
  if (clip.id.includes('cards')) return '卡片';
  if (clip.id.includes('graphics')) return '图形';
  if (clip.id.includes('subtitles')) return '字幕';
  if (clip.id.includes('background')) return '背景';
  if (clip.id === 'main-media') return '媒体';
  return clip.id;
}

// ============================================================
// 卡片属性检查器 — CardInspector
// ============================================================

import { useCallback, useState } from 'react';
import { markClipUserEdited } from '@ai-video/domain';

type CardInspectorProps = {
  clip: TimelineClip;
  project?: Project;
  updateProject: (next: Project) => void;
  updateSelectedScale: (value: number) => void;
};

/**
 * 卡片属性检查器 — 变换/动画/关键帧
 * 匹配视觉确认稿 card-properties-ai-v4-component-only.html
 */
export function CardInspector({ clip, project, updateProject, updateSelectedScale }: CardInspectorProps): React.JSX.Element {
  const layout = clip.layout;
  const x = layout?.x ?? 0;
  const y = layout?.y ?? 0;
  const scale = layout?.scale ?? 100;
  const [linked, setLinked] = useLinkedState(true);

  const handleTextChange = useCallback(
    (event: React.ChangeEvent<HTMLTextAreaElement>) => {
      if (project) {
        updateProject(
          markClipUserEdited(project, clip.id, {
            ...(clip.content as object),
            text: event.target.value,
          }),
        );
      }
    },
    [project, clip, updateProject],
  );

  const content = clip.content as Record<string, unknown> | undefined;
  const clipText = typeof content?.text === 'string' ? content.text : '';

  return (
    <div className="rp-inspector">
      <div className="rp-inspector-head">
        <h3 className="rp-inspector-title">卡片属性</h3>
        <button type="button" className="rp-inspector-undo" onClick={() => {/* 还原默认 */}}>
          还原
        </button>
      </div>

      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>位置与大小</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">缩放</span>
          <div className="rp-param-xy">
            <span className="rp-param-num" title="X 缩放">X {scale}%</span>
            <button
              type="button"
              className={`rp-chain-btn${linked ? ' active' : ''}`}
              onClick={() => setLinked(!linked)}
              title="链接 X/Y 缩放"
            >
              {linked ? '🔗' : '⛓️‍💥'}
            </button>
            <span className="rp-param-num" title="Y 缩放">Y {scale}%</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn" title="添加关键帧">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">位置</span>
          <div className="rp-param-xy">
            <span className="rp-param-num">X {x}</span>
            <span />
            <span className="rp-param-num">Y {y}</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn" title="添加关键帧">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">不透明度</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">100%</span>
          <button type="button" className="rp-key-btn" title="添加关键帧">◇</button>
        </div>
      </div>

      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>入场动画</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>
        <div className="rp-inspector-dropdown">
          <span>无</span>
          <span className="rp-dropdown-arrow">⌄</span>
        </div>
      </div>

      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>关键帧</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>
        <button type="button" className="rp-keyframes-add-btn" title="添加关键帧">
          ＋ 添加关键帧
        </button>
      </div>

      {clipText ? (
        <div className="rp-inspector-section">
          <div className="rp-inspector-section-title">
            <span>文字内容</span>
            <span className="rp-collapse-arrow">⌃</span>
          </div>
          <textarea
            className="rp-content-edit"
            aria-label="文字内容"
            value={clipText}
            onChange={handleTextChange}
          />
        </div>
      ) : null}

      {layout ? (
        <div className="rp-inspector-section">
          <div className="rp-inspector-section-title">
            <span>缩放控制</span>
          </div>
          <label className="rp-scale-row">
            缩放
            <input
              aria-label="缩放"
              type="number"
              min="1"
              max="1000"
              value={scale}
              onChange={(event) => updateSelectedScale(Number(event.target.value))}
            />
            %
          </label>
        </div>
      ) : null}
    </div>
  );
}

function useLinkedState(initial: boolean): [boolean, (v: boolean) => void] {
  const [linked, setLinked] = useState(initial);
  return [linked, setLinked];
}

// ============================================================
// 图形属性检查器 — GraphicInspector
// ============================================================

type GraphicInspectorProps = {
  clip: TimelineClip;
  project?: Project;
  updateProject: (next: Project) => void;
  updateSelectedScale: (value: number) => void;
};

/**
 * 图形属性检查器 — 与卡片类似
 */
export function GraphicInspector({ clip, project, updateProject, updateSelectedScale }: GraphicInspectorProps): React.JSX.Element {
  const layout = clip.layout;
  const scale = layout?.scale ?? 100;

  return (
    <div className="rp-inspector">
      <div className="rp-inspector-head">
        <h3 className="rp-inspector-title">图形属性</h3>
        <button type="button" className="rp-inspector-undo" onClick={() => {/* 还原默认 */}}>
          还原
        </button>
      </div>

      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>位置与大小</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">位置</span>
          <div className="rp-param-xy">
            <span className="rp-param-num">X {layout?.x ?? 0}</span>
            <span />
            <span className="rp-param-num">Y {layout?.y ?? 0}</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn" title="添加关键帧">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">缩放</span>
          <div className="rp-param-xy">
            <span className="rp-param-num">X {scale}%</span>
            <span />
            <span className="rp-param-num">Y {scale}%</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn" title="添加关键帧">◇</button>
        </div>
      </div>

      {layout ? (
        <div className="rp-inspector-section">
          <div className="rp-inspector-section-title">
            <span>缩放控制</span>
          </div>
          <label className="rp-scale-row">
            缩放
            <input
              aria-label="缩放"
              type="number"
              min="1"
              max="1000"
              value={scale}
              onChange={(event) => updateSelectedScale(Number(event.target.value))}
            />
            %
          </label>
        </div>
      ) : null}
    </div>
  );
}

// ============================================================
// 文字属性检查器 — TextInspector
// ============================================================

type TextInspectorProps = {
  clip: TimelineClip;
  selectedText?: string;
  selectedTextStyle?: Record<string, string | number | undefined>;
  updateSelectedTextStyle?: (field: string, value: string | number) => void;
};

/**
 * 文字属性检查器 — 字体/字号/颜色/对齐
 * 匹配视觉确认稿 text-properties-v1.html
 */
export function TextInspector({
  clip,
  selectedText,
  selectedTextStyle,
  updateSelectedTextStyle,
}: TextInspectorProps): React.JSX.Element {
  const style = selectedTextStyle ?? {};
  const fontFamily = String(style.fontFamily ?? '思源黑体');
  const fontSize = Number(style.fontSize ?? 46);
  const color = String(style.color ?? '#F7F2DC');
  const bg = String(style.background ?? '');

  return (
    <div className="rp-inspector rp-inspector-text">
      <div className="rp-inspector-head">
        <h3 className="rp-inspector-title">文字属性</h3>
        <button type="button" className="rp-inspector-undo" onClick={() => {/* 还原默认 */}}>
          还原
        </button>
      </div>

      {/* 内容 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>内容</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>
        <div className="rp-content-edit" contentEditable>
          {selectedText || '文字内容'}
        </div>
      </div>

      {/* 字体与排版 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>字体与排版</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">字体</span>
          <div className="rp-inspector-select">
            <span>{fontFamily}</span>
            <span className="rp-dropdown-arrow">⌄</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">字号</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">{fontSize}</span>
          <button type="button" className="rp-key-btn rp-key-on">◆</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">颜色</span>
          <div className="rp-inspector-select">
            <span className="rp-color-swatch" style={{ background: color }} />
            <span>{color}</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">对齐</span>
          <div className="rp-align-group">
            <button type="button" className="rp-align-btn">☰</button>
            <button type="button" className="rp-align-btn active">≡</button>
            <button type="button" className="rp-align-btn">☷</button>
          </div>
          <span />
          <span />
        </div>
      </div>

      {/* 位置与大小 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>位置与大小</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">位置</span>
          <div className="rp-param-two">
            <span className="rp-param-num">X 0</span>
            <span className="rp-param-num">Y 0</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">缩放</span>
          <div className="rp-param-two">
            <span className="rp-param-num">X 100%</span>
            <span className="rp-param-num">Y 100%</span>
          </div>
          <span />
          <button type="button" className="rp-key-btn">◇</button>
        </div>

        <div className="rp-inspector-param">
          <span className="rp-param-label">不透明度</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">100%</span>
          <button type="button" className="rp-key-btn">◇</button>
        </div>
      </div>

      {/* 文字背景 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>文字背景</span>
          <i className="rp-toggle active" />
        </div>
        <div className="rp-bg-options">
          <button type="button" className="rp-bg-option active">纯色</button>
          <button type="button" className="rp-bg-option">描边</button>
          <button type="button" className="rp-bg-option">圆角卡片</button>
        </div>
      </div>

      {/* 阴影与入场动画 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>阴影与入场动画</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>
        <div className="rp-inspector-param">
          <span className="rp-param-label">入场</span>
          <div className="rp-inspector-select">
            <span>淡入上移</span>
            <span className="rp-dropdown-arrow">⌄</span>
          </div>
          <span />
          <span />
        </div>
      </div>
    </div>
  );
}

// ============================================================
// 图表属性检查器 — ChartInspector
// ============================================================

/**
 * 图表属性检查器 — 数据表 + 外观
 * 匹配视觉确认稿 chart-properties-ai-v5-appearance-fixed.html
 */
export function ChartInspector(_props: InspectorProps): React.JSX.Element {
  return (
    <div className="rp-inspector rp-inspector-chart">
      <div className="rp-inspector-head">
        <h3 className="rp-inspector-title">图表属性</h3>
        <button type="button" className="rp-inspector-undo" onClick={() => {/* 还原默认 */}}>
          还原
        </button>
      </div>

      {/* 数据表 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>数据</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>
        <div className="rp-table-wrap">
          <table className="rp-chart-table">
            <thead>
              <tr>
                <th>季度</th>
                <th>系列 A</th>
                <th>系列 B</th>
                <th>系列 C</th>
              </tr>
            </thead>
            <tbody>
              <tr><td>Q1</td><td>30</td><td>45</td><td>22</td></tr>
              <tr><td>Q2</td><td>38</td><td>52</td><td>28</td></tr>
              <tr><td>Q3</td><td>42</td><td>56</td><td>35</td></tr>
              <tr><td>Q4</td><td>35</td><td>67</td><td className="rp-cell-editing">86</td></tr>
            </tbody>
          </table>
          <div className="rp-table-edit-bar">
            <span>检测到表格修改：Q4 从 78 更新为 86</span>
            <span className="rp-table-actions">
              <button type="button" className="rp-table-discard">撤销</button>
              <button type="button" className="rp-table-keep">确认修改</button>
            </span>
          </div>
        </div>
      </div>

      {/* 图表外观 */}
      <div className="rp-inspector-section">
        <div className="rp-inspector-section-title">
          <span>图表外观</span>
          <span className="rp-collapse-arrow">⌃</span>
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">系列颜色</span>
          <span className="rp-swatches">
            <i className="rp-swatch rp-swatch-blue active" />
            <i className="rp-swatch rp-swatch-green" />
            <i className="rp-swatch rp-swatch-amber" />
            <i className="rp-swatch rp-swatch-purple" />
          </span>
          <span className="rp-param-num">主题</span>
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">柱形圆角</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">8 px</span>
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">柱间距</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">18 px</span>
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">组间距</span>
          <div className="rp-param-slider">
            <i className="rp-slider-track" />
          </div>
          <span className="rp-param-num">24 px</span>
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">坐标轴</span>
          <span className="rp-axis-style">
            <button type="button" className="rp-axis-btn selected">显示</button>
            <button type="button" className="rp-axis-btn">标签</button>
            <button type="button" className="rp-axis-btn">刻度</button>
          </span>
          <i className="rp-toggle active" />
        </div>

        <div className="rp-appearance-row">
          <span className="rp-param-label">网格线</span>
          <span className="rp-axis-style">
            <button type="button" className="rp-axis-btn">实线</button>
            <button type="button" className="rp-axis-btn selected">虚线</button>
            <button type="button" className="rp-axis-btn">隐藏</button>
          </span>
          <i className="rp-toggle" />
        </div>

        <p className="rp-appearance-note">
          颜色、圆角和间距支持独立动画关键帧；坐标轴与网格线为图表全局样式。
        </p>
      </div>
    </div>
  );
}

// ============================================================
// 背景属性检查器 — BackgroundInspector
// ============================================================

type BackgroundInspectorProps = {
  clip: TimelineClip;
};

/**
 * 背景属性检查器
 */
export function BackgroundInspector({ clip }: BackgroundInspectorProps): React.JSX.Element {
  return (
    <div className="rp-inspector">
      <div className="rp-inspector-head">
        <h3 className="rp-inspector-title">背景属性</h3>
      </div>
      <div className="rp-inspector-section">
        <div className="rp-inspector-param">
          <span className="rp-param-label">资源 ID</span>
          <span className="rp-param-num">{clip.styleId}</span>
        </div>
      </div>
    </div>
  );
}