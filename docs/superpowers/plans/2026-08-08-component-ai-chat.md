# 组件 AI 对话修改 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为选中的卡片、图形和字幕提供按组件保存的 AI 对话与显式应用内容草案。

**Architecture:** `ProjectSchema` 新增可选的 `componentConversations` 映射，以片段 ID 保存非空消息、模型 ID 与待应用内容。主进程通过新的 `componentChat:send` IPC 调用用户选择的 OpenAI 兼容模型，校验其 JSON 响应；渲染进程将对话与草案写回项目，用户点击应用时才以既有 `markClipUserEdited` 更新片段 `content`。

**Tech Stack:** Electron IPC、React、TypeScript、Zod、Playwright、Vitest、现有 OpenAI 兼容云端模型配置。

## Global Constraints

- 只修改卡片、图形和字幕的 `content`，不修改布局、样式、位置、时长、缩放或资源 ID。
- 每个组件只保存一个非空会话；无对话时不写入项目。
- 使用已配置的任一模型；请求前沿用现有授权校验边界。
- 背景资源不显示组件 AI 对话面板。

---

### Task 1: 组件会话领域结构与模型响应校验

**Files:**

- Modify: `packages/domain/src/project.ts`
- Modify: `packages/domain/src/index.ts`
- Modify: `apps/desktop/src/main/semanticAnalysisService.ts`
- Test: `packages/domain/tests/project.test.ts`
- Test: `apps/desktop/tests/semanticAnalysisService.test.ts`

**Interfaces:**

- Produces `ComponentConversationSchema`：`modelId`、`messages`、`draftContent`、`updatedAt`。
- Produces `requestComponentContentEdit(request)`，返回 `{ reply: string; draftContent: unknown }`。
- Consumes OpenAI Chat Completions，要求模型返回带 `reply`、`draftContent` 的 JSON 对象。

- [x] **Step 1: 写失败的 schema 与服务测试**

测试项目保存和重新解析后保留 `componentConversations['ai-cards-0']`；用 stub fetch 断言组件调用解析出 `reply` 与 `draftContent`，非 JSON 内容抛出错误。

- [x] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/domain test project.test.ts && pnpm --filter desktop test semanticAnalysisService.test.ts`

Expected: FAIL，因为会话 schema 和组件内容请求函数尚不存在。

- [x] **Step 3: 实现最小 schema 和服务函数**

```ts
export type ComponentChatResult = { reply: string; draftContent: unknown };
export async function requestComponentContentEdit(request: ComponentChatRequest): Promise<ComponentChatResult> {
  const response = await fetch(`${request.baseUrl}/chat/completions`, { method: 'POST', headers, body });
  return ComponentChatResultSchema.parse(JSON.parse(content));
}
```

- [x] **Step 4: 运行领域层和服务测试**

Run: `pnpm --filter @ai-video/domain test && pnpm --filter desktop test semanticAnalysisService.test.ts`

Expected: PASS；空会话不强制写入，合法响应返回完整草案，非法响应失败。

### Task 2: 主进程组件对话 IPC

**Files:**

- Modify: `apps/desktop/src/shared/ipc.ts`
- Modify: `apps/desktop/src/preload/index.ts`
- Modify: `apps/desktop/src/main/index.ts`
- Test: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**

- Produces `window.aiVideo.componentChat.send(projectId, clipId, modelId, message, history)`。
- Consumes `requestComponentContentEdit`、`ModelRepository`、`ProjectRepository` 与安全凭证读取。
- Returns `{ reply: string; draftContent: unknown }` without writing the project.

- [x] **Step 1: 写失败的 IPC 冒烟测试**

断言预加载桥暴露 `componentChat.send`，且在没有模型时组件对话发送按钮保持禁用。

- [x] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop exec playwright test apps/desktop/tests/smoke.spec.ts --grep "component AI chat"`

Expected: FAIL，因为桥和面板尚不存在。

- [x] **Step 3: 实现最小 IPC 调用**

主进程读取项目、按片段 ID 查找卡片/图形/字幕，读取项目文稿和模型凭证，调用 `requestComponentContentEdit`。找不到项目、组件或模型时抛出明确错误；不保存工程。

- [x] **Step 4: 运行定向测试和构建**

Run: `pnpm --filter desktop run build && pnpm --filter desktop exec playwright test apps/desktop/tests/smoke.spec.ts --grep "component AI chat"`

Expected: PASS；桥暴露正确，未配置模型不可发送。

### Task 3: 右侧组件 AI 对话与显式应用

**Files:**

- Create: `apps/desktop/src/renderer/ComponentAiChatPanel.tsx`
- Modify: `apps/desktop/src/renderer/main.tsx`
- Modify: `apps/desktop/src/renderer/theme.css`
- Modify: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**

- Consumes selected `TimelineClip`、`project.componentConversations`、`window.aiVideo.componentChat.send` 与 `onProjectChange`。
- Produces `AI 修改组件` 面板，包含模型选择、消息输入、消息列表与 `应用修改`。
- On application calls `markClipUserEdited(project, clip.id, draftContent)` and preserves every non-content clip field.

- [x] **Step 1: 写失败的桌面端行为测试**

测试选中卡片时出现面板、背景不出现；手工注入候选草案后，应用前卡片内容不变，点击“应用修改”后内容改变且布局保持；重新打开项目仍显示该组件的会话。

- [x] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop exec playwright test apps/desktop/tests/smoke.spec.ts --grep "component AI chat|applies component AI draft"`

Expected: FAIL，因为面板和应用动作尚不存在。

- [x] **Step 3: 实现组件级面板**

`ComponentAiChatPanel` 仅在组件有内容时渲染。成功回复后追加用户/AI消息，保存对应组件会话和草案；应用按钮仅在有草案、未请求中时可用。`main.tsx` 将更新后的项目交给既有 `updateProject`，从而进入撤销和自动保存。

- [x] **Step 4: 运行完整相关验证**

Run: `pnpm --filter desktop test && pnpm --filter @ai-video/domain test && git diff --check`

Expected: PASS；原右侧容器、属性面板和时间线流程不回归。

- [ ] **Step 5: 提交**

仅暂存本计划涉及的领域、主进程、预加载、渲染、测试和规格文件；不混入工作区现有改动。
