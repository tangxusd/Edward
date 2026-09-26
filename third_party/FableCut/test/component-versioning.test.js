const test = require("node:test");
const assert = require("node:assert/strict");
const { validateComponentInstance, componentFrameAt } = require("../component-versioning.js");

const manifest = {
  protocol: "edward.web-runtime.v1", fps: 30, durationInFrames: 90,
  propsSchema: { properties: { color: { type: "string" }, x: { type: "number", minimum: -100, maximum: 100 } } },
  editableProperties: ["color", "x"], editableTracks: ["x"],
};
const resourceRef = { target: "web.runtime", componentId: "annotation.rect", version: "1.0.0", contentHash: "a".repeat(64) };
const cachedManifest = { ...manifest, componentId: "annotation.rect", version: "1.0.0", contentHash: "a".repeat(64) };

test("实例拒绝未知属性和未授权关键帧，并锁定内容哈希", () => {
  assert.equal(validateComponentInstance(manifest, { resourceRef, props: { color: "#fff", unknown: 1 }, keyframes: { x: [] } }).ok, false);
  assert.match(validateComponentInstance(manifest, { resourceRef, props: { color: "#fff" }, keyframes: { opacity: [] } }).reason, /未授权关键帧/);
  assert.deepEqual(validateComponentInstance(manifest, { resourceRef, props: { color: "#fff", x: 0 }, keyframes: { x: [] } }), { ok: true });
});

test("项目帧映射为作者最近整数帧", () => {
  assert.equal(componentFrameAt(45, { projectFps: 30 }, manifest), 45);
  assert.equal(componentFrameAt(15, { projectFps: 24 }, manifest), 19);
});

test("缓存组件必须将实例、缓存键和运行时清单锁定为同一身份", () => {
  const cachedRef = { ...resourceRef, source: "cached", cacheKey: `annotation.rect@1.0.0#${"a".repeat(64)}` };
  assert.deepEqual(validateComponentInstance(cachedManifest, { resourceRef: cachedRef, props: { color: "#fff" }, keyframes: {} }), { ok: true });
  assert.match(validateComponentInstance(cachedManifest, { resourceRef: { ...cachedRef, componentId: "other.component", cacheKey: `other.component@1.0.0#${"a".repeat(64)}` }, props: {}, keyframes: {} }).reason, /组件身份/);
  assert.match(validateComponentInstance(cachedManifest, { resourceRef: { ...cachedRef, cacheKey: `annotation.rect@1.0.1#${"a".repeat(64)}` }, props: {}, keyframes: {} }).reason, /缓存键/);
});
