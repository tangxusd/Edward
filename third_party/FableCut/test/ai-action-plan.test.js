const test = require("node:test");
const assert = require("node:assert/strict");
const { apply } = require("../ai-action-plan.js");

const tracks = [{ id: "V3", kind: "video" }, { id: "V2", kind: "video" }, { id: "V1", kind: "video" }];
const resource = { id: "annotation.rect.svg", runtime: "svg", name: "闭合矩形", props: { duration: { default: 3 }, x: { default: 0 } } };
function context(clips = []) {
  return { project: { revision: 7, fps: 30, clips }, resources: [resource], tracks, playhead: 2, nextClipId: () => "c_ai" };
}
function plan(operations) { return { schemaVersion: "edward.action-plan.v1", requestId: "test", baseProjectRevision: 7, operations }; }

test("ActionPlan inserts a verified native component onto the first free upper track", () => {
  const clips = [{ id: "c_existing", kind: "component", track: "V2", start: 2, duration: 3, props: {} }];
  const result = apply(plan([{ type: "insert_native_component", resourceId: resource.id }]), context(clips));
  assert.equal(result.clips.length, 2);
  assert.equal(result.clips[1].track, "V3");
  assert.equal(result.clips[1].componentId, resource.id);
  assert.deepEqual(clips, [{ id: "c_existing", kind: "component", track: "V2", start: 2, duration: 3, props: {} }]);
});

test("ActionPlan adds a new upper video track instead of reporting a collision", () => {
  const clips = tracks.map((track) => ({ id: `c_${track.id}`, kind: "component", track: track.id, start: 2, duration: 3, props: {} }));
  const result = apply(plan([{ type: "insert_native_component", resourceId: resource.id }]), context(clips));
  assert.equal(result.clips.at(-1).track, "V4");
  assert.deepEqual(result.tracks.at(-1), { id: "V4", kind: "video" });
});

test("ActionPlan rejects stale revisions, unknown resources, and retains the project on failure", () => {
  assert.throws(() => apply({ ...plan([{ type: "insert_native_component", resourceId: resource.id }]), baseProjectRevision: 6 }, context()), /已过期/);
  assert.throws(() => apply(plan([{ type: "insert_native_component", resourceId: "unknown" }]), context()), /未验证/);
  assert.throws(() => apply(plan([{ type: "remove_clip", targetId: "missing" }]), context()), /目标已不存在/);
});

test("ActionPlan validates every operation before returning a timeline", () => {
  const clips = [{ id: "c_1", kind: "component", track: "V2", start: 0, duration: 2, props: { x: 0 } }];
  assert.throws(() => apply(plan([
    { type: "set_component_props", targetId: "c_1", props: { x: 20 } },
    { type: "move_clip", targetId: "c_1", timelineStart: -1 },
  ]), context(clips)), /移动参数无效/);
  assert.deepEqual(clips[0].props, { x: 0 });
});

test("ActionPlan accepts only component properties declared by the native manifest", () => {
  const clips = [{ id: "c_1", kind: "component", componentId: resource.id, track: "V2", start: 0, duration: 2, props: { x: 0 } }];
  assert.throws(() => apply(plan([
    { type: "set_component_props", targetId: "c_1", props: { shellCommand: "unsafe" } },
  ]), context(clips)), /组件协议中声明/);
  assert.throws(() => apply(plan([
    { type: "set_component_props", targetId: "c_1", props: { x: "20" } },
  ]), context(clips)), /数值属性无效/);
});
