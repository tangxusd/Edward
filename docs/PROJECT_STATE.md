# Edward 项目状态

最后更新：2026-08-09

## 当前目标

开发跨 macOS/Windows 的 AI 剪视频桌面软件 **Edward 0.1.1**。已完成完整视觉与需求确认，尚未按新方案开始重构实现。

## 权威文档

- 产品与交互方案：`docs/superpowers/specs/2026-08-09-edward-0.1.1-ui-design.md`
- 全量实施计划：`docs/superpowers/plans/2026-08-09-edward-0.1.1-full-implementation.md`
- 历史设计基线：`docs/superpowers/specs/2026-08-07-ai-video-editor-design.md`

继续工作前先阅读前两份文档；已确认页面不可自行改变。

## 已确认核心规则

- 首页是唯一的新建/打开/切换项目入口；进入工作台后只能关闭项目返回首页。
- 工作台为深色、紧凑、剪映/达芬奇式布局；预览锁定，四区分栏可拖拽，时间线定位轴永远显示。
- 时间线素材可跨轨道移动、两端裁剪、自动增轨；主视频和背景默认铺满画布。
- 卡片、图表才显示组件级 AI 对话；无 AI 对话时右栏属性区占满高度。未选择组件时右栏是满高全局 AI 创作对话。
- 组件导入必须声明锁定样式和可编辑字段。卡片内文字只能在预览窗单独选中后进入文字属性修改；图表数据以表格编辑，外观以属性编辑。
- AI 生成的任何组件/数据都必须经过格式比较或合并，且由用户点击“应用”后生效。
- 用户可导入截图/视频让 AI 创作组件并保存到组件库；保存动作在客户端静默同步组件给后续管理员审核。用户不看到提交或审核提示。
- 导出手动触发，含文件名、横竖屏分辨率、帧率、格式、目录和真实进度；FFmpeg 渲染。
- 自动保存间隔为 10 分钟。

## 不在当前客户端首版开发范围

- 订阅、支付、图形/短信验证码、设备授权的外部服务。
- 管理员组件审核、发布、下架后台。
- 这些只保留客户端接口/状态边界，不伪造生产服务。

## 已确认视觉草稿

视觉草稿位于 `.superpowers/brainstorm/19764-1786246498/content/`，仅供审阅，不纳入产品资源或提交：

- 工作台基线：`edward-workbench-approved-v18.html`
- 图表属性：`chart-properties-ai-v5-appearance-fixed.html`
- 文字属性：`text-properties-v1.html`
- 卡片属性：`card-properties-ai-v4-component-only.html`
- 全局 AI 创作：`global-ai-create-v2-fixed-buttons.html`
- 时间线裁剪：`timeline-trim-v2-both-edges.html`

## 当前 Git 状态

- 当前分支：`main`
- 最新计划提交：`0be71cf docs: add Edward full implementation plan`
- 本状态文件提交后应成为新的最新提交。
- `.superpowers/` 与 `apps/desktop/.superpowers/` 是未跟踪的本地视觉/工具产物；不要默认暂存或删除。

## 推荐继续点

从实施计划的 **Task 1：Version and application navigation shell** 开始。先在隔离工作树执行，遵循 TDD，并在每个任务后运行 focused unit、build 和相关 Playwright smoke。
