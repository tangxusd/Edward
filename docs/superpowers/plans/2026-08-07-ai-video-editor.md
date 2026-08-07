# AI 剪视频工具 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 macOS 与 Windows 桌面 AI 剪视频工具：从文稿和单条主媒体生成可人工修订的多轨工程，并支持可复用资源、手动 HEVC 导出和订阅授权边界。

**Architecture:** Electron 主进程负责安全 IPC、工作目录、任务调度和系统凭证；React 渲染进程提供项目、预览、时间线、资源库和导出界面。Python sidecar 运行 faster-whisper，FFmpeg 负责媒体探测和渲染；云端模型仅接收文稿与时间戳并返回 schema 校验后的剪辑计划。

**Tech Stack:** Electron、React、TypeScript、Vite、Vitest、Playwright、Zod、Zustand、Python、faster-whisper、FFmpeg、OpenAI-compatible HTTP API。

## Global Constraints

- 支持 macOS 和 Windows；不得用平台私有能力替代另一平台的核心功能。
- 每个项目必须有一份文稿和一条音频或视频主媒体。
- 原始媒体不上传；本地转写，云端只做文本语义分析。
- API Key 仅保存在系统安全凭证库，永不写入工程、日志、资源包或导出文件。
- 默认画幅 16:9，支持 9:16、720p、1080p、1440p、4K 和自定义分辨率。
- 只允许用户手动导出；普通输出为 MP4/HEVC/AAC。
- 透明 MOV/HEVC + Alpha 在剪映 macOS、Windows 实测通过前不得标记为兼容。
- 资源库只能从设置页导入受校验 ZIP/JSON/CSS/静态资源包，禁止执行导入包代码。
- 每账号最多三台设备、同时一个活动会话、本地编辑离线窗口最多两小时；触发 AI 时在线校验。
- 不实现多机位、调色、混音台、关键帧动画或复杂转场。

---

## 文件结构

```text
apps/desktop/
  src/main/                 Electron 主进程和 IPC
  src/preload/              严格白名单 API
  src/renderer/             React UI
  src/shared/               Zod schema 与跨进程类型
  python/                   转写 sidecar
  tests/                    单元、IPC、端到端测试
packages/domain/            工程、时间线、资源和 AI 计划纯领域逻辑
packages/media/             FFmpeg 命令构造、进度解析与媒体探测
packages/licensing/         授权令牌、设备 ID 与会话协议客户端
docs/                       已确认设计和本计划
```

## Task 1: 初始化可测试的 Electron 工作区

**Files:**
- Create: `package.json`, `pnpm-workspace.yaml`, `tsconfig.base.json`
- Create: `apps/desktop/package.json`, `apps/desktop/vite.*.ts`, `apps/desktop/src/main/index.ts`
- Create: `apps/desktop/src/preload/index.ts`, `apps/desktop/src/renderer/main.tsx`
- Create: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**
- Produces: `window.aiVideo` only through `contextBridge`; Node integration remains disabled in renderer.

- [ ] **Step 1: 写启动窗口失败测试**

```ts
it('exposes only the desktop bridge', async () => {
  await expect(window.aiVideo).toBeDefined();
  expect((window as { require?: unknown }).require).toBeUndefined();
});
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop test smoke.spec.ts`

Expected: FAIL because the desktop project does not exist.

- [ ] **Step 3: 初始化 pnpm、Electron、Vite、React、TypeScript、Vitest 和 Playwright**

创建 `BrowserWindow({ webPreferences: { preload, contextIsolation: true, nodeIntegration: false } })`，在 preload 中以 `contextBridge.exposeInMainWorld('aiVideo', {})` 暴露空白安全桥。

- [ ] **Step 4: 运行启动和 smoke 测试**

Run: `pnpm install && pnpm --filter desktop test smoke.spec.ts && pnpm --filter desktop dev`

Expected: PASS；桌面窗口加载 React 根节点，renderer 无 Node `require`。

- [ ] **Step 5: 提交**

```bash
git add package.json pnpm-workspace.yaml tsconfig.base.json apps/desktop
git commit -m "feat: bootstrap Electron desktop workspace"
```

## Task 2: 定义工程、轨道和资源领域模型

**Files:**
- Create: `packages/domain/src/project.ts`, `packages/domain/src/timeline.ts`, `packages/domain/src/resource.ts`, `packages/domain/src/index.ts`
- Create: `packages/domain/tests/project.test.ts`, `packages/domain/tests/timeline.test.ts`

**Interfaces:**
- Produces: `Project`, `Track`, `TimelineClip`, `Resource`, `createProject()`, `moveClip()`, `resizeClip()`.

- [ ] **Step 1: 写时间线操作失败测试**

```ts
const project = createProject({ scriptPath: '/a.txt', mediaPath: '/a.mp3', mediaKind: 'audio' });
const moved = moveClip(project, 'card-1', 4);
expect(moved.tracks.card.clips[0].start).toBe(4);
expect(() => resizeClip(moved, 'card-1', -1)).toThrow('duration must be positive');
```

- [ ] **Step 2: 运行领域测试确认失败**

Run: `pnpm --filter @ai-video/domain test`

Expected: FAIL because the domain package is absent.

- [ ] **Step 3: 实现 Zod schema 和不可变领域操作**

定义固定轨道 `mainMedia`、`background`、`subtitles`、`cards`、`graphics`。`createProject()` 对 audio 自动创建使用 `defaultBackgroundResourceId` 的背景片段；所有 clip 包含 `id`、`start`、`duration`、`content`、`styleId`、`locked` 和可选 `sourceRange`。

- [ ] **Step 4: 运行测试**

Run: `pnpm --filter @ai-video/domain test`

Expected: PASS；移动、复制、删除和时长调整不修改输入对象。

- [ ] **Step 5: 提交**

```bash
git add packages/domain
git commit -m "feat: add project and timeline domain model"
```

## Task 3: 实现工作目录和多项目持久化

**Files:**
- Create: `apps/desktop/src/main/workspace.ts`, `apps/desktop/src/main/projectRepository.ts`
- Create: `apps/desktop/src/shared/ipc.ts`
- Modify: `apps/desktop/src/main/index.ts`, `apps/desktop/src/preload/index.ts`
- Create: `apps/desktop/tests/projectRepository.test.ts`

**Interfaces:**
- Consumes: `Project` from `@ai-video/domain`.
- Produces: `WorkspaceService.setRoot(root)`, `ProjectRepository.create(input)`, `open(id)`, `save(project)`, `archive(id)`.

- [ ] **Step 1: 写工程往返持久化失败测试**

```ts
await repository.create(input);
const reopened = await repository.open(input.id);
expect(reopened).toEqual(expect.objectContaining({ id: input.id, media: input.media }));
```

- [ ] **Step 2: 运行仓库测试确认失败**

Run: `pnpm --filter desktop test projectRepository.test.ts`

Expected: FAIL because `ProjectRepository` is undefined.

- [ ] **Step 3: 实现目录布局和原子写入**

创建 `projects/`、`library/`、`cache/`。用临时同目录文件加 rename 写入 `project.json`；项目含 `media/`、`transcripts/`、`ai-plans/`、`previews/`、`exports/`。IPC 只接受 schema 校验后的输入和用户选择的目录。

- [ ] **Step 4: 运行测试**

Run: `pnpm --filter desktop test projectRepository.test.ts`

Expected: PASS；重开工程数据相同，归档项目不出现在最近项目列表。

- [ ] **Step 5: 提交**

```bash
git add apps/desktop/src/main apps/desktop/src/shared apps/desktop/src/preload apps/desktop/tests
git commit -m "feat: add workspace and project persistence"
```

## Task 4: 实现资源库导入、分类、收藏和预览索引

**Files:**
- Create: `packages/domain/src/stylePackage.ts`, `packages/domain/tests/stylePackage.test.ts`
- Create: `apps/desktop/src/main/libraryRepository.ts`, `apps/desktop/src/main/stylePackageImporter.ts`
- Create: `apps/desktop/tests/stylePackageImporter.test.ts`

**Interfaces:**
- Produces: `StylePackageManifest`, `importStylePackage(zipPath)`, `listResources(filter)`, `toggleFavorite(id)`.

- [ ] **Step 1: 写风格包失败测试**

```ts
expect(() => parseStylePackageManifest({ version: 1, entryScript: 'run.js' })).toThrow('scripts are not allowed');
expect(parseStylePackageManifest(validManifest).type).toBe('card-style');
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/domain test stylePackage.test.ts`

Expected: FAIL because package parser is absent.

- [ ] **Step 3: 实现受限导入和缩略图索引**

只接收 ZIP 内 JSON、CSS、PNG、JPEG、WebP、SVG 和字体文件；拒绝脚本、路径穿越和未声明文件。复制合规资源至 `library/<type>/<id>/`，写入 `library/index.json`，为背景生成缩略图，为样式保存可渲染预览数据。

- [ ] **Step 4: 运行导入测试**

Run: `pnpm --filter desktop test stylePackageImporter.test.ts`

Expected: PASS；非法包不写入资源库，合规包可分类、收藏和读取预览。

- [ ] **Step 5: 提交**

```bash
git add packages/domain apps/desktop/src/main apps/desktop/tests
git commit -m "feat: add import-only resource library"
```

## Task 5: 实现媒体探测、波形和本地转写任务

**Files:**
- Create: `packages/media/src/ffprobe.ts`, `packages/media/src/ffmpeg.ts`, `packages/media/tests/ffprobe.test.ts`
- Create: `apps/desktop/python/transcribe.py`, `apps/desktop/src/main/transcriptionService.ts`
- Create: `apps/desktop/tests/transcriptionService.test.ts`

**Interfaces:**
- Produces: `probeMedia(path): Promise<MediaInfo>` and `transcribe(mediaPath, onProgress): Promise<TranscriptSegment[]>`.

- [ ] **Step 1: 写媒体解析和转写输出失败测试**

```ts
expect(await probeMedia(fixtureMp4)).toMatchObject({ kind: 'video', durationMs: expect.any(Number) });
expect(parseTranscript('[0.0-1.2] 你好')).toEqual([{ start: 0, end: 1.2, text: '你好' }]);
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/media test && pnpm --filter desktop test transcriptionService.test.ts`

Expected: FAIL because the media package and service are absent.

- [ ] **Step 3: 实现 FFprobe、波形命令和 Python sidecar 协议**

FFprobe 使用 JSON 输出；转写 sidecar 输出 NDJSON 进度事件和最终片段。`faster-whisper` 固定在本地运行，视频仅提取本地音频。服务取消时终止 child process，保留已完成项目状态。

- [ ] **Step 4: 运行测试与代表性媒体验证**

Run: `pnpm --filter @ai-video/media test && pnpm --filter desktop test transcriptionService.test.ts`

Expected: PASS；测试媒体产生时长、波形文件和时间戳字幕，不向网络发出媒体请求。

- [ ] **Step 5: 提交**

```bash
git add packages/media apps/desktop/python apps/desktop/src/main apps/desktop/tests
git commit -m "feat: add local media analysis and transcription"
```

## Task 6: 实现模型记录、安全凭证和 AI 剪辑计划校验

**Files:**
- Create: `packages/domain/src/aiPlan.ts`, `packages/domain/tests/aiPlan.test.ts`
- Create: `apps/desktop/src/main/modelRepository.ts`, `apps/desktop/src/main/semanticAnalysisService.ts`
- Create: `apps/desktop/tests/semanticAnalysisService.test.ts`

**Interfaces:**
- Produces: `ModelRecord`, `AiEditPlan`, `validateAiEditPlan(plan, project)`, `analyzeSemantics(request)`.

- [ ] **Step 1: 写非法 AI 计划失败测试**

```ts
expect(() => validateAiEditPlan({ clips: [{ start: 10, duration: -1 }] }, project)).toThrow('duration must be positive');
expect(() => validateAiEditPlan(planWithUnknownResource, project)).toThrow('resource not found');
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/domain test aiPlan.test.ts`

Expected: FAIL because `validateAiEditPlan` is absent.

- [ ] **Step 3: 实现多模型记录与 OpenAI-compatible 请求**

模型记录保存 name、baseUrl、modelId 和安全凭证引用；密钥写入系统凭证库。请求仅含文稿、转写和时间戳，要求 JSON schema 响应。服务端响应先做 JSON 解析，再做 Zod 和工程边界校验；失败不改动当前时间线。

- [ ] **Step 4: 运行测试**

Run: `pnpm --filter @ai-video/domain test aiPlan.test.ts && pnpm --filter desktop test semanticAnalysisService.test.ts`

Expected: PASS；请求没有媒体文件路径或密钥，非法模型输出被拒绝。

- [ ] **Step 5: 提交**

```bash
git add packages/domain apps/desktop/src/main apps/desktop/tests
git commit -m "feat: add validated semantic analysis"
```

## Task 7: 应用 AI 计划并保留人工修订版本

**Files:**
- Create: `packages/domain/src/planApplication.ts`, `packages/domain/tests/planApplication.test.ts`
- Modify: `packages/domain/src/project.ts`

**Interfaces:**
- Consumes: validated `AiEditPlan`, `Project`.
- Produces: `applyPlan(project, plan, mode: 'unmodified-only' | 'new-only' | 'replace-all'): Project`.

- [ ] **Step 1: 写人工修订保护失败测试**

```ts
const edited = markClipUserEdited(project, 'card-1');
const next = applyPlan(edited, regeneratedPlan, 'unmodified-only');
expect(next.tracks.cards.clips.find(clip => clip.id === 'card-1')?.content).toEqual(editedCard.content);
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/domain test planApplication.test.ts`

Expected: FAIL because plan application is absent.

- [ ] **Step 3: 实现版本快照和三种应用模式**

保存原始 AI 计划到 `ai-plans/`，对用户触及的片段标记 `userEditedAt`。仅替换未编辑片段、仅添加新片段或完整替换都生成新的工程 revision，可回退。

- [ ] **Step 4: 运行测试**

Run: `pnpm --filter @ai-video/domain test planApplication.test.ts`

Expected: PASS；人工文字、时长和样式不被“仅未修改”模式覆盖。

- [ ] **Step 5: 提交**

```bash
git add packages/domain
git commit -m "feat: preserve user edits across AI regeneration"
```

## Task 8: 构建项目启动页和设置页

**Files:**
- Create: `apps/desktop/src/renderer/features/home/HomePage.tsx`, `NewProjectDialog.tsx`
- Create: `apps/desktop/src/renderer/features/settings/WorkspaceSettings.tsx`, `ModelSettings.tsx`, `LibrarySettings.tsx`
- Create: `apps/desktop/src/renderer/state/projectStore.ts`
- Create: `apps/desktop/tests/newProjectDialog.spec.ts`

**Interfaces:**
- Consumes: preload methods `chooseWorkspace`, `createProject`, `importStylePackage`, `saveModelRecord`.
- Produces: project bootstrap request with one script and one media path.

- [ ] **Step 1: 写新建项目必填失败测试**

```ts
render(<NewProjectDialog onCreate={createProject} />);
await user.click(screen.getByRole('button', { name: '创建项目' }));
expect(screen.getByText('请选择音频或视频')).toBeVisible();
expect(createProject).not.toHaveBeenCalled();
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop test newProjectDialog.spec.ts`

Expected: FAIL because the dialog is absent.

- [ ] **Step 3: 实现工作目录、最近项目、模型状态和资源导入 UI**

新建项目要求媒体与文稿。设置页显示资源分类、收藏、缩略图预览和导入；模型列表显示当前模型名，在线为绿色、不可用为红色。资源库不提供“从当前页面保存”按钮。

- [ ] **Step 4: 运行测试**

Run: `pnpm --filter desktop test newProjectDialog.spec.ts`

Expected: PASS；缺少任一输入不可提交，完整输入能调用一次创建 IPC。

- [ ] **Step 5: 提交**

```bash
git add apps/desktop/src/renderer apps/desktop/tests
git commit -m "feat: add project and settings workflows"
```

## Task 9: 实现预览画布、检查器和多轨时间线

**Files:**
- Create: `apps/desktop/src/renderer/features/editor/PreviewCanvas.tsx`, `Timeline.tsx`, `Inspector.tsx`, `SubtitleInspector.tsx`, `ResourceInspector.tsx`
- Create: `apps/desktop/src/renderer/features/editor/timelineMath.ts`
- Create: `apps/desktop/tests/editorInteractions.spec.ts`

**Interfaces:**
- Consumes: `Project`, `moveClip`, `resizeClip`, `copyClip`, `deleteClip`.
- Produces: `onSelect({ kind: 'text' | 'resource', clipId, elementId? })` and immutable project updates.

- [ ] **Step 1: 写预览选择、吸附和缩放失败测试**

```ts
await user.click(screen.getByLabelText('卡片 card-1'));
expect(screen.getByRole('heading', { name: '卡片资源' })).toBeVisible();
await user.click(screen.getByLabelText('关键词文本'));
expect(screen.getByRole('heading', { name: '文字属性' })).toBeVisible();
expect(snapToGuides({ x: 959, y: 200 }, guides, 8)).toMatchObject({ x: 960 });
expect(resizeWithAspectRatio(rect, 'se', { x: 220, y: 180 }, false)).toMatchObject({ width: 220, height: 123.75 });
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter desktop test editorInteractions.spec.ts`

Expected: FAIL because editor components are absent.

- [ ] **Step 3: 实现固定轨道、预览排版和直接操作**

底部展示主媒体、背景、字幕、卡片和图形轨道。拖拽片段移动位置，拖拽两端调整持续时间；按钮支持添加、替换、删除和复制。预览画布中的资源有可拖拽外框和八向缩放控制点，默认锁定宽高比，按住 `Shift` 才自由缩放。`timelineMath.ts` 定义画布中心、组件边缘/中心和双卡/三卡布局的顶部、垂直中心、底部、等间距参考线；在 8 画布单位阈值内吸附。双卡/三卡中拖动一个自动布局组件时，基于画布可用区域重新分配同组组件的均匀间距；手动布局只吸附，不移动其他组件。选中资源外框时显示同类资源及预览；选中文字时显示内容、换行、字体、字号、颜色、文字背景和当前元素/轨道/页面/全项目范围。

- [ ] **Step 4: 运行 UI 和端到端测试**

Run: `pnpm --filter desktop test editorInteractions.spec.ts && pnpm --filter desktop e2e`

Expected: PASS；纯音频工程显示默认背景，字幕全局样式和局部覆盖都反映在预览。

- [ ] **Step 5: 提交**

```bash
git add apps/desktop/src/renderer apps/desktop/tests
git commit -m "feat: add visual editor and timeline interactions"
```

## Task 10: 实现 FFmpeg 手动导出、进度和取消

**Files:**
- Create: `packages/media/src/exportCommand.ts`, `packages/media/src/exportProgress.ts`, `packages/media/tests/exportCommand.test.ts`
- Create: `apps/desktop/src/main/exportService.ts`
- Create: `apps/desktop/src/renderer/features/export/ExportDialog.tsx`
- Create: `apps/desktop/tests/exportService.test.ts`

**Interfaces:**
- Produces: `startExport(request, onProgress): ExportJob`, `ExportProgress`, `cancelExport(jobId)`.

- [ ] **Step 1: 写导出命令和进度失败测试**

```ts
expect(buildExportCommand({ format: 'mp4', codec: 'hevc', width: 1920, height: 1080 })).toContain('-c:v libx265');
expect(parseFfmpegProgress('frame=  120 fps=30.0 time=00:00:04.00')).toMatchObject({ frame: 120 });
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/media test exportCommand.test.ts`

Expected: FAIL because export command builder is absent.

- [ ] **Step 3: 实现导出预设和安全取消**

导出面板提供 16:9/9:16、720p/1080p/1440p/4K/自定义宽高、高质量/均衡/紧凑、目录和文件名。普通输出使用 MP4/HEVC/AAC；透明候选输出使用 MOV/HEVC + Alpha。FFmpeg 以 `-progress pipe:1` 报告阶段、帧数、百分比、速度和 ETA；取消发送终止信号并删除精确的 partial 输出路径。

- [ ] **Step 4: 运行导出测试和代表性渲染**

Run: `pnpm --filter @ai-video/media test exportCommand.test.ts && pnpm --filter desktop test exportService.test.ts`

Expected: PASS；720p 测试工程导出到用户指定目录，取消不留下 partial 文件。

- [ ] **Step 5: 执行剪映透明兼容性人工验收**

在目标版本剪映 macOS 与 Windows 中分别导入输出 MOV，检查 Alpha、边缘、时长和音画同步，并记录版本与结果到 `docs/compatibility/jianying-hevc-alpha.md`。任一失败时，导出界面保持“实验性”标签。

- [ ] **Step 6: 提交**

```bash
git add packages/media apps/desktop/src/main apps/desktop/src/renderer apps/desktop/tests docs/compatibility
git commit -m "feat: add manual HEVC export workflow"
```

## Task 11: 实现授权客户端和单会话本地策略

**Files:**
- Create: `packages/licensing/src/deviceIdentity.ts`, `entitlementCache.ts`, `sessionGuard.ts`, `index.ts`
- Create: `packages/licensing/tests/sessionGuard.test.ts`
- Create: `apps/desktop/src/main/licensingService.ts`
- Create: `apps/desktop/tests/licensingService.test.ts`

**Interfaces:**
- Produces: `getDeviceIdentity()`, `validateEntitlement()`, `canUseAi()`, `canEditOffline()`.

- [ ] **Step 1: 写两小时缓存和 AI 强制校验失败测试**

```ts
expect(canEditOffline(cache, nowPlus(119 * 60_000))).toBe(true);
expect(canEditOffline(cache, nowPlus(121 * 60_000))).toBe(false);
await expect(canUseAi(expiredCache, client)).resolves.toBe(false);
expect(client.validate).toHaveBeenCalledTimes(1);
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pnpm --filter @ai-video/licensing test sessionGuard.test.ts`

Expected: FAIL because licensing package is absent.

- [ ] **Step 3: 实现设备哈希、密钥和授权缓存**

从 macOS、Windows 的硬件序列号来源构建不可逆哈希，并结合系统安全存储生成的设备密钥。缓存服务端签名授权两小时。AI 请求先检查有效授权缓存；缓存过期时才在线验证订阅。本地编辑仅在有效窗口中允许。服务器声明会话被替换时，阻断当前客户端的后续 AI 与关键操作。

- [ ] **Step 4: 运行授权测试**

Run: `pnpm --filter @ai-video/licensing test sessionGuard.test.ts && pnpm --filter desktop test licensingService.test.ts`

Expected: PASS；缓存期内不重复订阅校验；缓存过期时 AI 校验失败则不触发模型调用。

- [ ] **Step 5: 提交**

```bash
git add packages/licensing apps/desktop/src/main apps/desktop/tests
git commit -m "feat: add licensing client and offline guard"
```

## Task 12: 交付打包、端到端验收和文档

**Files:**
- Create: `apps/desktop/electron-builder.yml`
- Create: `docs/installation.md`, `docs/operations.md`, `docs/compatibility/jianying-hevc-alpha.md`
- Modify: `README.md`
- Create: `apps/desktop/tests/fullWorkflow.e2e.ts`

**Interfaces:**
- Consumes: all desktop IPC APIs and a mock OpenAI-compatible endpoint.
- Produces: signed/notarized-ready macOS and Windows packaging configuration, documented verification results.

- [ ] **Step 1: 写完整流程失败测试**

```ts
test('creates, analyzes, edits, saves and exports a project', async ({ page }) => {
  await createFixtureProject(page);
  await expect(page.getByText('初版时间线已生成')).toBeVisible();
  await editKeyword(page, '新的关键词');
  await exportToFixtureDirectory(page);
  await expect(page.getByText('导出完成')).toBeVisible();
});
```

- [ ] **Step 2: 运行端到端测试确认失败**

Run: `pnpm --filter desktop e2e fullWorkflow.e2e.ts`

Expected: FAIL until all feature paths are wired.

- [ ] **Step 3: 配置打包和完整流程**

打包 Node/Python/FFmpeg 运行时，保留 macOS 和 Windows 的显式路径配置。文档写明本地模型首次下载、工作目录备份、资源包导入、模型 API 配置、导出格式、剪映透明验证和授权离线边界。

- [ ] **Step 4: 运行所有检查**

Run: `pnpm lint && pnpm typecheck && pnpm test && pnpm --filter desktop e2e && pnpm --filter desktop package`

Expected: PASS；生成 macOS 与 Windows 的安装包构建产物，完整流程测试通过。

- [ ] **Step 5: 提交**

```bash
git add apps/desktop README.md docs
git commit -m "chore: package AI video editor desktop app"
```

## 实施前外部依赖

1. 授权服务需要用户确定账户系统、支付服务商与生产 API 域名；在此之前只能实现授权客户端、接口契约和测试服务器，不能上线真实试用或收款。
2. 剪映透明输出必须由目标 macOS、Windows 剪映版本进行人工导入验收；不能以 FFmpeg 成功编码替代该验收。
3. faster-whisper 模型权重、Python 运行时和 FFmpeg 二进制需要随平台安装包交付或首次下载；不得以更小模型替代既定质量目标。
