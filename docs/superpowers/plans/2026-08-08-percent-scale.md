# 属性面板百分比缩放 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在右侧资源检查器中为已选预览元素提供可持久化的统一百分比缩放。

**Architecture:** `main.tsx` 从当前选中片段布局读取或初始化基准尺寸和当前缩放百分比，并通过既有 `setClipLayout`、`markClipUserEdited`、`updateProject` 写回布局。布局 schema 追加可选基准尺寸与缩放百分比，项目文件同时保存实际宽高和可恢复的缩放状态。

**Tech Stack:** React、TypeScript、Playwright、现有 `@ai-video/domain` 项目和时间线工具。

## Global Constraints

- 只修改右侧属性检查器及其桌面端回归测试。
- 只提供一个缩放百分比，始终统一联动宽高。
- 所有更新必须进入既有撤销、自动保存和项目持久化流程。

---

### Task 1: 百分比缩放属性面板

**Files:**

- Modify: `packages/domain/src/timeline.ts`
- Modify: `apps/desktop/src/renderer/main.tsx`
- Modify: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**

- Consumes: `selected.layout`、`setClipLayout(project, clipId, layout)`、`markClipUserEdited(project, clipId)`。
- Produces: `缩放` 可访问控件；值变化经 `updateProject` 写入当前片段布局及其基准尺寸、缩放百分比。

- [x] **Step 1: 写失败的桌面端测试**

断言选中卡片后，填写“缩放”为 `125` 会持久化宽 `435`、高 `225` 和缩放值 `125`。

- [x] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop exec playwright test apps/desktop/tests/smoke.spec.ts --grep "scales a selected card from the inspector"`

Expected: FAIL，因为“缩放”控件尚不存在。

- [x] **Step 3: 实现最小属性面板逻辑**

根据基准宽高和单一缩放百分比计算布局，调用 `setClipLayout`、`markClipUserEdited` 和 `updateProject` 写回。

- [x] **Step 4: 运行定向测试和构建**

Run: `pnpm --filter desktop exec playwright test apps/desktop/tests/smoke.spec.ts --grep "scales a selected card from the inspector" && pnpm --filter desktop run build`

Expected: PASS；缩放百分比统一联动宽高，重开项目后仍可读取该缩放值。

- [ ] **Step 5: 提交**

仅暂存本任务涉及的检查器、测试和设计文档；不混入现有未提交改动。
