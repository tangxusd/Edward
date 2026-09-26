const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const { execFileSync } = require("node:child_process");
const { modelView, apply } = require("../ai-action-plan.js");

test("C++ capability snapshot crosses the host bridge into modelView and apply", () => {
  const binary = process.env.ORBIT_CAPABILITY_TEST_BIN || path.resolve(__dirname, "../../../build-0.7.0/tests/ai/test_capability_registry");
  const snapshot = JSON.parse(execFileSync(binary, ["--dump-capability-snapshot"], { encoding: "utf8" }));
  const view = modelView(snapshot);
  assert.ok(view.some((item) => item.id === "create_marker"));
  const result = apply({
    schemaVersion: "orbit.bound-action-plan.v2",
    requestId: "cross-layer",
    baseProjectRevision: 1,
    operations: [{ type: "create_marker", timelineFrame: 1, label: "M1", color: "blue" }],
  }, {
    project: { revision: 1, fps: 30, clips: [], markers: [] },
    tracks: [{ id: "V1", kind: "video" }],
    resources: [],
    media: [],
    playhead: 0,
    capabilitySnapshot: snapshot,
    nextClipId: () => "c_cross",
    nextMarkerId: () => "m_cross",
  });
  assert.equal(result.markers.length, 1);
});
