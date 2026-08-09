import { _electron as electron, expect, test } from '@playwright/test';

test('exposes only the desktop bridge', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();

    await expect(page.locator('main')).toContainText('Edward');
    await expect(
      page.evaluate(() => (window as Window & { aiVideo?: unknown }).aiVideo),
    ).resolves.toBeDefined();
    await expect(
      page.evaluate(() => Object.keys((window as unknown as Window & { aiVideo: { projects: unknown } }).aiVideo)),
    ).resolves.toEqual(['workspace', 'projects', 'library', 'models', 'analysis', 'componentChat', 'export']);
    await expect(
      page.evaluate(() => Object.keys((window as unknown as Window & { aiVideo: { workspace: Record<string, unknown> } }).aiVideo.workspace)),
    ).resolves.toEqual(['setRoot', 'chooseDirectory', 'chooseFile', 'getRoot']);
    await expect(
      page.evaluate(() => Object.keys((window as unknown as Window & { aiVideo: { analysis: Record<string, unknown> } }).aiVideo.analysis)),
    ).resolves.toEqual(['generate', 'apply', 'list']);
    await expect(
      page.evaluate(() => (window as Window & { require?: unknown }).require),
    ).resolves.toBeUndefined();
  } finally {
    await app.close();
  }
});

test('opens settings from the top bar and switches settings tabs', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await expect(page.getByRole('dialog', { name: '设置' })).toHaveCount(0);
    await page.getByRole('button', { name: '设置' }).click();
    const settingsDialog = page.getByRole('dialog', { name: '设置' });
    const settingsTrigger = page.getByRole('button', { name: '设置', exact: true });
    await expect(settingsDialog).toBeVisible();
    const generalTab = settingsDialog.getByRole('tab', { name: '通用' });
    const modelsTab = settingsDialog.getByRole('tab', { name: 'AI 模型' });
    await expect(generalTab).toBeFocused();
    await expect(generalTab).toHaveAttribute('tabindex', '0');
    await expect(modelsTab).toHaveAttribute('tabindex', '-1');
    await expect(settingsTrigger).toHaveCSS('position', 'fixed');
    await expect(settingsTrigger).toHaveCSS('top', '9px');
    await expect(settingsTrigger).toHaveCSS('right', '88px');
    await expect(settingsDialog).toHaveCSS('position', 'fixed');
    await expect(settingsDialog).toHaveCSS('display', 'grid');
    await expect(settingsDialog).toHaveCSS('place-items', 'center');
    const settingsDialogBody = settingsDialog.locator(':scope > section');
    const settingsDialogGeometry = await settingsDialogBody.evaluate((element) => ({
      height: element.getBoundingClientRect().height,
      left: element.getBoundingClientRect().left,
      top: element.getBoundingClientRect().top,
      viewportHeight: window.innerHeight,
      viewportWidth: window.innerWidth,
      width: element.getBoundingClientRect().width,
    }));
    expect(Math.abs(settingsDialogGeometry.left - (settingsDialogGeometry.viewportWidth - settingsDialogGeometry.width) / 2)).toBeLessThanOrEqual(1);
    expect(Math.abs(settingsDialogGeometry.top - (settingsDialogGeometry.viewportHeight - settingsDialogGeometry.height) / 2)).toBeLessThanOrEqual(1);
    expect(Math.abs(settingsDialogGeometry.width - Math.min(640, settingsDialogGeometry.viewportWidth - 40))).toBeLessThanOrEqual(1);
    await expect(settingsDialog.locator('[role="tablist"]')).toHaveCSS('display', 'flex');
    await expect(settingsDialog.getByRole('tab', { name: '通用' })).toHaveCSS('color', 'rgb(255, 255, 255)');
    await expect(settingsDialog.getByRole('tab', { name: '通用' })).toHaveCSS('border-bottom-color', 'rgb(61, 139, 253)');
    await expect(page.getByLabel('工作目录路径')).toBeVisible();
    await expect(generalTab).toHaveAttribute('id', 'settings-tab-general');
    await expect(generalTab).toHaveAttribute('aria-controls', 'settings-panel-general');
    await expect(modelsTab).toHaveAttribute('id', 'settings-tab-models');
    await expect(modelsTab).toHaveAttribute('aria-controls', 'settings-panel-models');
    await generalTab.press('ArrowRight');
    await expect(modelsTab).toHaveAttribute('aria-selected', 'true');
    await expect(generalTab).toHaveAttribute('tabindex', '-1');
    await expect(modelsTab).toHaveAttribute('tabindex', '0');
    await expect(modelsTab).toBeFocused();
    await expect(page.getByLabel('模型设置')).toBeVisible();
    await modelsTab.press('ArrowRight');
    await expect(generalTab).toHaveAttribute('aria-selected', 'true');
    await expect(generalTab).toBeFocused();
    await modelsTab.click();
    const closeButton = settingsDialog.getByRole('button', { name: '关闭设置' });
    const lastFocusable = settingsDialog.locator('button:not([disabled]):not([tabindex="-1"]), input:not([disabled])').last();
    await lastFocusable.focus();
    await page.keyboard.press('Tab');
    await expect(closeButton).toBeFocused();
    await page.keyboard.press('Shift+Tab');
    await expect(lastFocusable).toBeFocused();
    await closeButton.click();
    await expect(page.getByRole('dialog', { name: '设置' })).toHaveCount(0);
    await expect(settingsTrigger).toBeFocused();
    await settingsTrigger.click();
    await expect(page.getByRole('tab', { name: 'AI 模型' })).toBeFocused();
  } finally {
    await app.close();
  }
});

test('hides the resource library on the home screen and shows it in the editor', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await expect(page.locator('.home-card')).toBeVisible();
    await expect(page.getByRole('button', { name: '新建项目' })).toBeVisible();
    await expect(page.getByLabel('资源库')).toBeHidden();

    await page.getByRole('button', { name: '新建项目' }).click();
    await expect(page.locator('.home-modal')).toBeVisible();
    await page.locator('.home-modal-close').click();
    await expect(page.locator('.home-modal')).toHaveCount(0);
  } finally {
    await app.close();
  }
});

test('returns to the home screen after changing the workspace from an open project', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/workspace-state-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/workspace-state-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('资源库')).toBeVisible();

    await page.getByRole('button', { name: '设置' }).click();
    const workspace = page.getByRole('region', { name: '工作目录' });
    await workspace.getByLabel('工作目录路径').fill(`/tmp/workspace-state-${Date.now()}`);
    await workspace.getByRole('button', { name: '应用' }).click();
    await page.getByRole('button', { name: '关闭设置' }).click();

    await expect(page.getByLabel('项目列表')).toBeVisible();
    await expect(page.getByLabel('新建项目')).toBeVisible();
    await expect(page.getByLabel('资源库')).toHaveCount(0);
  } finally {
    await app.close();
  }
});

test('closes an open project and returns to the home screen', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/close-project-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/close-project-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('资源库')).toBeVisible();
    await expect(page.getByLabel('项目名称')).toBeVisible();
    await expect(page.getByLabel('保存状态')).toBeVisible();
    await page.getByRole('button', { name: '关闭项目' }).click();
    await expect(page.getByLabel('项目列表')).toBeVisible();
    await expect(page.getByLabel('新建项目')).toBeVisible();
    await expect(page.getByLabel('项目名称')).toHaveCount(0);
    await expect(page.getByLabel('保存状态')).toHaveCount(0);
  } finally {
    await app.close();
  }
});

test('shows the project save status in the editor header', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/save-status-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/save-status-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('保存状态')).toHaveText('已保存');
  } finally {
    await app.close();
  }
});

test('contains editor panels inside the right inspector', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/inspector-container-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/inspector-container-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const inspector = page.getByLabel('资源检查器');
    await expect(inspector.getByLabel('AI时间线分析')).toBeVisible();
    await expect(inspector.getByLabel('字幕样式')).toBeVisible();
  } finally {
    await app.close();
  }
});

test('keeps the preview canvas out of the right inspector column', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-grid-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/preview-grid-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const preview = await page.getByLabel('预览画布').boundingBox();
    const inspector = await page.getByLabel('资源检查器').boundingBox();
    if (!preview || !inspector) throw new Error('editor panels are not visible');
    expect(preview.x + preview.width).toBeLessThanOrEqual(inspector.x);
  } finally {
    await app.close();
  }
});

test('shows the current project media name in the editor header', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/project-title-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/project-title-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    const projectName = page.getByLabel('项目名称');
    await expect(projectName).toHaveText(mediaPath.split('/').at(-1)!);
    await expect(projectName).toHaveCSS('left', '270px');
  } finally {
    await app.close();
  }
});

test('resizes the left editor panel by dragging its divider', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/panel-resize-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/panel-resize-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const initialWidth = await page.evaluate(() => Number.parseInt(getComputedStyle(document.documentElement).getPropertyValue('--left-panel-width'), 10));
    await page.mouse.move(initialWidth, 300);
    await page.mouse.down();
    await page.mouse.move(initialWidth - 40, 300);
    await page.mouse.up();
    await expect.poll(() => page.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--left-panel-width'))).toBe(`${initialWidth - 40}px`);
  } finally {
    await app.close();
  }
});

test('resizes the right editor panel by dragging its divider', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/right-panel-resize-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/right-panel-resize-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const viewportWidth = await page.evaluate(() => window.innerWidth);
    const initialWidth = await page.evaluate(() => Number.parseInt(getComputedStyle(document.documentElement).getPropertyValue('--right-panel-width'), 10));
    await page.mouse.move(viewportWidth - initialWidth, 300);
    await page.mouse.down();
    await page.mouse.move(viewportWidth - initialWidth + 40, 300);
    await page.mouse.up();
    await expect.poll(() => page.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--right-panel-width'))).toBe(`${initialWidth - 40}px`);
  } finally {
    await app.close();
  }
});

test('opens the export dialog only from the header export button', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await expect(page.getByLabel('导出')).toHaveCount(0);
    await expect(page.getByLabel('分辨率')).toHaveCount(0);
    const exportTrigger = page.getByRole('button', { name: '导出' });
    await expect(exportTrigger).toHaveCount(1);
    await expect(exportTrigger).toHaveCSS('position', 'fixed');
    await expect(exportTrigger).toHaveCSS('right', '14px');
    await exportTrigger.click();
    const exportDialog = page.getByRole('dialog', { name: '导出' });
    await expect(exportDialog).toHaveCount(1);
    await expect(exportDialog).toBeVisible();
    await expect(exportDialog.getByLabel('分辨率')).toBeVisible();
    const closeButton = exportDialog.getByRole('button', { name: '关闭导出' });
    const lastFocusable = exportDialog.locator('button:not([disabled]), input:not([disabled]), select:not([disabled])').last();
    await expect(closeButton).toBeFocused();
    await lastFocusable.focus();
    await page.keyboard.press('Tab');
    await expect(closeButton).toBeFocused();
    await page.keyboard.press('Shift+Tab');
    await expect(lastFocusable).toBeFocused();
    await closeButton.click();
    await expect(page.getByLabel('导出')).toHaveCount(0);
    await expect(page.getByLabel('分辨率')).toHaveCount(0);
    await expect(exportTrigger).toBeFocused();
    await exportTrigger.click();
    await expect(closeButton).toBeFocused();
    await page.keyboard.press('Escape');
    await expect(page.getByRole('dialog', { name: '导出' })).toHaveCount(0);
    await expect(exportTrigger).toBeFocused();
  } finally {
    await app.close();
  }
});

test('locks ordinary quality controls when transparent export is selected', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByRole('button', { name: '导出' }).click();
    const exportDialog = page.getByRole('dialog', { name: '导出' });
    const quality = exportDialog.getByLabel('质量');
    await expect(quality).toBeEnabled();
    await exportDialog.getByRole('checkbox').check();
    await expect(quality).toBeDisabled();
    await expect(exportDialog).toContainText('ProRes 4444 MOV');
  } finally {
    await app.close();
  }
});

test('moves a timeline clip by dragging it', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/script.txt');
    await page.getByLabel('音频或视频').fill('/tmp/source.mp3');
    await page.getByRole('button', { name: '创建项目' }).click();

    const clip = page.getByLabel('多轨时间线').locator('div[role="button"]').filter({ hasText: 'main-media' });
    await clip.scrollIntoViewIfNeeded();
    const before = await clip.boundingBox();
    if (!before) throw new Error('timeline clip is not visible');
    await page.mouse.move(before.x + 10, before.y + 10);
    await page.mouse.down();
    await page.mouse.move(before.x + 70, before.y + 10);
    await page.mouse.up();

    await expect(clip).toHaveCSS('margin-left', '60px');
  } finally {
    await app.close();
  }
});

test('resizes a timeline clip by dragging its duration control', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/resize-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/resize-script.txt');
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();

    const clip = page.getByLabel('多轨时间线').locator('div[role="button"]').filter({ hasText: 'main-media' });
    const durationControl = page.getByLabel('调整 main-media 时长');
    await clip.scrollIntoViewIfNeeded();
    const before = await clip.boundingBox();
    const control = await durationControl.boundingBox();
    if (!before || !control) throw new Error('timeline duration control is not visible');
    await page.mouse.move(control.x + control.width / 2, control.y + control.height / 2);
    await page.mouse.down();
    await page.mouse.move(control.x + control.width / 2 + 60, control.y + control.height / 2);
    await page.mouse.up();

    await expect(clip).toHaveCSS('width', '90px');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.mainMedia.clips[0]?.duration;
    }, mediaPath)).toBe(1.5);
  } finally {
    await app.close();
  }
});

test('uses the compositing order in the timeline after opening a project', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/order-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/order-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();

    await expect(page.getByLabel('项目列表')).toHaveCount(0);
    await expect(page.getByLabel('工作目录')).toHaveCount(0);
    await expect(page.getByLabel('新建项目')).toHaveCount(0);
    await expect(page.getByLabel('多轨时间线').locator(':scope > div').evaluateAll((tracks) => tracks.map((track) => track.getAttribute('aria-label')))).resolves.toEqual(['图形轨道', '卡片轨道', '字幕轨道', '背景轨道', '主媒体轨道']);
  } finally {
    await app.close();
  }
});

test('undoes and redoes a manual timeline edit', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/history-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/history-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();

    const clip = page.getByLabel('多轨时间线').locator('div[role="button"]').filter({ hasText: 'main-media' });
    await clip.scrollIntoViewIfNeeded();
    const before = await clip.boundingBox();
    if (!before) throw new Error('timeline clip is not visible');
    await page.mouse.move(before.x + 10, before.y + 10);
    await page.mouse.down();
    await page.mouse.move(before.x + 70, before.y + 10);
    await page.mouse.up();
    await expect(clip).toHaveCSS('margin-left', '60px');

    await page.getByRole('button', { name: '撤销' }).click();
    await expect(clip).toHaveCSS('margin-left', '0px');
    await page.getByRole('button', { name: '重做' }).click();
    await expect(clip).toHaveCSS('margin-left', '60px');
  } finally {
    await app.close();
  }
});

test('renders the audio project default background in the preview canvas', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/preview-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('预览背景 default-background')).toBeVisible();
    await expect(page.getByLabel('主音频预览')).toBeAttached();
  } finally {
    await app.close();
  }
});

test('opens background resource choices from the audio background track', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/background-resource-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/background-resource-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('背景轨道').getByRole('button').filter({ hasText: 'default-background' }).click();
    await expect(page.getByLabel('背景资源')).toBeVisible();
    await expect(page.getByLabel('背景资源')).toContainText('暂无同类资源');
  } finally {
    await app.close();
  }
});

test('copies and deletes a background clip from the timeline', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/background-edit-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/background-edit-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const track = page.getByLabel('背景轨道');
    await track.getByRole('button', { name: '复制 default-background', exact: true }).click();
    await expect(track.getByRole('button').filter({ hasText: 'default-background-copy' })).toBeVisible();
    await track.getByRole('button', { name: '删除 default-background-copy', exact: true }).click();
    await expect(track.getByRole('button').filter({ hasText: 'default-background-copy' })).toHaveCount(0);
  } finally {
    await app.close();
  }
});

test('resizes an audio project background clip from the timeline', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/background-resize-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/background-resize-script.txt');
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();

    const clip = page.getByLabel('背景轨道').getByRole('button').filter({ hasText: 'default-background' });
    const control = page.getByLabel('调整 default-background 时长');
    const before = await control.boundingBox();
    if (!before) throw new Error('background duration control is not visible');
    await page.mouse.move(before.x + before.width / 2, before.y + before.height / 2);
    await page.mouse.down();
    await page.mouse.move(before.x + before.width / 2 + 60, before.y + before.height / 2);
    await page.mouse.up();

    await expect(clip).toHaveCSS('width', '90px');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.background.clips[0]?.duration;
    }, mediaPath)).toBe(1.5);
  } finally {
    await app.close();
  }
});

test('moves an audio project background clip from the timeline', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/background-move-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/background-move-script.txt');
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();

    const clip = page.getByLabel('背景轨道').getByRole('button').filter({ hasText: 'default-background' });
    const before = await clip.boundingBox();
    if (!before) throw new Error('background clip is not visible');
    await page.mouse.move(before.x + 12, before.y + 12);
    await page.mouse.down();
    await page.mouse.move(before.x + 72, before.y + 12);
    await page.mouse.up();

    await expect(clip).toHaveCSS('margin-left', '60px');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.background.clips[0]?.start;
    }, mediaPath)).toBe(1);
  } finally {
    await app.close();
  }
});

test('uses the selected project aspect ratio in preview', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/aspect-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/aspect-source-${Date.now()}.mp3`);
    await page.getByLabel('项目画幅').selectOption('9:16');
    await page.getByRole('button', { name: '创建项目' }).click();
    const preview = page.getByLabel('预览画布');
    await expect(preview).toHaveCSS('aspect-ratio', '9 / 16');
    const bounds = await preview.boundingBox();
    if (!bounds) throw new Error('preview canvas is not visible');
    expect(bounds.width / bounds.height).toBeCloseTo(9 / 16, 2);
  } finally {
    await app.close();
  }
});

test('renders the imported video as the main preview media', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/preview-video-source-${Date.now()}.mp4`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-video-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('主视频预览')).toHaveAttribute('src', `file://${mediaPath}`);
  } finally {
    await app.close();
  }
});

test('renders AI card clips from the project timeline in the preview canvas', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-card-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/preview-card-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '卡片预览', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '重点' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await expect(page.getByLabel('项目卡片 ai-cards-0')).toContainText('重点');
    await expect(page.getByLabel('卡片缩放控件 ai-cards-0')).toBeVisible();
    await page.getByLabel('项目卡片 ai-cards-0').click();
    await expect(page.getByLabel('项目卡片 ai-cards-0')).toHaveCSS('border-width', '2px');
    await expect(page.getByLabel('资源检查器')).toContainText('ai-cards-0');
    await expect(page.getByLabel('卡片资源')).toBeVisible();
    const previewBounds = await page.getByLabel('预览画布').boundingBox();
    if (!previewBounds) throw new Error('preview canvas is not visible');
    await page.getByLabel('预览画布').click({ position: { x: previewBounds.width - 40, y: previewBounds.height - 80 } });
    await expect(page.getByLabel('资源检查器')).toContainText('未选择资源');
  } finally {
    await app.close();
  }
});

test('scales a selected card from the inspector by percentage', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/preview-percent-scale-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-percent-scale-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '百分比缩放', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '可缩放卡片' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await page.getByLabel('项目卡片 ai-cards-0').click();

    const scale = page.getByLabel('缩放', { exact: true });
    await expect(scale).toHaveValue('100');
    await scale.fill('125');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips[0]?.layout;
    }, mediaPath)).toMatchObject({ width: 435, height: 225, scale: 125, baseWidth: 348, baseHeight: 180 });

    await scale.fill('50');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips[0]?.layout;
    }, mediaPath)).toMatchObject({ width: 174, height: 90, scale: 50 });

    await page.reload();
    await page.getByLabel('项目列表').locator('article').filter({ hasText: mediaPath }).getByRole('button', { name: '打开' }).click();
    await page.getByLabel('项目卡片 ai-cards-0').click();
    await expect(page.getByLabel('缩放', { exact: true })).toHaveValue('50');
  } finally {
    await app.close();
  }
});

test('shows component AI chat only for editable preview components', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/component-chat-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/component-chat-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '组件对话', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '卡片内容' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await page.getByLabel('项目卡片 ai-cards-0').click();
    await expect(page.getByLabel('AI修改组件')).toBeVisible();
    await expect(page.getByRole('button', { name: '发送给AI' })).toBeDisabled();
    await page.getByLabel('背景轨道').getByRole('button').filter({ hasText: 'default-background' }).click();
    await expect(page.getByLabel('AI修改组件')).toHaveCount(0);
  } finally {
    await app.close();
  }
});

test('applies only a validated component AI draft after confirmation', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/component-chat-apply-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/component-chat-apply-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '组件草案', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '原始内容' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      if (!project) throw new Error('project not found');
      await window.aiVideo.projects.save({ ...project, componentConversations: { 'ai-cards-0': { modelId: 'test-model', messages: [{ role: 'user', content: '修改文字' }, { role: 'assistant', content: '已生成草案' }], draftContent: { text: '修改后内容' }, updatedAt: '2026-08-08T00:00:00.000Z' } } });
    }, mediaPath);
    await page.reload();
    await page.getByLabel('项目列表').locator('article').filter({ hasText: mediaPath }).getByRole('button', { name: '打开' }).click();
    await page.getByLabel('项目卡片 ai-cards-0').click();
    await expect(page.getByLabel('组件草案状态')).toContainText('草案已通过格式校验');
    await expect(page.getByLabel('项目卡片 ai-cards-0')).toContainText('原始内容');
    await page.getByRole('button', { name: '应用修改' }).click();
    await expect(page.getByLabel('项目卡片 ai-cards-0')).toContainText('修改后内容');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips[0]?.content;
    }, mediaPath)).toEqual({ text: '修改后内容' });
  } finally {
    await app.close();
  }
});

test('blocks an incompatible component AI draft', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/component-chat-invalid-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/component-chat-invalid-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '错误草案', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '原始内容' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      if (!project) throw new Error('project not found');
      await window.aiVideo.projects.save({ ...project, componentConversations: { 'ai-cards-0': { modelId: 'test-model', messages: [{ role: 'user', content: '增加未知字段' }, { role: 'assistant', content: '错误草案' }], draftContent: { data: [1, 2] }, updatedAt: '2026-08-08T00:00:00.000Z' } } });
    }, mediaPath);
    await page.reload();
    await page.getByLabel('项目列表').locator('article').filter({ hasText: mediaPath }).getByRole('button', { name: '打开' }).click();
    await page.getByLabel('项目卡片 ai-cards-0').click();
    await expect(page.getByLabel('组件草案状态')).toContainText('草案格式不兼容');
    await expect(page.getByRole('button', { name: '应用修改' })).toBeDisabled();
  } finally {
    await app.close();
  }
});

test('renders graphical timeline clips in the preview canvas', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-graphic-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/preview-graphic-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '图形预览', clips: [{ track: 'graphics', start: 0, duration: 2, content: { type: 'timeline' }, styleId: 'timeline-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await expect(page.getByLabel('项目图形 ai-graphics-0')).toBeVisible();
    await page.getByLabel('项目图形 ai-graphics-0').click();
    await expect(page.getByLabel('资源检查器')).toContainText('ai-graphics-0');
  } finally {
    await app.close();
  }
});

test('renders an AI subtitle in the preview canvas for local text editing', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/preview-subtitle-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-subtitle-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '字幕预览', clips: [{ track: 'subtitles', start: 0, duration: 2, content: { text: '可编辑字幕' }, styleId: 'subtitle-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await page.getByLabel('项目字幕 ai-subtitles-0').click();
    await expect(page.getByLabel('文字内容')).toHaveValue('可编辑字幕');
    await page.getByLabel('局部字体').fill('PingFang SC');
    await page.getByLabel('局部字体').blur();
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      const content = project?.tracks.subtitles.clips[0]?.content as { textStyle?: { fontFamily?: string } };
      return content.textStyle?.fontFamily;
    }, mediaPath)).toBe('PingFang SC');
  } finally {
    await app.close();
  }
});

test('persists a dragged project card layout', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/preview-layout-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-layout-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '卡片布局', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '可移动卡片' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    const card = page.getByLabel('项目卡片 ai-cards-0');
    const before = await card.boundingBox();
    if (!before) throw new Error('project card is not visible');
    await page.mouse.move(before.x + 20, before.y + 20);
    await page.mouse.down();
    await page.mouse.move(before.x + 80, before.y + 50);
    await page.mouse.up();
    await card.click();
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips[0]?.layout?.x;
    }, mediaPath)).toBeGreaterThanOrEqual(120);
  } finally {
    await app.close();
  }
});

test('persists a resized project card layout', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/preview-resize-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/preview-resize-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '卡片缩放', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '可缩放卡片' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    const control = page.getByLabel('卡片缩放控件 ai-cards-0');
    const bounds = await control.boundingBox();
    if (!bounds) throw new Error('project card resize control is not visible');
    await page.mouse.move(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
    await page.mouse.down();
    await page.mouse.move(bounds.x + bounds.width / 2 + 80, bounds.y + bounds.height / 2);
    await page.mouse.up();
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips[0]?.layout?.width;
    }, mediaPath)).toBeGreaterThan(348);
  } finally {
    await app.close();
  }
});

test('shows a center guide while dragging a card into horizontal alignment', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/guide-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/guide-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '居中参考线', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '对齐卡片' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();

    const canvas = page.getByLabel('预览画布');
    const card = page.getByLabel('项目卡片 ai-cards-0');
    const canvasBounds = await canvas.boundingBox();
    const cardBounds = await card.boundingBox();
    if (!canvasBounds || !cardBounds) throw new Error('preview card is not visible');
    await page.mouse.move(cardBounds.x + 20, cardBounds.y + 20);
    await page.mouse.down();
    await page.mouse.move(canvasBounds.x + canvasBounds.width / 2 - cardBounds.width / 2 + 20, cardBounds.y + 20);
    await expect(page.getByLabel('垂直对齐参考线')).toBeVisible();
    await page.mouse.up();
  } finally {
    await app.close();
  }
});

test('shows a center guide while dragging a card into vertical alignment', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/vertical-guide-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/vertical-guide-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '垂直居中参考线', clips: [{ track: 'cards', start: 0, duration: 2, content: { text: '对齐卡片' }, styleId: 'card-style' }] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();

    const canvas = page.getByLabel('预览画布');
    const card = page.getByLabel('项目卡片 ai-cards-0');
    const canvasBounds = await canvas.boundingBox();
    const cardBounds = await card.boundingBox();
    if (!canvasBounds || !cardBounds) throw new Error('preview card is not visible');
    await page.mouse.move(cardBounds.x + 20, cardBounds.y + 20);
    await page.mouse.down();
    await page.mouse.move(cardBounds.x + 20, canvasBounds.y + canvasBounds.height / 2 - cardBounds.height / 2 + 20);
    await expect(page.getByLabel('水平对齐参考线')).toBeVisible();
    await page.mouse.up();
  } finally {
    await app.close();
  }
});

test('persists the two-card layout into project cards', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/two-card-layout-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/two-card-layout-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '双卡布局', clips: [
      { track: 'cards', start: 0, duration: 2, content: { text: '第一张' }, styleId: 'card-style' },
      { track: 'cards', start: 2, duration: 2, content: { text: '第二张' }, styleId: 'card-style' },
    ] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await expect(page.getByText('已应用到时间线')).toBeVisible();

    const twoCardButton = page.locator('[aria-label="卡片布局"] button').filter({ hasText: '双卡' });
    await expect(twoCardButton).toBeVisible();
    await twoCardButton.dispatchEvent('click');
    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips.map((clip) => clip.layout);
    }, mediaPath)).toEqual([
      { x: 120, y: 180, width: 348, height: 180 },
      { x: 492, y: 180, width: 348, height: 180 },
    ]);

    await page.reload();
    await page.getByLabel('项目列表').locator('article').filter({ hasText: mediaPath }).getByRole('button', { name: '打开' }).click();
    await expect(page.locator('[aria-label="卡片布局"] button').filter({ hasText: '双卡' })).toHaveAttribute('aria-pressed', 'true');
  } finally {
    await app.close();
  }
});

test('persists the three-card layout with inferred spacing', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/three-card-layout-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/three-card-layout-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await page.getByLabel('AI编辑计划').fill(JSON.stringify({ summary: '三卡布局', clips: [
      { track: 'cards', start: 0, duration: 2, content: { text: '第一张' }, styleId: 'card-style' },
      { track: 'cards', start: 2, duration: 2, content: { text: '第二张' }, styleId: 'card-style' },
      { track: 'cards', start: 4, duration: 2, content: { text: '第三张' }, styleId: 'card-style' },
    ] }));
    await page.getByRole('button', { name: '应用到时间线' }).click();
    await expect(page.getByText('已应用到时间线')).toBeVisible();
    await page.locator('[aria-label="卡片布局"] button').filter({ hasText: '三卡' }).dispatchEvent('click');

    await expect.poll(() => page.evaluate(async (path) => {
      const project = (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path);
      return project?.tracks.cards.clips.map((clip) => clip.layout);
    }, mediaPath)).toEqual([
      { x: 120, y: 180, width: 224, height: 180 },
      { x: 368, y: 180, width: 224, height: 180 },
      { x: 616, y: 180, width: 224, height: 180 },
    ]);

    await page.reload();
    await page.getByLabel('项目列表').locator('article').filter({ hasText: mediaPath }).getByRole('button', { name: '打开' }).click();
    await expect(page.locator('[aria-label="卡片布局"] button').filter({ hasText: '三卡' })).toHaveAttribute('aria-pressed', 'true');
  } finally {
    await app.close();
  }
});

test('persists subtitle style changes with the project', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/subtitle-style-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/subtitle-style-script.txt');
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('项目列表')).toHaveCount(0);
    await expect(page.getByLabel('工作目录')).toHaveCount(0);
    const projectId = await page.evaluate(async (path) => (await window.aiVideo.projects.list()).find((candidate) => candidate.media.path === path)?.id, mediaPath);
    if (!projectId) throw new Error('created project id is missing');
    const font = page.getByLabel('字体');
    await font.fill('PingFang SC');
    await font.blur();
    await expect.poll(() => page.evaluate((id) => window.aiVideo.projects.open(id).then((value) => value.subtitleStyle.fontFamily), projectId)).toBe('PingFang SC');
  } finally {
    await app.close();
  }

  const reopened = await electron.launch({ args: ['.'] });
  try {
    const page = await reopened.firstWindow();
    const project = page.locator('section[aria-label="项目列表"] article').filter({ hasText: mediaPath }).first();
    await project.getByRole('button', { name: '打开' }).click();
    await expect(page.getByLabel('字体')).toHaveValue('PingFang SC');
  } finally {
    await reopened.close();
  }
});

test('saves a named cloud model record', async () => {
  const app = await electron.launch({ args: ['.'] });
  const name = `云模型-${Date.now()}`;

  try {
    const page = await app.firstWindow();
    await page.getByRole('button', { name: '设置' }).click();
    await page.getByRole('tab', { name: 'AI 模型' }).click();
    const modelPanel = page.getByLabel('模型设置');
    await modelPanel.getByLabel('名称').fill(name);
    await modelPanel.getByLabel('API 地址').fill('https://model.example.test/v1');
    await modelPanel.getByRole('textbox', { name: '模型', exact: true }).fill('semantic-editor');
    await modelPanel.getByLabel('凭证引用').fill('credential-test');
    await modelPanel.getByLabel('模型凭证').fill('secret-test-value');
    await modelPanel.getByRole('button', { name: '保存模型' }).click();
    await expect(modelPanel).toContainText(name);
    const modelStatus = page.getByLabel('当前模型状态');
    await expect(modelStatus).toContainText(`${name} 离线`);
    await expect(modelStatus).toHaveCSS('right', '440px');
    await expect(modelStatus).toHaveCSS('color', 'rgb(209, 67, 67)');
  } finally {
    await app.close();
  }
});

test('reports an invalid style package import', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill(`/tmp/style-package-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/style-package-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();
    const library = page.getByLabel('资源库');
    await library.getByLabel('风格包路径').fill('/tmp/missing-style-package.zip');
    await library.getByRole('button', { name: '导入风格包' }).click();
    await expect(library.getByRole('alert')).toContainText('导入失败');
  } finally {
    await app.close();
  }
});

test('shows AI timeline analysis controls without a project', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    const analysis = page.getByLabel('AI时间线分析');
    await expect(analysis.getByRole('button', { name: '开始AI分析' })).toBeDisabled();
    await expect(analysis.getByLabel('已保存AI计划')).toBeDisabled();
  } finally {
    await app.close();
  }
});

test('defaults AI plan application to preserve edited clips', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await expect(page.getByLabel('AI应用方式')).toHaveValue('unmodified-only');
  } finally {
    await app.close();
  }
});

test('shows custom resolution inputs for manual export', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByRole('button', { name: '导出' }).click();
    const exportPanel = page.getByLabel('导出');
    await exportPanel.getByLabel('分辨率').selectOption('custom');
    await expect(exportPanel.getByLabel('自定义宽度')).toBeVisible();
    await expect(exportPanel.getByLabel('自定义高度')).toBeVisible();
  } finally {
    await app.close();
  }
});

test('shows a native workspace directory picker', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByRole('button', { name: '设置' }).click();
    await expect(page.getByLabel('工作目录').getByRole('button', { name: '选择目录' })).toBeVisible();
  } finally {
    await app.close();
  }
});

test('shows workspace validation errors in the settings panel', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    await page.getByRole('button', { name: '设置' }).click();
    const workspace = page.getByRole('region', { name: '工作目录' });
    await workspace.getByLabel('工作目录路径').fill('relative/workspace');
    await workspace.getByRole('button', { name: '应用' }).click();
    await expect(workspace.getByRole('alert')).toContainText('workspace root must be absolute');
  } finally {
    await app.close();
  }
});

test('switches project storage when changing the workspace', async () => {
  const app = await electron.launch({ args: ['.'] });
  const sourceRoot = `/tmp/ai-video-workspace-source-${Date.now()}`;
  const destinationRoot = `/tmp/ai-video-workspace-destination-${Date.now()}`;
  const mediaPath = `/tmp/workspace-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.evaluate((nextRoot) => window.aiVideo.workspace.setRoot(nextRoot), sourceRoot);
    await page.getByLabel('文案文稿').fill(`/tmp/workspace-source-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    await expect(page.getByLabel('项目列表')).toHaveCount(0);
    await expect(page.getByLabel('工作目录')).toHaveCount(0);
    await expect(page.evaluate((path) => window.aiVideo.projects.list().then((projects) => projects.some((candidate) => candidate.media.path === path)), mediaPath)).resolves.toBe(true);

    await page.getByRole('button', { name: '设置' }).click();
    const workspace = page.getByRole('region', { name: '工作目录' });
    await workspace.getByLabel('工作目录路径').fill(destinationRoot);
    await workspace.getByRole('button', { name: '应用' }).click();
    await expect(workspace).toContainText(`已设置：${destinationRoot}`);
    await page.getByRole('button', { name: '关闭设置' }).click();
    await expect(page.getByLabel('项目列表')).not.toContainText(mediaPath);
  } finally {
    await app.close();
  }
});

test('reloads saved models when changing the workspace', async () => {
  const app = await electron.launch({ args: ['.'] });
  const sourceRoot = `/tmp/ai-video-model-workspace-source-${Date.now()}`;
  const destinationRoot = `/tmp/ai-video-model-workspace-destination-${Date.now()}`;

  try {
    const page = await app.firstWindow();
    await page.evaluate((nextRoot) => window.aiVideo.workspace.setRoot(nextRoot), sourceRoot);
    await page.getByRole('button', { name: '设置' }).click();
    await page.getByRole('tab', { name: 'AI 模型' }).click();
    const modelSettings = page.getByLabel('模型设置');
    await modelSettings.getByLabel('名称').fill('源目录模型');
    await modelSettings.getByLabel('API 地址').fill('https://example.com/v1');
    await modelSettings.getByLabel('模型', { exact: true }).fill('source-model');
    await modelSettings.getByLabel('凭证引用').fill(`source-credential-${Date.now()}`);
    await modelSettings.getByLabel('模型凭证').fill('test-key');
    await modelSettings.getByRole('button', { name: '保存模型' }).click();
    await expect(modelSettings.getByLabel('已保存模型')).toContainText('源目录模型');

    await page.getByRole('tab', { name: '通用' }).click();
    const workspace = page.getByRole('region', { name: '工作目录' });
    await workspace.getByLabel('工作目录路径').fill(destinationRoot);
    await workspace.getByRole('button', { name: '应用' }).click();

    await page.getByRole('tab', { name: 'AI 模型' }).click();
    const destinationModelSettings = page.getByLabel('模型设置');
    await expect(destinationModelSettings.getByLabel('已保存模型')).not.toContainText('源目录模型');
    await expect(page.getByLabel('分析模型')).toContainText('暂无模型');
  } finally {
    await app.close();
  }
});
