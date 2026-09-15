const test = require('node:test');
const assert = require('node:assert/strict');
const { normalizeOutputSpec, createExportSnapshot } = require('../export-compositor.js');
const fs = require('node:fs');
const path = require('node:path');

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

test('server finalizes exports without replacing an existing filename', () => {
  const server = fs.readFileSync(path.join(__dirname, '..', 'server.js'), 'utf8');
  assert.match(server, /function finalizeExportPath\(sess\)/);
  assert.match(server, /fs\.copyFileSync\(sess\.partPath, out, fs\.constants\.COPYFILE_EXCL\)/);
  assert.match(server, /if \(e\.code === "EEXIST"\) continue/);
});

test('legacy component foreignObject clone clears monitor positioning', () => {
  const runtime = fs.readFileSync(path.join(__dirname, '..', 'component-runtime.js'), 'utf8');
  assert.match(runtime, /clone\.style\.position = "relative"/);
  assert.match(runtime, /clone\.style\.inset = "auto"/);
  assert.match(runtime, /clone\.style\.right = "auto"/);
  assert.match(runtime, /clone\.style\.bottom = "auto"/);
});
