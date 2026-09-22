const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const app = fs.readFileSync(path.join(__dirname, "..", "app.js"), "utf8");
const runtime = fs.readFileSync(path.join(__dirname, "..", "component-runtime.js"), "utf8");

test("所有公共资源页默认从我的收藏开始，标注页不再绕过资源浏览器", () => {
  assert.match(app, /filter: "favorites"/);
  assert.match(app, /resourceBrowserState\.filter = "favorites"/);
  assert.match(app, /new URLSearchParams\(\{ tabKey: resourceBrowserState\.tab, filter: resourceBrowserState\.filter/);
  assert.doesNotMatch(app, /tab === "annotation"\) \{ els\.resourceBrowser/);
});

test("卡片只显示名称和收藏数或本地标记，并复用收藏与插入操作", () => {
  assert.match(app, /function createResourceActionButton\(resource, action\)/);
  assert.match(app, /meta\.textContent = resource\.localFixed \? "本地" : `收藏 \$\{Number\(resource\.favorite_count \|\| 0\)\}`/);
  assert.doesNotMatch(app, /resource-card-summary"\)\.textContent = resource\.summary/);
  assert.match(app, /void insertResourceClip\(resource\)/);
  assert.match(app, /function toggleResourceFavorite\(resource\)/);
});

test("公共组件先写入受校验缓存，再以锁定版本和内容哈希运行", () => {
  assert.match(app, /resource\.cache_preview_url = payload\.previewUrl \|\| resource\.cache_preview_url/);
  assert.match(app, /function cachedResourcePreviewUrl\(resource\)/);
  assert.match(app, /fablecut-resource-preview:/);
  assert.match(app, /function preferredResourcePreviewUrl\(resource\)/);
  assert.match(app, /const resourceRef = \{ resourceId: resource\.id, componentId: resource\.component_id, version: resource\.version, contentHash: resource\.content_hash, target: "web\.runtime", cacheKey: cache\.key, source: "cached" \}/);
  assert.match(app, /if \(resourceRef\) clipOut\.resourceRef = resourceRef/);
  assert.match(runtime, /cacheEntryUrl\(resourceRef\.cacheKey, "edward-runtime\.json"\)/);
  assert.match(runtime, /mountDirectComponent\(clip\.componentId \|\| "demo", clip\.props \|\| \{\}, time - clip\.start, mode, renderViewport, clip\.resourceRef \|\| null\)/);
});

test("资源服务错误码仅以中文反馈给用户", () => {
  assert.match(app, /function resourceRequestMessage\(code, status\)/);
  assert.match(app, /catalog_unavailable: "资源目录暂时不可用，请稍后重试。"/);
  assert.match(app, /resource_cache_unavailable: "资源缓存失败，请稍后重试。"/);
  assert.match(app, /resourceRequestMessage\(value\?\.error, response\.status\)/);
});

test("资源目录与收藏操作写入统一诊断日志", () => {
  assert.match(app, /resource_catalog/);
  assert.match(app, /resource_favorite_start/);
  assert.match(app, /resource_favorite_end/);
  const server = fs.readFileSync(path.join(__dirname, "..", "server.js"), "utf8");
  assert.match(server, /resource_catalog/);
  assert.match(server, /resource_favorite/);
  assert.match(server, /console\.log\(`resource \$\{event\}/);
  assert.doesNotMatch(server, /console\.log\([^\n]*authorization/);
});

test("本地缓存预览失效时自动清理标记并回退远端 MP4", () => {
  assert.match(app, /function recoverResourcePreview\(resource, video\)/);
  assert.match(app, /localStorage\.removeItem\(previewKey\)/);
  assert.match(app, /video\.src = resource\.preview_url/);
});

test("公共组件在缓存完成后才重新计算插入轨道，避免异步缓存期间发生冲突", () => {
  assert.match(app, /let resourceInsertionQueue = Promise\.resolve\(\)/);
  assert.match(app, /async function insertResourceClipNow\(resource, requestedStart\)/);
  assert.match(app, /cache = await cachePublicResource\(resource\);[\s\S]*resolveResourceInsertionTrack/);
});

test("资源目录缓存键必须按当前用户隔离", () => {
  assert.match(app, /function resourceCacheUserScope\(\)/);
  assert.match(app, /fablecut-resource-cache:\$\{resourceCacheUserScope\(\)\}/);
  assert.match(app, /fablecut-resource-item:\$\{userScope\}/);
  assert.match(app, /api\/resources\/cache-session/);
});
