import { describe, expect, it } from 'vitest';
import { parseStylePackageManifest } from '../src/index.js';

describe('style package manifests', () => {
  it('rejects executable entry points and path traversal', () => {
    expect(() => parseStylePackageManifest({ version: 1, id: 'a', name: 'A', type: 'card-style', category: 'card', entryScript: 'run.js' })).toThrow();
    expect(() => parseStylePackageManifest({ version: 1, id: 'a', name: 'A', type: 'card-style', category: 'card', assets: ['../run.js'] })).toThrow();
    expect(() => parseStylePackageManifest({ version: 1, id: 'a', name: 'A', type: 'card-style', category: 'card', assets: ['..\\run.js'] })).toThrow();
    expect(() => parseStylePackageManifest({ version: 1, id: 'a', name: 'A', type: 'card-style', category: 'card', assets: ['C:\\outside.png'] })).toThrow();
    expect(() => parseStylePackageManifest({ version: 1, id: '../outside', name: 'A', type: 'card-style', category: 'card' })).toThrow();
    expect(() => parseStylePackageManifest({ version: 1, id: 'nested\\outside', name: 'A', type: 'card-style', category: 'card' })).toThrow();
  });

  it('accepts a static card style package', () => {
    expect(parseStylePackageManifest({ version: 1, id: 'a', name: 'A', type: 'card-style', category: 'card', assets: ['preview.png', 'style.css'] }).type).toBe('card-style');
  });
});
