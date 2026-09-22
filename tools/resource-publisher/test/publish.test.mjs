import test from "node:test";
import assert from "node:assert/strict";
import { validateManifest, validatePackage, sha256File } from "../publish.mjs";
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

test("拒绝缺少运行时清单、入口或哈希不匹配资产的发布包", () => {
  const manifest = {
    component_id: "demo.component", version: "1.0.0", runtime: "react", target: "web.runtime",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" }, editableProperties: ["color"], editableTracks: ["x"],
    assets: [{ path: "component.js", mimeType: "text/javascript", bytes: 3, sha256: "a".repeat(64) }],
  };
  assert.match(validatePackage(manifest, new Map(), Buffer.alloc(12)).join(" "), /edward-runtime/);
  const runtime = Buffer.from(JSON.stringify({ protocol: "edward.web-runtime.v1", runtime: "react", previewEntry: "component.js", renderEntry: "component.js", editableProperties: ["color"], editableTracks: ["x"], capabilities: { preview: true, export: true } }));
  assert.match(validatePackage(manifest, new Map([["edward-runtime.json", runtime], ["component.js", Buffer.from("bad")]]), Buffer.concat([Buffer.alloc(4), Buffer.from("ftyp"), Buffer.alloc(4)])).join(" "), /asset hash/);
});

test("发布包运行时合同必须与外层版本清单一致", () => {
  const manifest = {
    component_id: "demo.component", version: "1.0.0", runtime: "react", target: "web.runtime",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" },
    editableProperties: ["color"], editableTracks: ["x"], assets: [],
  };
  const runtime = Buffer.from(JSON.stringify({
    protocol: "edward.web-runtime.v1", runtime: "react", fps: 24, durationInFrames: 80,
    propsSchema: { type: "object", additionalProperties: true, properties: {} },
    previewEntry: "component.js", renderEntry: "component.js",
    editableProperties: ["other"], editableTracks: ["y"], capabilities: { preview: true, export: true },
  }));
  const files = new Map([["edward-runtime.json", runtime], ["component.js", Buffer.from("x")]]);
  const errors = validatePackage(manifest, files, Buffer.concat([Buffer.alloc(4), Buffer.from("ftyp"), Buffer.alloc(4)]));
  assert.ok(errors.includes("runtime contract does not match manifest"));
});

test("发布器拒绝明显过大的预览文件", () => {
  const manifest = {
    component_id: "demo.component", version: "1.0.0", runtime: "react", target: "web.runtime",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" },
    editableProperties: ["color"], editableTracks: ["x"], assets: [],
  };
  const runtime = Buffer.from(JSON.stringify({
    protocol: "edward.web-runtime.v1", runtime: "react", fps: 30, durationInFrames: 90,
    propsSchema: { type: "object", additionalProperties: false, properties: {} },
    previewEntry: "component.js", renderEntry: "component.js",
    editableProperties: ["color"], editableTracks: ["x"], capabilities: { preview: true, export: true },
  }));
  const preview = Buffer.concat([Buffer.alloc(4), Buffer.from("ftyp"), Buffer.alloc(20 * 1024 * 1024)]);
  const errors = validatePackage(manifest, new Map([["edward-runtime.json", runtime], ["component.js", Buffer.from("x")]]), preview);
  assert.ok(errors.includes("preview is too large"));
});

test("运行时可编辑属性必须有对应的受限 schema 定义", () => {
  const manifest = {
    component_id: "demo.component", version: "1.0.0", runtime: "react", target: "web.runtime",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" },
    editableProperties: ["color"], editableTracks: ["x"], assets: [],
  };
  const runtime = Buffer.from(JSON.stringify({
    protocol: "edward.web-runtime.v1", runtime: "react", fps: 30, durationInFrames: 90,
    propsSchema: { type: "object", additionalProperties: false, properties: { x: { type: "number", default: 0 } } },
    previewEntry: "component.js", renderEntry: "component.js",
    editableProperties: ["color"], editableTracks: ["x"], capabilities: { preview: true, export: true },
  }));
  const errors = validatePackage(manifest, new Map([["edward-runtime.json", runtime], ["component.js", Buffer.from("x")]]), Buffer.concat([Buffer.alloc(4), Buffer.from("ftyp"), Buffer.alloc(4)]));
  assert.ok(errors.includes("runtime properties schema is incomplete"));
});

test("缺少运行时编辑列表时返回校验错误而非抛出异常", () => {
  const manifest = {
    component_id: "demo.component", version: "1.0.0", runtime: "react", target: "web.runtime",
    capabilities: { preview: true, export: true, editable: true, audio: false, transparent: true },
    timeline: { authoringFps: 30, durationFrames: 90, frameRounding: "nearest" },
    editableProperties: ["color"], editableTracks: ["x"], assets: [],
  };
  const runtime = Buffer.from(JSON.stringify({
    protocol: "edward.web-runtime.v1", runtime: "react", fps: 30, durationInFrames: 90,
    propsSchema: { type: "object", additionalProperties: false, properties: {} },
    previewEntry: "component.js", renderEntry: "component.js", capabilities: { preview: true, export: true },
  }));
  const errors = validatePackage(manifest, new Map([["edward-runtime.json", runtime], ["component.js", Buffer.from("x")]]), Buffer.concat([Buffer.alloc(4), Buffer.from("ftyp"), Buffer.alloc(4)]));
  assert.ok(errors.includes("runtime entries are invalid"));
});
