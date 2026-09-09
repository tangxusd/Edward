const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");

test("FableCut keeps the three-column editor regions and timeline anchors", () => {
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  const css = fs.readFileSync(path.join(root, "style.css"), "utf8");
  for (const id of ["binList", "monitorStage", "inspector", "timelinePanel"]) assert.match(html, new RegExp(`id="${id}"`));
  assert.match(css, /\.app[\s\S]*display:\s*grid/);
  assert.match(css, /\.app > \.upper > \.inspector[\s\S]*grid-row:\s*2\s*\/\s*4/);
  assert.match(css, /\.app > \.timeline-panel[\s\S]*grid-column:\s*1\s*\/\s*3/);
  assert.match(css, /\.app > \.timeline-panel \.clip[\s\S]*pointer-events:\s*auto/);
});
