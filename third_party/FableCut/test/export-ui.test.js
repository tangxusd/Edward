const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "..");

test("导出弹窗使用中文模式、配置和操作文案", () => {
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  const dialog = html.match(/<div class="overlay hidden" id="exportSetup">([\s\S]*?)<\/div>\s*<\/div>/)?.[1] || "";
  assert.match(dialog, /<h2>导出<\/h2>/);
  assert.match(dialog, /快速导出（服务器编码）/);
  assert.match(dialog, /实时导出（浏览器编码）/);
  assert.match(dialog, /编码配置/);
  assert.match(dialog, /开始导出/);
  assert.doesNotMatch(dialog, /Encoding profile|Fast \(ffmpeg\)|Cancel<\/button>/);
});

test("导出按选中编码配置的扩展名检查冲突", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(app, /selectedExtension = els\.engineFast\.checked \? \(exportProfileMeta\(selectedProfile\)\.extension \|\| "\.mp4"\) : "\.mp4"/);
  assert.match(app, /encodeURIComponent\(selectedExtension\)/);
});

test("编码配置提供稳定的中文显示名", () => {
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  for (const label of ["草稿 · H.264 快速", "交付 · H.264 均衡", "高质量 · H.264 慢速", "ProRes 422 HQ · MOV"]) {
    assert.match(app, new RegExp(label.replace(/[.*+?^${}()|[\\]\\\\]/g, "\\\\$&")));
  }
});

test("导出弹窗提供按当前画幅适配的分辨率选择", () => {
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  const app = fs.readFileSync(path.join(root, "app.js"), "utf8");
  assert.match(html, /id="exportResolutionSel"/);
  assert.match(html, /输出分辨率/);
  assert.match(app, /function exportResolutionOptions\(\)/);
  assert.match(app, /function getExportOutputSpec\(\)/);
  assert.match(app, /window\.fablecutQtOutputSpec = outputSpec/);
  assert.match(app, /fastExport\(outputSpec\)/);
  assert.match(app, /Number\(outputSpec\.width\)/);
});
