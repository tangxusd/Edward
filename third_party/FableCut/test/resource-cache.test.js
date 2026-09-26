const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { resourceCacheKey, validateCacheIdentity, validateCacheIndex } = require("../resource-cache.js");
const { extractZipBuffer } = require("../zip-extract.js");

function storedZip(name, bytes) {
  const fileName = Buffer.from(name);
  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt32LE(bytes.length, 18);
  local.writeUInt32LE(bytes.length, 22);
  local.writeUInt16LE(fileName.length, 26);
  const central = Buffer.alloc(46);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt16LE(20, 4);
  central.writeUInt16LE(20, 6);
  central.writeUInt32LE(bytes.length, 20);
  central.writeUInt32LE(bytes.length, 24);
  central.writeUInt16LE(fileName.length, 28);
  const centralStart = local.length + fileName.length + bytes.length;
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(1, 8);
  end.writeUInt16LE(1, 10);
  end.writeUInt32LE(central.length + fileName.length, 12);
  end.writeUInt32LE(centralStart, 16);
  return Buffer.concat([local, fileName, bytes, central, fileName, end]);
}

test("缓存索引必须同时锁定组件、版本、内容哈希与完整文件集", () => {
  const valid = {
    componentId: "text.title.fade",
    version: "1.0.0",
    contentHash: "a".repeat(64),
    files: ["manifest.json", "package.zip", "preview.mp4"],
    fileHashes: { "manifest.json": "b".repeat(64), "package.zip": "c".repeat(64), "preview.mp4": "d".repeat(64) },
    fileMetadata: { "manifest.json": { size: 1, mtimeMs: 1 }, "package.zip": { size: 2, mtimeMs: 2 }, "preview.mp4": { size: 3, mtimeMs: 3 } },
  };
  assert.equal(resourceCacheKey(valid), `text.title.fade@1.0.0#${"a".repeat(64)}`);
  assert.deepEqual(validateCacheIdentity(valid), { ok: true });
  assert.deepEqual(validateCacheIndex(valid), { ok: true });
  assert.equal(validateCacheIndex({ ...valid, files: ["manifest.json", "preview.mp4"] }).ok, false);
  assert.equal(validateCacheIndex({ ...valid, contentHash: "not-a-hash" }).ok, false);
  assert.equal(validateCacheIndex({ ...valid, fileHashes: { "manifest.json": "b".repeat(64) } }).ok, false);
  assert.equal(validateCacheIndex({ ...valid, fileMetadata: { "manifest.json": { size: 1, mtimeMs: 1 } } }).ok, false);
});

test("ZIP 解包拒绝路径穿越且只能写入缓存运行目录", () => {
  const output = fs.mkdtempSync(path.join(__dirname, ".resource-cache-"));
  try {
    assert.deepEqual(extractZipBuffer(storedZip("component.js", Buffer.from("export {}")), output), ["component.js"]);
    assert.equal(fs.readFileSync(path.join(output, "component.js"), "utf8"), "export {}");
    assert.throws(() => extractZipBuffer(storedZip("../escape.js", Buffer.from("no")), output), /zip_entry_path_invalid/);
  } finally { fs.rmSync(output, { recursive: true, force: true }); }
});

test("本机缓存只使用详情接口签名地址，并核验包哈希与运行时入口", () => {
  const server = fs.readFileSync(path.join(__dirname, "..", "server.js"), "utf8");
  assert.match(server, /\/functions\/v1\/resource-detail\?resourceId=/);
  assert.match(server, /sha256\(packageBytes\) !== identity\.contentHash/);
  assert.match(server, /function validateCachedResource\(existing\)/);
  assert.match(server, /function readVerifiedResourceCache\(key\)/);
  assert.match(server, /cache_file_hash_invalid/);
  assert.match(server, /cache_preview_invalid/);
  assert.match(server, /const cache = readVerifiedResourceCache\(url\.searchParams\.get\("key"\)\);/);
  assert.match(server, /extractZipBuffer\(packageBytes, runtimeDirectory\)/);
  assert.match(server, /\/api\/resources\/cache-entry/);
});

test("缓存复用只比对文件元数据，完整哈希校验只在缓存建立或文件变化后执行", () => {
  const server = fs.readFileSync(path.join(__dirname, "..", "server.js"), "utf8");
  assert.match(server, /const resourceCacheVerification = new Map\(\)/);
  assert.match(server, /cacheFilesMatchMetadata\(existing\.directory, existing\.index\)/);
  assert.match(server, /validateCachedResource\(existing\)/);
  assert.match(server, /cacheFileMetadata/);
});

test("缓存接口返回的运行时清单包含与缓存键完全一致的组件身份", () => {
  const server = fs.readFileSync(path.join(__dirname, "..", "server.js"), "utf8");
  assert.match(server, /function runtimeWithIdentity\(runtime, identity\)/);
  assert.match(server, /manifest: runtimeWithIdentity\(existing\.runtime, identity\)/);
  assert.match(server, /manifest: runtimeWithIdentity\(runtime, identity\)/);
});
