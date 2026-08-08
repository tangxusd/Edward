import { _electron as electron, expect, test } from '@playwright/test';

test('exposes only the desktop bridge', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();

    await expect(page.locator('main')).toContainText('AI 剪视频工具');
    await expect(
      page.evaluate(() => (window as Window & { aiVideo?: unknown }).aiVideo),
    ).resolves.toBeDefined();
    await expect(
      page.evaluate(() => Object.keys((window as unknown as Window & { aiVideo: { projects: unknown } }).aiVideo)),
    ).resolves.toEqual(['workspace', 'projects', 'library', 'models', 'analysis', 'export']);
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
    await expect(page.getByLabel('项目列表')).toBeVisible();
    await expect(page.getByLabel('新建项目')).toBeVisible();
    await expect(page.getByLabel('资源库')).toBeHidden();

    await page.getByLabel('文案文稿').fill(`/tmp/library-script-${Date.now()}.txt`);
    await page.getByLabel('音频或视频').fill(`/tmp/library-source-${Date.now()}.mp3`);
    await page.getByRole('button', { name: '创建项目' }).click();

    await expect(page.getByLabel('项目列表')).toHaveCount(0);
    await expect(page.getByLabel('新建项目')).toHaveCount(0);
    await expect(page.getByLabel('资源库')).toBeVisible();
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
    await expect(page.getByLabel('预览画布')).toHaveCSS('aspect-ratio', '9 / 16');
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
    await expect(page.getByLabel('资源检查器')).toContainText('ai-cards-0');
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
    await expect(page.getByLabel('当前模型状态')).toContainText(`${name} 离线`);
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
