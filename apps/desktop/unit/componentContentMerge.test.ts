import { describe, expect, it } from 'vitest';
import { describeComponentChanges, mergeComponentContent } from '../src/renderer/componentContentMerge.js';

describe('mergeComponentContent', () => {
  it('keeps omitted fields while applying compatible component data', () => {
    expect(mergeComponentContent({ title: '收入', series: [1, 2], style: { color: '#fff' } }, { series: [3, 4] })).toEqual({ title: '收入', series: [3, 4], style: { color: '#fff' } });
  });

  it('rejects incompatible data types and unknown fields', () => {
    expect(() => mergeComponentContent({ series: [1, 2] }, { series: '错误格式' })).toThrow('字段“series”类型不兼容');
    expect(() => mergeComponentContent({ series: [1, 2] }, { labels: ['一月'] })).toThrow('字段“labels”不在组件数据格式中');
    expect(() => mergeComponentContent({ series: [{ label: '一月', value: 12 }] }, { series: [{ label: '一月', value: 13, color: 'red' }] })).toThrow('字段“color”不在组件数据格式中');
  });

  it('lists only fields that change after a valid merge', () => {
    expect(describeComponentChanges({ title: '收入', series: [1, 2] }, { title: '收入', series: [3, 4] })).toEqual(['series']);
  });
});
