const test = require("node:test");
const assert = require("node:assert/strict");
const { resolveResourceInsertionTrack } = require("../resource-timeline.js");

const tracks = [
  { id: "V3", kind: "video" },
  { id: "V2", kind: "video" },
  { id: "V1", kind: "video" },
];

test("资源卡片优先把当前资源的三秒片段插入最底层的可用轨道", () => {
  const result = resolveResourceInsertionTrack(tracks, [], 5, 3, 16);
  assert.deepEqual(result, { trackId: "V1", createdTrack: null });
});

test("资源卡片遇到冲突时按层级向上寻找空轨道", () => {
  const clips = [{ id: "v1", track: "V1", start: 5, duration: 3 }];
  const result = resolveResourceInsertionTrack(tracks, clips, 5, 3, 16);
  assert.deepEqual(result, { trackId: "V2", createdTrack: null });
});

test("所有现有视频轨道冲突时创建下一条视频轨道", () => {
  const clips = tracks.map((track) => ({ id: track.id, track: track.id, start: 5, duration: 3 }));
  const result = resolveResourceInsertionTrack(tracks, clips, 5, 3, 16);
  assert.deepEqual(result, { trackId: "V4", createdTrack: { id: "V4", kind: "video" } });
});

test("中间上一层不存在时就在该层新建轨道", () => {
  const sparseTracks = [{ id: "V3", kind: "video" }, { id: "V1", kind: "video" }];
  const clips = [{ id: "v1", track: "V1", start: 5, duration: 3 }];
  const result = resolveResourceInsertionTrack(sparseTracks, clips, 5, 3, 16);
  assert.deepEqual(result, { trackId: "V2", createdTrack: { id: "V2", kind: "video" } });
});

test("文本收藏卡只在文本我的收藏中前置，不污染远端资源列表", () => {
  const app = require("node:fs").readFileSync(require("node:path").join(__dirname, "..", "app.js"), "utf8");
  assert.match(app, /function fixedTextFavoriteCards\(\)/);
  assert.match(app, /resourceBrowserState\.tab !== "text" \|\| resourceBrowserState\.filter !== "favorites"/);
  assert.match(app, /const displayItems = \[\.\.\.fixedTextFavoriteCards\(\), \.\.\.\(items \|\| \[\]\)\]/);
  assert.doesNotMatch(app, /resourceBrowserState\.items\s*=\s*\[\.\.\.fixedTextFavoriteCards/);
  assert.match(app, /addTimelineTrack\("video", placement\.createdTrack\.id\)/);
});

test("标题和字幕遵循画布中心坐标与安全区位置合同", () => {
  const app = require("node:fs").readFileSync(require("node:path").join(__dirname, "..", "app.js"), "utf8");
  const fs = require("node:fs");
  const path = require("node:path");
  assert.match(app, /id: "edward\.local\.text\.title"[\s\S]*x: 0,[\s\S]*y: 0,[\s\S]*align: "center",[\s\S]*vAlign: "middle"/);
  assert.match(app, /id: "edward\.local\.text\.subtitle"[\s\S]*x: 0,[\s\S]*y: subtitleSafeAreaY\(project\.height\),[\s\S]*align: "center"/);
  assert.match(app, /preview_url: "\/assets\/resource-previews\/title\.mp4"/);
  assert.match(app, /preview_url: "\/assets\/resource-previews\/subtitle\.mp4"/);
  for (const file of ["title.mp4", "subtitle.mp4"]) {
    const bytes = fs.readFileSync(path.join(__dirname, "..", "assets", "resource-previews", file));
    assert.equal(bytes.subarray(4, 8).toString("ascii"), "ftyp");
  }
  assert.match(app, /const VERTICAL_BOTTOM_SAFE_AREA_RATIO = 0\.34;/);
  assert.match(app, /function subtitleSafeAreaY\(height\)\s*\{\s*return Number\(height\) \* \(VERTICAL_BOTTOM_SAFE_AREA_RATIO - 0\.5\);/);
});
