import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './tests',
  testIgnore: ['**/._*'],
  timeout: 30_000,
  workers: 1,
});
