# Settings Dialog Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move project workspace and cloud-model configuration into a dismissible settings dialog opened from the top bar.

**Architecture:** Add one renderer-owned `SettingsDialog` shell with local open and active-tab state. Reuse `WorkspaceSettings` and `ModelSettings` for their existing persistence logic, but make each component render only its settings content. Keep the export dialog independent and keep library, project history, and project creation in the left column.

**Tech Stack:** Electron, React, TypeScript, existing Playwright Electron smoke tests, native CSS.

## Global Constraints

- Do not add dependencies.
- Keep export options exclusively in the export dialog.
- Keep the resource library in the editor left column; keep new-project and history panels on the home screen.
- Entering a project must not expose the workspace or project-history panels in the editor.

---

### Task 1: Lock The Settings Dialog Contract

**Files:**
- Modify: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**
- Consumes: top-bar `button` named `设置`.
- Produces: test expectations for `dialog` named `设置`, `通用` and `AI 模型` tabs, and closing behavior.

- [ ] **Step 1: Write the failing test**

```ts
test('opens settings from the top bar and switches settings tabs', async () => {
  const app = await electron.launch({ args: ['.'] });
  try {
    const page = await app.firstWindow();
    await expect(page.getByRole('dialog', { name: '设置' })).toHaveCount(0);
    await page.getByRole('button', { name: '设置' }).click();
    await expect(page.getByRole('dialog', { name: '设置' })).toBeVisible();
    await expect(page.getByLabel('工作目录路径')).toBeVisible();
    await page.getByRole('tab', { name: 'AI 模型' }).click();
    await expect(page.getByLabel('模型设置')).toBeVisible();
    await page.getByRole('button', { name: '关闭设置' }).click();
    await expect(page.getByRole('dialog', { name: '设置' })).toHaveCount(0);
  } finally {
    await app.close();
  }
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pnpm --filter desktop run build && pnpm --filter desktop exec playwright test tests/smoke.spec.ts --grep "opens settings from the top bar"`

Expected: FAIL because no `设置` button or dialog exists.

- [ ] **Step 3: Commit the failing test**

```bash
git add apps/desktop/tests/smoke.spec.ts
git commit -m "test: define settings dialog behavior"
```

### Task 2: Create The Dialog Shell And Migrate Settings Content

**Files:**
- Create: `apps/desktop/src/renderer/SettingsDialog.tsx`
- Modify: `apps/desktop/src/renderer/main.tsx`
- Modify: `apps/desktop/src/renderer/WorkspaceSettings.tsx`
- Modify: `apps/desktop/src/renderer/ModelSettings.tsx`

**Interfaces:**
- Consumes: `WorkspaceSettings({ onChanged?: () => void })` and `ModelSettings({ onModelsChanged?: (models: ModelRecord[]) => void })`.
- Produces: `SettingsDialog({ onWorkspaceChanged?: () => void; onModelsChanged?: (models: ModelRecord[]) => void })`.

- [ ] **Step 1: Make settings components content-only**

```tsx
// WorkspaceSettings.tsx
return <section aria-label="工作目录"><h2>项目工作目录</h2>{/* existing controls */}</section>;

// ModelSettings.tsx
return <section aria-label="模型设置"><h2>云端模型</h2>{/* existing controls */}</section>;
```

Remove the `project-opened` and `project-closed` visibility state from `WorkspaceSettings`; the dialog shell owns when these controls render.

- [ ] **Step 2: Create the modal shell with tab state**

```tsx
export function SettingsDialog({ onWorkspaceChanged, onModelsChanged }: Props): React.JSX.Element {
  const [open, setOpen] = useState(false);
  const [tab, setTab] = useState<'general' | 'models'>('general');
  return <>
    <button type="button" className="settings-trigger" onClick={() => setOpen(true)}>设置</button>
    {open ? <div role="dialog" aria-label="设置" aria-modal="true">
      <section>
        <header><h2>设置</h2><button aria-label="关闭设置" onClick={() => setOpen(false)}>关闭</button></header>
        <div role="tablist" aria-label="设置分类">
          <button role="tab" aria-selected={tab === 'general'} onClick={() => setTab('general')}>通用</button>
          <button role="tab" aria-selected={tab === 'models'} onClick={() => setTab('models')}>AI 模型</button>
        </div>
        {tab === 'general' ? <WorkspaceSettings onChanged={onWorkspaceChanged} /> : <ModelSettings onModelsChanged={onModelsChanged} />}
      </section>
    </div> : null}
  </>;
}
```

- [ ] **Step 3: Replace direct settings panel rendering in `main.tsx`**

```tsx
<header>
  {/* existing title, undo/redo, and model status */}
</header>
<SettingsDialog onWorkspaceChanged={workspaceChanged} onModelsChanged={setModels} />
```

Remove the direct `<WorkspaceSettings />` and `<ModelSettings />` children from `App`.

- [ ] **Step 4: Run the dialog test**

Run: `pnpm --filter desktop run build && pnpm --filter desktop exec playwright test tests/smoke.spec.ts --grep "opens settings from the top bar"`

Expected: PASS.

- [ ] **Step 5: Commit the implementation**

```bash
git add apps/desktop/src/renderer/SettingsDialog.tsx apps/desktop/src/renderer/main.tsx apps/desktop/src/renderer/WorkspaceSettings.tsx apps/desktop/src/renderer/ModelSettings.tsx apps/desktop/tests/smoke.spec.ts
git commit -m "feat: move app settings into dialog"
```

### Task 3: Style And Verify The Settings Dialog

**Files:**
- Modify: `apps/desktop/src/renderer/theme.css`
- Test: `apps/desktop/tests/smoke.spec.ts`

**Interfaces:**
- Consumes: `SettingsDialog` roles and labels from Task 2.
- Produces: a centered, non-overlapping settings dialog and a right-aligned top-bar trigger.

- [ ] **Step 1: Add dialog and tab styles**

```css
.settings-trigger { position: fixed; top: 9px; right: 88px; }
[role="dialog"][aria-label="设置"] { position: fixed; z-index: 40; inset: 0; display: grid; place-items: center; }
[role="dialog"][aria-label="设置"] > section { width: min(640px, calc(100vw - 40px)); }
[role="tablist"] { display: flex; border-bottom: 1px solid #252a32; }
[role="tab"][aria-selected="true"] { color: #fff; border-bottom-color: #3d8bfd; }
```

Remove the old left-column positioning rules for `工作目录` and `模型设置`.

- [ ] **Step 2: Run focused persistence tests**

Run: `pnpm --filter desktop exec playwright test tests/smoke.spec.ts --grep "opens settings from the top bar|saves a named cloud model|workspace validation|reloads saved models"`

Expected: PASS.

- [ ] **Step 3: Run the complete renderer smoke suite and build**

Run: `pnpm --filter desktop run build && pnpm --filter desktop exec playwright test tests/smoke.spec.ts`

Expected: PASS.

- [ ] **Step 4: Commit styling and verification changes**

```bash
git add apps/desktop/src/renderer/theme.css apps/desktop/tests/smoke.spec.ts
git commit -m "style: present app settings as modal dialog"
```

## Self-Review

- Spec coverage: Tasks 1-3 cover the top-bar entry, general and AI model tabs, reuse of existing persistence behavior, separation from export, and regression checks.
- Placeholder scan: no deferred implementation markers remain.
- Type consistency: `SettingsDialog` accepts the existing workspace and model callbacks, so `App` retains ownership of resource and model refresh state.
