"use strict";

const HASH_RE = /^[a-f0-9]{64}$/;
const VERSION_RE = /^\d+\.\d+\.\d+$/;
const COMPONENT_RE = /^[a-z0-9][a-z0-9._-]{2,127}$/;
const REQUIRED_FILES = new Set(["manifest.json", "package.zip", "preview.mp4"]);

function resourceCacheKey({ componentId, version, contentHash }) {
  return `${componentId}@${version}#${contentHash}`;
}

function safeRelativePath(value) {
  return typeof value === "string" && value.length > 0 && !value.startsWith("/") && !value.includes("\\") && !value.split("/").some((part) => !part || part === "." || part === "..");
}

function validateCacheIdentity(index) {
  if (!index || !COMPONENT_RE.test(String(index.componentId || ""))) return { ok: false, reason: "invalid_component_id" };
  if (!VERSION_RE.test(String(index.version || ""))) return { ok: false, reason: "invalid_version" };
  if (!HASH_RE.test(String(index.contentHash || ""))) return { ok: false, reason: "invalid_content_hash" };
  return { ok: true };
}

function validateCacheIndex(index, { requireMetadata = true } = {}) {
  const identity = validateCacheIdentity(index);
  if (!identity.ok) return identity;
  if (!Array.isArray(index.files) || !index.files.every(safeRelativePath)) return { ok: false, reason: "invalid_files" };
  const files = new Set(index.files);
  for (const required of REQUIRED_FILES) if (!files.has(required)) return { ok: false, reason: `missing_${required}` };
  if (!index.fileHashes || typeof index.fileHashes !== "object" || Array.isArray(index.fileHashes)) return { ok: false, reason: "invalid_file_hashes" };
  const hashFiles = Object.keys(index.fileHashes);
  if (hashFiles.length !== files.size || hashFiles.some((file) => !files.has(file) || !HASH_RE.test(String(index.fileHashes[file] || "")))) return { ok: false, reason: "invalid_file_hashes" };
  if (!index.fileMetadata && !requireMetadata) return { ok: true };
  if (!index.fileMetadata || typeof index.fileMetadata !== "object" || Array.isArray(index.fileMetadata)) return { ok: false, reason: "invalid_file_metadata" };
  const metadataFiles = Object.keys(index.fileMetadata);
  if (metadataFiles.length !== files.size || metadataFiles.some((file) => !files.has(file) || !validFileMetadata(index.fileMetadata[file]))) return { ok: false, reason: "invalid_file_metadata" };
  return { ok: true };
}

function validFileMetadata(value) {
  return !!value && Number.isInteger(value.size) && value.size >= 0 && Number.isFinite(value.mtimeMs) && value.mtimeMs >= 0;
}

function cacheFileMetadata(file) {
  const stat = require("fs").statSync(file);
  return { size: stat.size, mtimeMs: stat.mtimeMs };
}

function cacheFilesMatchMetadata(directory, index) {
  return index.files.every((relativePath) => {
    const file = require("path").join(directory, relativePath);
    if (!file.startsWith(directory + require("path").sep) || !require("fs").existsSync(file)) return false;
    const actual = cacheFileMetadata(file);
    const expected = index.fileMetadata[relativePath];
    return actual.size === expected.size && actual.mtimeMs === expected.mtimeMs;
  });
}

function cacheDirectory(root, { componentId, version, contentHash }) {
  return require("path").join(root, encodeURIComponent(componentId), `${version}-${contentHash}`);
}

module.exports = { resourceCacheKey, safeRelativePath, validateCacheIdentity, validateCacheIndex, cacheFileMetadata, cacheFilesMatchMetadata, cacheDirectory };
