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
      page.evaluate(() => (window as Window & { require?: unknown }).require),
    ).resolves.toBeUndefined();
  } finally {
    await app.close();
  }
});

test('drags preview cards while preserving the grab offset', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
    const card = page.getByLabel('可拖拽卡片').first();
    await card.scrollIntoViewIfNeeded();
    const before = await card.boundingBox();
    if (!before) throw new Error('preview card is not visible');
    const beforeLeft = await card.evaluate((element) => Number.parseFloat(getComputedStyle(element).left));
    const beforeTop = await card.evaluate((element) => Number.parseFloat(getComputedStyle(element).top));

    await page.mouse.move(before.x + 20, before.y + 20);
    await page.mouse.down();
    await page.mouse.move(before.x + 120, before.y + 70);
    await page.mouse.up();

    await expect(card).toHaveCSS('left', `${beforeLeft + 100}px`);
    await expect(card).toHaveCSS('top', `${beforeTop + 50}px`);
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

    const clip = page.locator('div[role="button"]').filter({ hasText: 'main-media' }).first();
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

test('persists subtitle style changes with the project', async () => {
  const app = await electron.launch({ args: ['.'] });
  const mediaPath = `/tmp/subtitle-style-source-${Date.now()}.mp3`;

  try {
    const page = await app.firstWindow();
    await page.getByLabel('文案文稿').fill('/tmp/subtitle-style-script.txt');
    await page.getByLabel('音频或视频').fill(mediaPath);
    await page.getByRole('button', { name: '创建项目' }).click();
    const createdProject = page.locator('section[aria-label="项目列表"] article').filter({ hasText: mediaPath }).first();
    await expect(createdProject).toBeVisible();
    const font = page.getByLabel('字体');
    await font.fill('PingFang SC');
    await font.blur();
    const projectId = await createdProject.locator('strong').textContent();
    if (!projectId) throw new Error('created project id is missing');
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
    const modelPanel = page.getByLabel('模型设置');
    await modelPanel.getByLabel('名称').fill(name);
    await modelPanel.getByLabel('API 地址').fill('https://model.example.test/v1');
    await modelPanel.getByRole('textbox', { name: '模型', exact: true }).fill('semantic-editor');
    await modelPanel.getByLabel('凭证引用').fill('credential-test');
    await modelPanel.getByLabel('模型凭证').fill('secret-test-value');
    await modelPanel.getByRole('button', { name: '保存模型' }).click();
    await expect(modelPanel).toContainText(name);
  } finally {
    await app.close();
  }
});

test('reports an invalid style package import', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();
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
    const exportPanel = page.getByLabel('导出');
    await exportPanel.getByLabel('分辨率').selectOption('custom');
    await expect(exportPanel.getByLabel('自定义宽度')).toBeVisible();
    await expect(exportPanel.getByLabel('自定义高度')).toBeVisible();
  } finally {
    await app.close();
  }
});
