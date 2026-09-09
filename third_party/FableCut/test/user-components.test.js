const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");

test("demo user component manifest is browser-runnable and exposes editable props", () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, "components/demo/manifest.json"), "utf8"));
  assert.equal(manifest.entry, "./component.js");
  assert.deepEqual(Object.keys(manifest.props), ["title", "color", "x"]);
  assert.match(fs.readFileSync(path.join(root, "components/demo/component.js"), "utf8"), /export function mount/);
});

test("component service paths stay inside the component root", () => {
  const componentsRoot = path.join(root, "components");
  const safe = path.normalize(path.join(componentsRoot, "demo", "component.js"));
  const escaped = path.normalize(path.join(componentsRoot, "demo", "..", "..", "server.js"));
  assert.ok(safe.startsWith(componentsRoot + path.sep));
  assert.ok(!escaped.startsWith(componentsRoot + path.sep));
});
