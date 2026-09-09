const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");

test("server applies a shared dot-file exclusion rule to resource scans", () => {
  const server = fs.readFileSync(path.join(root, "server.js"), "utf8");
  assert.match(server, /function isVisibleResourceName\(name\)/);
  assert.match(server, /!base\.startsWith\("\."\)/);
  assert.match(server, /filter\(isVisibleResourceName\)/);
  assert.match(server, /if \(!isVisibleResourceName\(f\)\) continue/);
});

test("front-end library list also rejects dot files defensively", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(app, /runtime\.library\[dir\].*filter\(\(f\) => !String\(f\.name \|\| ""\)\.startsWith\("\."\)\)/s);
});
