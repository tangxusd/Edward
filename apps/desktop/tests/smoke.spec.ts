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
    ).resolves.toEqual(['workspace', 'projects', 'library', 'models', 'export']);
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
