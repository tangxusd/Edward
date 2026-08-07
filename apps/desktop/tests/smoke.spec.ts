import { _electron as electron, expect, test } from '@playwright/test';

test('exposes only the desktop bridge', async () => {
  const app = await electron.launch({ args: ['.'] });

  try {
    const page = await app.firstWindow();

    await expect(page.locator('#root')).toBeAttached();
    await expect(
      page.evaluate(() => (window as Window & { aiVideo?: unknown }).aiVideo),
    ).resolves.toBeDefined();
    await expect(
      page.evaluate(() => (window as Window & { require?: unknown }).require),
    ).resolves.toBeUndefined();
  } finally {
    await app.close();
  }
});
