const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");

test("Chinese-first bilingual UI resources are wired before app startup", () => {
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  const i18n = fs.readFileSync(path.join(root, "i18n.js"), "utf8");
  assert.match(html, /id="languageSel"/);
  assert.match(html, /src="i18n\.js"/);
  assert.match(i18n, /zh-CN/);
  assert.match(i18n, /en-US/);
  assert.match(i18n, /localStorage\.getItem\("fablecut-language"\)/);
  for (const key of ["Snap", "Audio Hold", "Delete", "Shake", "Inspector", "Adjust", "Frame"]) {
    assert.match(i18n, new RegExp(`\\\"${key}\\\"`), `missing translation key: ${key}`);
  }
});
