const test = require('node:test');
const assert = require('node:assert/strict');
const { normalizeOutputSpec, createExportSnapshot } = require('../export-compositor.js');

test('normalizes explicit 4K 60fps output without crop', () => {
  const spec = normalizeOutputSpec({ width: 3840, height: 2160, fps: 60 }, { width: 1280, height: 720, fps: 30 });
  assert.deepEqual(spec, { width: 3840, height: 2160, fps: 60, pixelRatio: 1, crop: 'none', format: 'mp4', quality: 'high' });
});

test('snapshot is detached from mutable project state', () => {
  const project = { width: 1280, height: 720, fps: 30, clips: [{ id: 'card6', kind: 'component', props: { text: 'A' } }] };
  const snapshot = createExportSnapshot(project);
  project.clips[0].props.text = 'B';
  assert.equal(snapshot.clips[0].props.text, 'A');
});
