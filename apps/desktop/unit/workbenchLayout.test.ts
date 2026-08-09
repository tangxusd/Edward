import { describe, expect, test } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

describe('workbench layout', () => {
  const cssPath = resolve(__dirname, '../src/renderer/theme.css');
  const css = readFileSync(cssPath, 'utf8');

  test('workbench-grid uses 5 columns and 3 rows', () => {
    expect(css).toContain('grid-template-columns: 245px 7px 1fr 7px 310px');
    expect(css).toContain('grid-template-rows: 430px 7px 245px');
  });

  test('right panel spans full height (rows 1-4)', () => {
    expect(css).toContain('grid-row: 1 / 4');
  });

  test('timeline spans columns 1-3', () => {
    expect(css).toContain('grid-column: 1 / 4');
  });

  test('dividers have col-resize and row-resize cursors', () => {
    expect(css).toContain('cursor: col-resize');
    expect(css).toContain('cursor: row-resize');
  });

  test('topbar uses 3-column grid', () => {
    expect(css).toContain('grid-template-columns: 1fr auto 1fr');
  });

  test('workbench container has correct background', () => {
    expect(css).toContain('background: #181818');
    expect(css).toContain('border: 1px solid #3a3a3a');
  });
});