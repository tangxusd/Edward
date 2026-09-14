# Edward 原生标注组件测试管道 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不使用 Component IR、转换层或第三方 NLE 的前提下，向“标注”tab加入 React、GSAP、HTML/CSS、SVG 四种本地矩形组件，并验证时间线插入、属性编辑、预览和逐帧导出一致。

**Architecture:** 每个组件保留自己的原生源文件和运行时入口。时间线片段只保存 `resourceId`、`runtime`、`source`、时间范围和原生 `props`；组件运行时直接接收局部时间，预览和导出调用同一套 `mount/update/unmount`。manifest 只负责入口和参数描述，不负责格式转换。

**Tech Stack:** FableCut 原生 JavaScript/DOM/SVG、React/ReactDOM 本地浏览器构建、GSAP 本地浏览器构建、Node.js 内置测试、现有 Canvas + DOM 合成导出器。

## Global Constraints

- 严禁使用 Component IR、理解层、转换层或任何等价中间表示。
- 严禁使用 OpenShot、DaVinci Resolve Studio、Premiere 及其 API、插件、脚本或通信方式。
- FableCut 运行时继续保持零网络运行依赖；React、ReactDOM、GSAP 以项目内固定文件提供。
- 组件动画必须由时间线局部时间驱动，禁止使用墙钟时间、随机值或外部网络资源。
- 预览和导出必须使用相同组件入口、相同参数快照和相同局部时间采样。
- 只修改本地“标注”测试资源与 FableCut 组件管道，不接云端管理员发布。

---

### Task 1: 固定本地运行时依赖和组件入口合同

**Files:**
- Create: `third_party/FableCut/vendor/react/react.production.min.js`
- Create: `third_party/FableCut/vendor/react/react-dom.production.min.js`
- Create: `third_party/FableCut/vendor/gsap/gsap.min.js`
- Create: `third_party/FableCut/vendor/runtime-manifest.json`
- Modify: `third_party/FableCut/index.html`
- Modify: `third_party/FableCut/server.js`
- Modify: `third_party/FableCut/component-runtime.js`
- Test: `third_party/FableCut/test/native-annotation-runtime.test.js`

**Interfaces:**
- Produces `window.React`, `window.ReactDOM` and `window.gsap` from local files only.
- Produces `loadManifest(id)` returning a manifest with `entry`, `runtime` and `props`.
- Requires each component module to export `mount({host, props, time, mode, viewport})` and return `{update, destroy}`.

- [ ] **Step 1: Write the failing dependency and manifest tests**

```js
test("native runtimes are local files and four component contracts are explicit", () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, "vendor/runtime-manifest.json"), "utf8"));
  for (const key of ["react", "react-dom", "gsap"]) {
    assert.match(manifest[key].path, /^vendor\//);
    assert.ok(manifest[key].version);
  }
});
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `node --test third_party/FableCut/test/native-annotation-runtime.test.js`

Expected: FAIL because the local runtime manifest and test fixtures do not exist.

- [ ] **Step 3: Pin and copy the browser builds locally**

Use npm package archives, record the exact versions and SHA-256 values in `vendor/runtime-manifest.json`, and copy only browser runtime files into the repository. Do not load a CDN URL and do not add a runtime `node_modules` lookup.

- [ ] **Step 4: Load runtimes before component modules**

Add local `<script defer>` tags in `index.html` before `app.js`. In `server.js`, keep `/vendor/*` under `APP_DIR` and reject path traversal. In `component-runtime.js`, reject manifests whose runtime is not one of `react`, `gsap`, `html-css`, or `svg`, and reject an entry outside the component directory.

- [ ] **Step 5: Run the focused test and verify it passes**

Run: `node --test third_party/FableCut/test/native-annotation-runtime.test.js`

Expected: PASS for local dependency paths, manifest loading constraints, and the mount/update/destroy contract.

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/vendor third_party/FableCut/index.html third_party/FableCut/server.js third_party/FableCut/component-runtime.js third_party/FableCut/test/native-annotation-runtime.test.js
git commit -m "feat: add local native component runtime contract"
```

### Task 2: Add the four native rectangle resources

**Files:**
- Create: `third_party/FableCut/components/annotation.rect.react/manifest.json`
- Create: `third_party/FableCut/components/annotation.rect.react/component.js`
- Create: `third_party/FableCut/components/annotation.rect.gsap/manifest.json`
- Create: `third_party/FableCut/components/annotation.rect.gsap/component.js`
- Create: `third_party/FableCut/components/annotation.rect.html-css/manifest.json`
- Create: `third_party/FableCut/components/annotation.rect.html-css/component.js`
- Create: `third_party/FableCut/components/annotation.rect.html-css/component.css`
- Create: `third_party/FableCut/components/annotation.rect.svg/manifest.json`
- Create: `third_party/FableCut/components/annotation.rect.svg/component.js`
- Test: `third_party/FableCut/test/native-annotation-resources.test.js`

**Interfaces:**
- Each manifest uses `runtime` matching its own source and declares `x`, `y`, `width`, `height`, `borderWidth`, `radius`, `color`, `progress`, and `duration`.
- Each module renders a horizontal, no-fill rectangle with `borderWidth=2`, `radius=5`, `color=#1683ff` defaults and computes `progress` from `time / duration` when the caller did not provide an explicit progress value.
- Each module's `update` accepts `(props, localTime, viewport)` and has no timer or network dependency.

- [ ] **Step 1: Write failing resource tests**

```js
for (const id of ["rect-react", "rect-gsap", "rect-html-css", "rect-svg"]) {
  test(`${id} exposes the direct runtime contract`, () => {
    const manifest = readManifest(id);
    assert.equal(manifest.category, "annotation");
    assert.deepEqual(Object.keys(manifest.props), REQUIRED_PROPS);
    assert.match(readEntry(id), /mount/);
    assert.doesNotMatch(readEntry(id), /ComponentIr|Resolve|Premiere|OpenShot/);
  });
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `node --test third_party/FableCut/test/native-annotation-resources.test.js`

Expected: FAIL because the four manifests and entries do not exist.

- [ ] **Step 3: Implement each native renderer**

React creates the rectangle as a React element and updates it with ReactDOM. GSAP creates a timeline and seeks it to `localTime` on every update. HTML/CSS creates a DOM element and writes CSS custom properties. SVG creates a native `<rect>` and writes `stroke-dashoffset`. All four must use identical normalized geometry and must keep the fill transparent.

- [ ] **Step 4: Add deterministic geometry and time assertions**

The test must assert default values, `t=0`, `t=duration/2`, and `t=duration` progress values, plus the exact border width, radius, color and no-fill declaration in each source.

- [ ] **Step 5: Run the test and verify it passes**

Run: `node --test third_party/FableCut/test/native-annotation-resources.test.js`

Expected: PASS for all four resource manifests and source-level contracts.

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/components/annotation.rect.* third_party/FableCut/test/native-annotation-resources.test.js
git commit -m "feat: add native annotation rectangle resources"
```

### Task 3: Register annotation resources and insert native clips

**Files:**
- Modify: `third_party/FableCut/server.js: component manifest route and safe component listing`
- Modify: `third_party/FableCut/app.js: component library state, annotation tab rendering, addComponent flow`
- Modify: `third_party/FableCut/index.html: annotation library labels if required`
- Test: `third_party/FableCut/test/user-components.test.js`
- Test: `third_party/FableCut/test/native-annotation-resources.test.js`

**Interfaces:**
- Produces `listNativeAnnotationResources()` returning the four manifests in stable order.
- Produces `addNativeComponent(resourceId)` creating a clip `{kind:"component", componentId: resourceId, runtime, source, start, duration, props}` at the playhead.
- The clip remains compatible with `syncDirectComponents(visibleClips, time, viewport)`.

- [ ] **Step 1: Add failing registration and insertion assertions**

```js
test("annotation library exposes four native resources", () => {
  const app = readApp();
  for (const id of ["annotation.rect.react", "annotation.rect.gsap", "annotation.rect.html-css", "annotation.rect.svg"])
    assert.match(app, new RegExp(id.replaceAll(".", "\\\\.")));
});
```

- [ ] **Step 2: Run focused tests and verify failure**

Run: `node --test third_party/FableCut/test/user-components.test.js third_party/FableCut/test/native-annotation-resources.test.js`

Expected: FAIL because the annotation tab still points only to the generic SVG/media directory and `addComponent()` always inserts `demo`.

- [ ] **Step 3: Register resources without changing generic media behavior**

Keep the existing `library/svg` path for ordinary SVG files. Add a dedicated local component list for `annotation`, render each resource with its runtime label and a preview button, and add an `＋` action that calls `addNativeComponent(resourceId)`.

- [ ] **Step 4: Insert a native clip with an immutable default snapshot**

Load the selected manifest, clone its defaults into `props`, store its `runtime` and `source`, use `track: "V2"`, `start: state.time`, and `duration: props.duration`, then select the new clip and redraw immediately.

- [ ] **Step 5: Run focused tests and verify pass**

Run: `node --test third_party/FableCut/test/user-components.test.js third_party/FableCut/test/native-annotation-resources.test.js`

Expected: PASS for stable annotation ordering and native clip insertion fields.

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/server.js third_party/FableCut/app.js third_party/FableCut/index.html third_party/FableCut/test/user-components.test.js third_party/FableCut/test/native-annotation-resources.test.js
git commit -m "feat: register native annotation components"
```

### Task 4: Bind native parameters to the property inspector

**Files:**
- Modify: `third_party/FableCut/app.js: renderInspector and inspector input handlers`
- Modify: `third_party/FableCut/style.css: component parameter controls if needed`
- Test: `third_party/FableCut/test/native-annotation-inspector.test.js`

**Interfaces:**
- Produces `renderNativeComponentInspector(clip, manifest)` using the manifest's declared fields only.
- Inspector changes call `updateNativeComponentProp(clip, key, value)`, validate against that manifest field, write the clip's native `props`, schedule save, and redraw.

- [ ] **Step 1: Write failing inspector tests**

```js
test("native annotation inspector exposes only declared editable props", () => {
  const app = readApp();
  assert.match(app, /renderNativeComponentInspector/);
  assert.match(app, /data-native-prop/);
  assert.match(app, /updateNativeComponentProp/);
});
```

- [ ] **Step 2: Run test and verify failure**

Run: `node --test third_party/FableCut/test/native-annotation-inspector.test.js`

Expected: FAIL because the current `kind === "component"` inspector is hard-coded for Card 6 fields.

- [ ] **Step 3: Replace only the component branch with manifest-driven direct bindings**

Keep the existing inspector for text/media/audio. For native components, render controls for position, size, border width, radius, color, progress and duration from the manifest; do not infer fields from source code. The input handler must reject values outside each declared min/max and retain the previous valid value.

- [ ] **Step 4: Update direct runtime immediately after a property change**

After writing `clip.props[key]`, call the mounted instance's `update` with the current local time, then call `scheduleSave()`, `renderInspector()` and `drawFrame(state.time)`. No conversion or intermediate object is created.

- [ ] **Step 5: Run test and verify pass**

Run: `node --test third_party/FableCut/test/native-annotation-inspector.test.js third_party/FableCut/test/user-components.test.js`

Expected: PASS for manifest-only fields, validation, immediate preview refresh, and persistence of the edited native props.

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/app.js third_party/FableCut/style.css third_party/FableCut/test/native-annotation-inspector.test.js
git commit -m "feat: expose native annotation props in inspector"
```

### Task 5: Make preview and export use the same native frame contract

**Files:**
- Modify: `third_party/FableCut/component-runtime.js: syncDirectComponents and captureCompositeFrame`
- Modify: `third_party/FableCut/app.js: drawFrame and export frame preparation`
- Modify: `third_party/FableCut/export-compositor.js: frame preparation contract`
- Test: `third_party/FableCut/test/native-annotation-render.test.js`

**Interfaces:**
- `syncDirectComponents(clips, time, viewport)` updates every active native clip with `time - clip.start` and the same viewport in preview/export.
- `prepareFrame(clips, time, viewport)` resolves only after every active native component update has completed and stale hosts are hidden or destroyed.
- `captureCompositeFrame(outputSpec)` captures the current prepared frame without introducing a crop or stale component.

- [ ] **Step 1: Write failing frame-contract tests**

```js
test("preview and export use identical local times", () => {
  const source = readRuntime();
  assert.match(source, /time - clip\.start/);
  assert.match(source, /mode/);
  assert.match(source, /prepareFrame/);
});
```

- [ ] **Step 2: Run test and verify failure**

Run: `node --test third_party/FableCut/test/native-annotation-render.test.js`

Expected: FAIL until the runtime explicitly passes the same local time and export mode through all four resource paths.

- [ ] **Step 3: Implement deterministic native frame preparation**

Pass `{mode: "preview"}` during normal playback and `{mode: "export"}` during export. Before each frame, hide all non-active component hosts, await all active updates, and fail the frame if an update rejects. Never reuse the previous frame's component host in an empty interval.

- [ ] **Step 4: Validate transparent fill, geometry and frame timing**

At 24, 30 and 60 fps, sample `t=0`, midpoint and end; assert the component host is absent outside its interval and that local time equals `frame / fps - start` within one frame duration.

- [ ] **Step 5: Run tests and verify pass**

Run: `node --test third_party/FableCut/test/native-annotation-render.test.js third_party/FableCut/test/export-compositor.test.js`

Expected: PASS with no stale layer, no implicit crop, and stable animation speed across FPS values.

- [ ] **Step 6: Commit**

```bash
git add third_party/FableCut/component-runtime.js third_party/FableCut/app.js third_party/FableCut/export-compositor.js third_party/FableCut/test/native-annotation-render.test.js
git commit -m "fix: align native component preview and export frames"
```

### Task 6: End-to-end validation and project documentation

**Files:**
- Modify: `third_party/FableCut/CLAUDE.md`
- Modify: `third_party/FableCut/README.md`
- Modify: `docs/operation-log/2026-09-13-vercel-auth-recovery.md`
- Test: `third_party/FableCut/test/native-annotation-e2e.test.js`

**Interfaces:**
- Documents the local annotation resources, direct runtime contract, local dependency policy and validation commands.
- Produces a deterministic test report for all four resources.

- [ ] **Step 1: Write the failing end-to-end contract test**

```js
test("all annotation resources satisfy the complete local pipeline contract", () => {
  for (const id of IDS) {
    const m = readManifest(id);
    assert.equal(m.category, "annotation");
    assert.ok(m.entry && m.runtime && m.props);
    assert.ok(m.preview === true && m.export === true);
  }
});
```

- [ ] **Step 2: Run the complete focused suite and record failures**

Run: `node --test third_party/FableCut/test/native-annotation-*.test.js third_party/FableCut/test/export-compositor.test.js`

Expected: FAIL only until all earlier tasks are complete; after implementation every test passes.

- [ ] **Step 3: Update FableCut documentation and operation log**

Document the four resource IDs, the local runtime dependency rule, the manifest fields, and the exact test commands. Explicitly state that this path does not use Component IR or external NLEs.

- [ ] **Step 4: Run repository checks**

Run:

```bash
node --check third_party/FableCut/server.js
node --check third_party/FableCut/app.js
node --check third_party/FableCut/component-runtime.js
node --test third_party/FableCut/test/native-annotation-*.test.js third_party/FableCut/test/export-compositor.test.js
cmake --build build --target edward_app -j2
git diff --check
```

Expected: all commands exit with status 0. If Qt is unavailable, record the exact environment error instead of claiming a successful build.

- [ ] **Step 5: Commit**

```bash
git add third_party/FableCut/CLAUDE.md third_party/FableCut/README.md third_party/FableCut/test/native-annotation-e2e.test.js docs/operation-log/2026-09-13-vercel-auth-recovery.md
git commit -m "test: verify native annotation preview and export pipeline"
```
