import { afterEach, describe, expect, it, vi } from 'vitest';
import { requestComponentContentEdit } from '../src/main/semanticAnalysisService.js';

afterEach(() => vi.unstubAllGlobals());

describe('requestComponentContentEdit', () => {
  it('returns a structured component content draft', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: true, json: async () => ({ choices: [{ message: { content: JSON.stringify({ reply: '已更新数据', draftContent: { text: '新内容' } }) } }] }) }));
    await expect(requestComponentContentEdit({ baseUrl: 'https://model.test', modelId: 'chat-model', apiKey: 'key', script: '文稿', componentType: 'card', content: { text: '旧内容' }, message: '修改内容', history: [] })).resolves.toEqual({ reply: '已更新数据', draftContent: { text: '新内容' } });
  });

  it('rejects a response without a component content draft', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: true, json: async () => ({ choices: [{ message: { content: JSON.stringify({ reply: '没有草案' }) } }] }) }));
    await expect(requestComponentContentEdit({ baseUrl: 'https://model.test', modelId: 'chat-model', apiKey: 'key', script: '文稿', componentType: 'card', content: { text: '旧内容' }, message: '修改内容', history: [] })).rejects.toThrow('component edit returned invalid content');
  });
});
