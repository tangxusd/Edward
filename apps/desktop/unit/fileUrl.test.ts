import { describe, expect, it } from 'vitest';

import { toLocalFileUrl } from '../src/renderer/fileUrl.js';

describe('toLocalFileUrl', () => {
  it('converts POSIX paths and escapes spaces', () => {
    expect(toLocalFileUrl('/Users/test/My Video.mp4')).toBe('file:///Users/test/My%20Video.mp4');
  });

  it('converts Windows paths without losing the drive letter', () => {
    expect(toLocalFileUrl('C:\\Users\\test\\素材.png')).toBe('file:///C:/Users/test/%E7%B4%A0%E6%9D%90.png');
  });
});
