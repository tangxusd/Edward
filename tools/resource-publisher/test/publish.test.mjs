import test from "node:test";
import assert from "node:assert/strict";
import { validateManifest, sha256File } from "../publish.mjs";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";

test("rejects invalid manifest fields", () => {
  const errors = validateManifest({});
  for (const expected of ["component_id is invalid", "tab_key is invalid", "category_id must be a UUID", "name is required", "version is invalid", "runtime is invalid", "target is invalid", "capabilities is invalid", "timeline is invalid", "editableProperties is invalid", "editableTracks is invalid", "assets is invalid"]) {
    assert.ok(errors.includes(expected), expected);
  }
});

test("hashes package bytes deterministically", async () => {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), "edward-resource-"));
  const file = path.join(dir, "package.zip");
  await fs.writeFile(file, "resource");
  assert.equal(await sha256File(file), "5de95319f17467ed6dc58e4e0b16c1193a13b35d60dc48bcf06bf6b7beebbe6c");
});

test("validates optional component target", () => {
  const base = {
    component_id: "demo.component", tab_key: "text", category_id: "00000000-0000-0000-0000-000000000000", name: "Demo", version: "1.0.0",
    runtime: "react", capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" }, editableProperties: ["color"], editableTracks: ["x"],
    assets: [{ path: "component.js", mimeType: "text/javascript", bytes: 1, sha256: "a".repeat(64) }],
  };
  assert.deepEqual(validateManifest({ ...base, target: "Web.Runtime" }).at(-1), "target is invalid");
  assert.deepEqual(validateManifest({ ...base, target: "other.runtime" }).at(-1), "target is invalid");
  assert.equal(validateManifest({ ...base, target: "web.runtime" }).length, 0);
});

test("requires native runtime metadata and an MP4 preview contract", () => {
  const base = {
    component_id: "demo.component", tab_key: "text", category_id: "00000000-0000-0000-0000-000000000000",
    name: "Demo", version: "1.0.0", target: "web.runtime", runtime: "react",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" },
    editableProperties: ["color"], editableTracks: ["x"],
    assets: [{ path: "component.js", mimeType: "text/javascript", bytes: 1, sha256: "a".repeat(64) }],
  };
  assert.deepEqual(validateManifest(base), []);
  assert.match(validateManifest({ ...base, runtime: "canvas" }).join(" "), /runtime is invalid/);
  assert.match(validateManifest({ ...base, timeline: { authoringFps: 30 } }).join(" "), /timeline is invalid/);
  assert.match(validateManifest({ ...base, editableTracks: ["unknown"] }).join(" "), /editableTracks is invalid/);
});
