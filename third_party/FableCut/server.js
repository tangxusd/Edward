/* ═══════════════════════════════════════════════════════════════════════════
   FableCut server — zero-dependency Node.js
   Run:  node server.js   →  http://localhost:7777

   Adds to the browser editor:
     • persistent project      ./project.json      (GET/PUT /api/project)
     • media library folder    ./media/            (served at /media/*, POST /api/upload,
                                                    POST /api/import-url)
     • live reload             GET /api/events     (SSE: event "change" for
                                                    project/media/library;
                                                    event "profiles" for
                                                    encoding-profiles.json)

   Automation: any tool (e.g. Claude Code) can edit project.json or drop files
   into ./media — the browser UI reloads instantly. Schema: see CLAUDE.md.
   ═══════════════════════════════════════════════════════════════════════════ */
"use strict";
const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { spawn, spawnSync, execFile } = require("child_process");

const { analyze } = require("./analyze");
const {
  PROFILES_FILE,
  loadEncodeProfiles,
  invalidateEncodeProfiles,
  resolveProfile,
  listProfilesPublic,
  profileSummary,
  buildExportArgs,
  dryRunProfile,
} = require("./encode-profiles");
const { downloadImportUrl, maybeFaststart } = require("./import-url");
const { resourceCacheKey, validateCacheIdentity, validateCacheIndex, cacheFileMetadata, cacheFilesMatchMetadata, cacheDirectory, safeRelativePath } = require("./resource-cache");
const { extractZipBuffer } = require("./zip-extract");

const {
  APP_DIR, DATA_DIR, MEDIA_DIR, EXPORTS_DIR, ANALYSIS_DIR, RESOURCE_CACHE_DIR, LIBRARY_DIR,
  PROJECT_FILE, LIBRARY_SUBDIRS, COMPONENTS_DIR, ensureDirs,
} = require("./paths");
// macOS metadata files (for example .DS_Store and ._image.svg) are never user assets.
function isVisibleResourceName(name) {
  const base = path.basename(String(name || ""));
  return !!base && !base.startsWith(".");
}

/* Static app files are served from the install dir; everything the user creates
   lives under DATA_DIR. The two are the same unless FABLECUT_DATA_DIR is set. */
const ROOT = APP_DIR;
const PORT = process.env.PORT || 7777;
const HOST = process.env.HOST || "127.0.0.1";
const SUPABASE_URL = String(process.env.SUPABASE_URL || "").replace(/\/$/, "");
const SUPABASE_ANON_KEY = process.env.SUPABASE_ANON_KEY || process.env.SUPABASE_PUBLISHABLE_KEY || "";
const resourceCacheJobs = new Map();
const resourceCacheVerification = new Map();
const resourceCacheSessions = new Map();

/* Requests must come from the local machine (or an explicitly allowed host).
   The Host check stops DNS rebinding; the Origin check stops malicious web
   pages firing blind cross-origin writes at the API. Opt into LAN use with
   HOST=0.0.0.0 and FABLECUT_ALLOWED_HOSTS=192.168.1.20,mybox.local */
const ALLOWED_HOSTS = new Set(["localhost", "127.0.0.1", "[::1]", "::1", HOST.toLowerCase()]);
for (const h of (process.env.FABLECUT_ALLOWED_HOSTS || "").split(","))
  if (h.trim()) ALLOWED_HOSTS.add(h.trim().toLowerCase());

function hostAllowed(value) {
  if (!value) return false;
  // strip a :port suffix, but not the colons inside a bare IPv6 address
  const host = value.replace(/^(\[[^\]]*\]|[^:]+)(:\d+)?$/, "$1").toLowerCase();
  return ALLOWED_HOSTS.has(host);
}
function requestAllowed(req) {
  if (!hostAllowed(req.headers.host)) return false;
  const origin = req.headers.origin;
  if (origin) {
    try { return hostAllowed(new URL(origin).host); } catch { return false; }
  }
  return true;
}

/* ffmpeg powers optional niceties (faststart remux on upload, fast export).
   Everything else works without it. */
let HAS_FFMPEG = false;
try { HAS_FFMPEG = spawnSync("ffmpeg", ["-version"], { stdio: "ignore" }).status === 0; } catch {}

const MIME = {
  ".html": "text/html; charset=utf-8", ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8", ".json": "application/json",
  ".mp4": "video/mp4", ".webm": "video/webm", ".mov": "video/quicktime",
  ".mkv": "video/x-matroska", ".m4v": "video/mp4",
  ".mp3": "audio/mpeg", ".wav": "audio/wav", ".ogg": "audio/ogg",
  ".m4a": "audio/mp4", ".aac": "audio/aac", ".flac": "audio/flac",
  ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg",
  ".gif": "image/gif", ".webp": "image/webp", ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
  ".ttf": "font/ttf", ".otf": "font/otf", ".woff": "font/woff", ".woff2": "font/woff2",
};

ensureDirs();
fs.mkdirSync(RESOURCE_CACHE_DIR, { recursive: true });
if (!fs.existsSync(PROJECT_FILE)) {
  fs.writeFileSync(PROJECT_FILE, JSON.stringify({
    name: "Untitled Project", width: 1280, height: 720, fps: 30,
    revision: 0, media: [], clips: [],
  }, null, 2));
}

/* ── SSE clients + file watching ── */
const sseClients = new Set();
function broadcast(event = "change") {
  const payload = `event: ${event}\ndata: ${event}\n\n`;
  for (const res of sseClients) res.write(payload);
}
let debounce = null;
let profilesDebounce = null;
function onFsChange() {
  clearTimeout(debounce);
  debounce = setTimeout(() => broadcast("change"), 150);
}
function onProfilesChange() {
  invalidateEncodeProfiles();
  clearTimeout(profilesDebounce);
  profilesDebounce = setTimeout(() => broadcast("profiles"), 150);
}
/* watch the directory, not the file — atomic tmp+rename writes would detach a
   direct file watcher on Windows */
if (process.env.FABLECUT_NO_FS_WATCH !== "1") {
  try { fs.watch(DATA_DIR, (ev, f) => { if (f === "project.json") onFsChange(); }); } catch {}
  try {
    fs.watch(ROOT, (ev, f) => {
      if (f === path.basename(PROFILES_FILE)) onProfilesChange();
    });
  } catch {}
  try { fs.watch(MEDIA_DIR, onFsChange); } catch {}
  for (const d of LIBRARY_SUBDIRS) {
    try { fs.watch(path.join(LIBRARY_DIR, d), onFsChange); } catch {}
  }
}

/* ── Helpers ── */
function safeName(name) {
  return name.replace(/[^\w.\- ()\[\]]+/g, "_").slice(0, 120) || "file";
}
function sendJSON(res, code, obj, headers = {}) {
  const body = JSON.stringify(obj);
  res.writeHead(code, { "Content-Type": "application/json", "Cache-Control": "no-store", ...headers });
  res.end(body);
}
function readComponentManifest(id) {
  const dir = path.join(COMPONENTS_DIR, id);
  if (!dir.startsWith(COMPONENTS_DIR + path.sep)) return null;
  const legacyFile = path.join(dir, "manifest.json");
  const runtimeFile = path.join(dir, "edward-runtime.json");
  let legacy = null, runtime = null;
  try { if (fs.existsSync(legacyFile)) legacy = JSON.parse(fs.readFileSync(legacyFile, "utf8")); } catch {}
  try { if (fs.existsSync(runtimeFile)) runtime = JSON.parse(fs.readFileSync(runtimeFile, "utf8")); } catch {}
  if (runtime?.protocol === "edward.web-runtime.v1" && typeof runtime.previewEntry === "string" &&
      typeof runtime.renderEntry === "string" && typeof runtime.runtime === "string") {
    return { ...(legacy || {}), ...runtime, id, name: legacy?.name || id, category: legacy?.category || "annotation" };
  }
  return legacy && typeof legacy.name === "string" && typeof legacy.entry === "string" ? { ...legacy, id } : null;
}
function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on("data", (c) => chunks.push(c));
    req.on("end", () => resolve(Buffer.concat(chunks)));
    req.on("error", reject);
  });
}
/** Claim final + sibling `.part` paths with exclusive create (`wx`) so concurrent
 *  exports cannot pick the same free name before either file exists. */
function reserveExportPaths(dir, baseName, ext) {
  const base = baseName.replace(/\.(mp4|mov|m4v|mkv|webm)$/i, "");
  let i = 0;
  for (;;) {
    const stem = i === 0 ? base : `${base}_${i}`;
    i++;
    const outPath = path.join(dir, stem + ext);
    const partPath = path.join(dir, stem + ".part" + ext);
    if (fs.existsSync(outPath)) continue;
    try {
      fs.closeSync(fs.openSync(partPath, "wx")); // exclusive create — claim the name
    } catch (e) {
      if (e.code === "EEXIST") continue;
      throw e;
    }
    if (fs.existsSync(outPath)) {
      try { fs.rmSync(partPath, { force: true }); } catch { }
      continue;
    }
    return { outPath, partPath };
  }
}
function finalizeExportPath(sess) {
  const dir = path.dirname(sess.outPath);
  const ext = path.extname(sess.outPath);
  const base = path.basename(sess.outPath, ext);
  for (let i = 0; ; i++) {
    const stem = i === 0 ? base : `${base}_${i}`;
    const out = path.join(dir, stem + ext);
    try {
      fs.copyFileSync(sess.partPath, out, fs.constants.COPYFILE_EXCL);
      fs.rmSync(sess.partPath, { force: true });
      return out;
    } catch (e) {
      if (e.code === "EEXIST") continue;
      throw e;
    }
  }
}
function previewExportName(baseName, ext = ".mp4") {
  const base = safeName(baseName || "export").replace(/\.(mp4|mov|m4v|mkv|webm)$/i, "");
  for (let i = 0; ; i++) {
    const stem = i === 0 ? base : `${base}_${i}`;
    const name = stem + ext;
    if (!fs.existsSync(path.join(EXPORTS_DIR, name)) && !fs.existsSync(path.join(EXPORTS_DIR, stem + ".part" + ext)))
      return { requestedName: base + ext, available: i === 0, name };
  }
}
function run(cmd, args) {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { maxBuffer: 1 << 24 }, (err, _out, stderr) =>
      err ? reject(new Error((stderr || String(err)).slice(-800))) : resolve());
  });
}
async function supabaseProxy(pathname, req, body) {
  if (!SUPABASE_URL || !SUPABASE_ANON_KEY) throw new Error("supabase_not_configured");
  const headers = { apikey: SUPABASE_ANON_KEY, Accept: "application/json",
    Authorization: req.headers.authorization || `Bearer ${SUPABASE_ANON_KEY}` };
  if (body !== undefined) headers["Content-Type"] = "application/json";
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 30000);
  let response;
  try {
    response = await fetch(SUPABASE_URL + pathname, { method: body === undefined ? "GET" : "POST", headers, body: body === undefined ? undefined : JSON.stringify(body), signal: controller.signal });
  } catch (error) {
    if (error?.name === "AbortError") return { status: 504, value: { error: "supabase_timeout", path: pathname } };
    throw error;
  } finally { clearTimeout(timeout); }
  const text = await response.text();
  let value; try { value = JSON.parse(text); } catch { value = { error: text.slice(0, 300) }; }
  return { status: response.status, value };
}
async function supabaseAuth(pathname, body) {
  if (!SUPABASE_URL || !SUPABASE_ANON_KEY) throw new Error("supabase_not_configured");
  const response = await fetch(SUPABASE_URL + pathname, { method: "POST", headers: { apikey: SUPABASE_ANON_KEY, "Content-Type": "application/json", Accept: "application/json" }, body: JSON.stringify(body) });
  const text = await response.text();
  let value; try { value = JSON.parse(text); } catch { value = { error: "auth_unavailable" }; }
  return { status: response.status, value };
}

function resourceCacheLog(event, fields = {}) {
  const record = { time: new Date().toISOString(), event, ...fields };
  const line = JSON.stringify(record) + "\n";
  try { fs.appendFileSync(path.join(DATA_DIR, "resource-cache.log"), line, { mode: 0o600 }); } catch {}
  console.log(`resource ${event} ${Object.entries(fields).map(([key, value]) => `${key}=${String(value).replace(/[\r\n ]/g, "_").slice(0, 160)}`).join(" ")}`.trim());
}
function resourceRequestId() { return crypto.randomBytes(12).toString("hex"); }
function resourceCacheSession(req) {
  const match = /(?:^|;\s*)fablecut-resource-session=([a-f0-9]{48})(?:;|$)/.exec(String(req.headers.cookie || ""));
  if (!match) return false;
  const expiresAt = resourceCacheSessions.get(match[1]);
  if (!expiresAt || expiresAt < Date.now()) { resourceCacheSessions.delete(match[1]); return false; }
  return true;
}
function issueResourceCacheSession() {
  const token = crypto.randomBytes(24).toString("hex");
  resourceCacheSessions.set(token, Date.now() + 15 * 60 * 1000);
  return token;
}
function safeResourceReason(error) {
  const reason = String(error?.message || error || "internal_error");
  return /^[a-z0-9_:-]{1,120}$/i.test(reason) ? reason : "internal_error";
}
function sha256(bytes) { return crypto.createHash("sha256").update(bytes).digest("hex"); }
function parseResourceCacheKey(key) {
  const match = /^([a-z0-9][a-z0-9._-]{2,127})@(\d+\.\d+\.\d+)#([a-f0-9]{64})$/.exec(String(key || ""));
  return match ? { componentId: match[1], version: match[2], contentHash: match[3] } : null;
}
function readResourceCache(key) {
  const identity = parseResourceCacheKey(key);
  if (!identity) return null;
  const directory = cacheDirectory(RESOURCE_CACHE_DIR, identity);
  const indexPath = path.join(directory, "cache.json");
  try {
    const index = JSON.parse(fs.readFileSync(indexPath, "utf8"));
    if (!validateCacheIndex(index, { requireMetadata: false }).ok || resourceCacheKey(index) !== key) return null;
    return { identity, directory, index };
  } catch { return null; }
}
async function fetchSignedResource(url) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 30000);
  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) throw new Error("resource_download_failed");
    return Buffer.from(await response.arrayBuffer());
  } finally { clearTimeout(timer); }
}
function verifyRuntimeManifest(runtime, packageManifest) {
  const runtimes = new Set(["react", "html-css", "gsap", "svg"]);
  if (!runtime || runtime.protocol !== "edward.web-runtime.v1" || !runtimes.has(runtime.runtime)) throw new Error("runtime_manifest_invalid");
  if (!safeRelativePath(runtime.previewEntry) || !safeRelativePath(runtime.renderEntry)) throw new Error("runtime_entry_invalid");
  const timeline = packageManifest?.timeline;
  const runtimeProperties = Array.isArray(runtime.editableProperties) ? runtime.editableProperties : [];
  const runtimeTracks = Array.isArray(runtime.editableTracks) ? runtime.editableTracks : [];
  const sameList = (a, b) => Array.isArray(a) && Array.isArray(b) && a.length === b.length && a.every((value, index) => value === b[index]);
  const schemaValid = runtime.propsSchema && runtime.propsSchema.type === "object" && runtime.propsSchema.additionalProperties === false && runtime.propsSchema.properties && typeof runtime.propsSchema.properties === "object";
  const propertiesComplete = schemaValid && [...runtimeProperties, ...runtimeTracks.map((track) => String(track).split(".")[0])].every((property) => Object.hasOwn(runtime.propsSchema.properties, property));
  if (!Number.isInteger(runtime.fps) || !Number.isInteger(runtime.durationInFrames) || !timeline || runtime.fps !== timeline.authoringFps || runtime.durationInFrames !== timeline.durationFrames || !sameList(runtimeProperties, packageManifest.editableProperties) || !sameList(runtimeTracks, packageManifest.editableTracks) || !schemaValid || !propertiesComplete || !runtime.capabilities?.preview || !runtime.capabilities?.export) throw new Error("runtime_contract_invalid");
  const assets = Array.isArray(packageManifest.assets) ? packageManifest.assets : [];
  const allowed = new Set(["edward-runtime.json", runtime.previewEntry, runtime.renderEntry]);
  for (const asset of assets) {
    if (!asset || !safeRelativePath(asset.path) || !/^[a-f0-9]{64}$/.test(String(asset.sha256 || ""))) throw new Error("asset_manifest_invalid");
    allowed.add(asset.path);
  }
  return allowed;
}
function validateExtractedAssets(runtimeDir, packageManifest, allowedPaths) {
  for (const asset of packageManifest.assets || []) {
    const file = path.join(runtimeDir, asset.path);
    if (!file.startsWith(runtimeDir + path.sep) || !fs.existsSync(file) || fs.statSync(file).size !== asset.bytes || sha256(fs.readFileSync(file)) !== asset.sha256) throw new Error("asset_hash_invalid");
  }
  for (const entry of allowedPaths) {
    const file = path.join(runtimeDir, entry);
    if (!file.startsWith(runtimeDir + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) throw new Error("runtime_entry_missing");
  }
}
function validateCachedResource(existing) {
  const { directory, identity, index } = existing;
  for (const relativePath of index.files) {
    const file = path.join(directory, relativePath);
    if (!file.startsWith(directory + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile() || sha256(fs.readFileSync(file)) !== index.fileHashes[relativePath])
      throw new Error("cache_file_hash_invalid");
  }
  const packageBytes = fs.readFileSync(path.join(directory, "package.zip"));
  if (sha256(packageBytes) !== identity.contentHash) throw new Error("cache_package_hash_invalid");
  const packageManifest = JSON.parse(fs.readFileSync(path.join(directory, "manifest.json"), "utf8"));
  if (packageManifest.component_id !== identity.componentId || packageManifest.version !== identity.version) throw new Error("cache_manifest_mismatch");
  const runtimeDirectory = path.join(directory, "runtime");
  const runtime = JSON.parse(fs.readFileSync(path.join(runtimeDirectory, "edward-runtime.json"), "utf8"));
  const allowedPaths = verifyRuntimeManifest(runtime, packageManifest);
  validateExtractedAssets(runtimeDirectory, packageManifest, allowedPaths);
  const preview = fs.readFileSync(path.join(directory, "preview.mp4"));
  if (preview.length < 12 || preview.subarray(4, 8).toString("ascii") !== "ftyp") throw new Error("cache_preview_invalid");
  return runtime;
}
function runtimeWithIdentity(runtime, identity) {
  return { ...runtime, componentId: identity.componentId, version: identity.version, contentHash: identity.contentHash };
}
function readVerifiedResourceCache(key) {
  const existing = readResourceCache(key);
  if (!existing) return null;
  try {
    const verified = resourceCacheVerification.get(key);
    if (verified && cacheFilesMatchMetadata(existing.directory, existing.index)) return { ...existing, runtime: verified.runtime };
    const runtime = validateCachedResource(existing);
    existing.index.fileMetadata = Object.fromEntries(existing.index.files.map((relativePath) => [relativePath, cacheFileMetadata(path.join(existing.directory, relativePath))]));
    fs.writeFileSync(path.join(existing.directory, "cache.json"), JSON.stringify(existing.index));
    resourceCacheVerification.set(key, { runtime });
    return { ...existing, runtime };
  } catch (error) {
    fs.rmSync(existing.directory, { recursive: true, force: true });
    resourceCacheVerification.delete(key);
    resourceCacheLog("resource_cache_invalidated", { componentId: existing.identity.componentId, version: existing.identity.version, contentHash: existing.identity.contentHash, reason: safeResourceReason(error) });
    return null;
  }
}
async function cacheResourcePackage(req, request, requestId) {
  const identity = { componentId: request.componentId, version: request.version, contentHash: request.contentHash };
  if (typeof request.resourceId !== "string" || !validateCacheIdentity(identity).ok) throw new Error("invalid_cache_request");
  const detail = await supabaseProxy(`/functions/v1/resource-detail?resourceId=${encodeURIComponent(request.resourceId)}&version=${encodeURIComponent(identity.version)}&contentHash=${encodeURIComponent(identity.contentHash)}`, req);
  if (detail.status !== 200 || !detail.value?.resource || !detail.value?.version) throw new Error("resource_detail_unavailable");
  if (detail.value.resource.component_id !== identity.componentId || detail.value.resource.target !== "web.runtime" || detail.value.version.version !== identity.version || detail.value.version.content_hash !== identity.contentHash) throw new Error("resource_identity_mismatch");
  const key = resourceCacheKey(identity);
  const existing = readVerifiedResourceCache(key);
  if (existing) {
    return { state: "ready", key, previewUrl: `/api/resources/cache-preview?key=${encodeURIComponent(key)}`, manifest: runtimeWithIdentity(existing.runtime, identity) };
  }
  if (resourceCacheJobs.has(key)) return resourceCacheJobs.get(key);
  const task = (async () => {
    const [manifestBytes, packageBytes, previewBytes] = await Promise.all([fetchSignedResource(detail.value.manifestUrl), fetchSignedResource(detail.value.packageUrl), fetchSignedResource(detail.value.previewUrl)]);
    if (sha256(packageBytes) !== identity.contentHash) throw new Error("package_hash_invalid");
    const packageManifest = JSON.parse(manifestBytes.toString("utf8"));
    if (packageManifest.component_id !== identity.componentId || packageManifest.version !== identity.version) throw new Error("package_manifest_mismatch");
    const finalDirectory = cacheDirectory(RESOURCE_CACHE_DIR, identity);
    const temporaryDirectory = `${finalDirectory}.tmp-${process.pid}-${crypto.randomBytes(6).toString("hex")}`;
    try {
      fs.mkdirSync(temporaryDirectory, { recursive: false });
      fs.writeFileSync(path.join(temporaryDirectory, "manifest.json"), manifestBytes, { flag: "wx" });
      fs.writeFileSync(path.join(temporaryDirectory, "package.zip"), packageBytes, { flag: "wx" });
      fs.writeFileSync(path.join(temporaryDirectory, "preview.mp4"), previewBytes, { flag: "wx" });
      const runtimeDirectory = path.join(temporaryDirectory, "runtime");
      fs.mkdirSync(runtimeDirectory);
      const extracted = extractZipBuffer(packageBytes, runtimeDirectory);
      const runtimePath = path.join(runtimeDirectory, "edward-runtime.json");
      const runtime = JSON.parse(fs.readFileSync(runtimePath, "utf8"));
      const allowedPaths = verifyRuntimeManifest(runtime, packageManifest);
      validateExtractedAssets(runtimeDirectory, packageManifest, allowedPaths);
      const files = ["manifest.json", "package.zip", "preview.mp4", ...extracted.map((entry) => `runtime/${entry}`)];
      const fileHashes = Object.fromEntries(files.map((relativePath) => [relativePath, sha256(fs.readFileSync(path.join(temporaryDirectory, relativePath)))]));
      const fileMetadata = Object.fromEntries(files.map((relativePath) => [relativePath, cacheFileMetadata(path.join(temporaryDirectory, relativePath))]));
      const index = { ...identity, resourceId: request.resourceId, files, fileHashes, fileMetadata };
      if (!validateCacheIndex(index).ok) throw new Error("cache_index_invalid");
      fs.writeFileSync(path.join(temporaryDirectory, "cache.json"), JSON.stringify(index), { flag: "wx" });
      if (!fs.existsSync(finalDirectory)) fs.renameSync(temporaryDirectory, finalDirectory);
      else fs.rmSync(temporaryDirectory, { recursive: true, force: true });
      resourceCacheVerification.set(key, { runtime });
      resourceCacheLog("resource_cache_ready", { requestId, componentId: identity.componentId, version: identity.version, contentHash: identity.contentHash });
      return { state: "ready", key, previewUrl: `/api/resources/cache-preview?key=${encodeURIComponent(key)}`, manifest: runtimeWithIdentity(runtime, identity) };
    } catch (error) {
      fs.rmSync(temporaryDirectory, { recursive: true, force: true });
      resourceCacheLog("resource_cache_failed", { requestId, componentId: identity.componentId, version: identity.version, contentHash: identity.contentHash, reason: safeResourceReason(error) });
      throw error;
    }
  })();
  resourceCacheJobs.set(key, task);
  try { return await task; } finally { resourceCacheJobs.delete(key); }
}

function clearResourceCache() {
  fs.rmSync(RESOURCE_CACHE_DIR, { recursive: true, force: true });
  fs.mkdirSync(RESOURCE_CACHE_DIR, { recursive: true, mode: 0o700 });
  resourceCacheVerification.clear();
  resourceCacheJobs.clear();
  resourceCacheSessions.clear();
}

/* Remux MP4-family uploads with `+faststart` so the moov atom leads the file —
   without it <video> stalls for seconds probing over Range requests. */
function faststart(file) { return maybeFaststart(file); }

/* ── Export sessions ──
   Two modes share the same HTTP session API:
     jpeg   — browser streams JPEGs; ffmpeg encodes via an encoding profile (Fast)
     annexb — browser streams Annex-B H.264 (one POST may concatenate several AUs);
              ffmpeg stream-copies (WebCodecs)
   Both spawn on the FIRST frame, not here: the audio mix is uploaded between
   /begin and the first frame, and a one-pass encode needs it on disk. */
const exportSessions = new Map();
/* Container + bitstream BT.709/tv for the WebCodecs (annexb) path.
   `-c copy` alone drops the colr atom; the bsf writes VUI so players see
   primaries/transfer, not "unknown". JPEG Fast export uses profile.color instead. */
const COLOR_TAGS = [
  "-colorspace", "bt709", "-color_primaries", "bt709",
  "-color_trc", "bt709", "-color_range", "tv",
];
const COLOR_BSF = "h264_metadata=colour_primaries=1:transfer_characteristics=1:matrix_coefficients=1:video_full_range_flag=0";
const EXPORT_IDLE_MS = 10 * 60 * 1000; // abandon sessions with no successful activity
const EXPORT_SWEEP_MS = 60 * 1000;
let exportSweepTimer = null;
function touchExport(sess) {
  if (sess) sess.lastTouch = Date.now();
}
function attachProc(sess, proc) {
  sess.proc = proc;
  sess.stderr = "";
  proc.stderr.on("data", (d) => { sess.stderr = (sess.stderr + d).slice(-2000); });
  proc.stdin.on("error", () => {}); // EPIPE if ffmpeg dies mid-stream
  sess.done = new Promise((res) => proc.on("close", res));
}
async function beginExport(fps, name, profileId, hasAudio, mode) {
  const m = mode === "annexb" ? "annexb" : "jpeg";
  const rate = Number(fps);
  if (!Number.isFinite(rate) || rate <= 0) {
    throw new Error("export fps required (pass project.fps)");
  }
  const id = Date.now().toString(36) + Math.random().toString(36).slice(2, 7);
  const safe = safeName(name || "export");
  // Same filesystem as the finished file so renameSync(partPath, out) cannot EXDEV.
  const dir = fs.mkdtempSync(path.join(EXPORTS_DIR, "fablecut-"));

  if (m === "annexb") {
    const { outPath, partPath } = reserveExportPaths(EXPORTS_DIR, safe, ".mp4");
    const sess = {
      mode: m, fps: rate, proc: null, dir,
      name: safe, hasAudio: !!hasAudio,
      wav: null, partPath, outPath,
      stderr: "", done: null, lastTouch: Date.now(),
      err: () => sess.stderr.trim().split("\n").filter(Boolean).slice(-3)
        .map((l) => l.trim()).join(" · "),
    };
    sess.writeLock = Promise.resolve();
    exportSessions.set(id, sess);
    return { id, mode: m };
  }

  const profile = resolveProfile(profileId);
  const dry = await dryRunProfile(profile, { fps: rate, hasAudio });
  if (!dry.ok) throw new Error(`profile "${profile.id}" was rejected by ffmpeg: ${dry.error}`);
  // Reserve the output name now (not at first frame) so concurrent exports
  // cannot both see the same free path. The empty .part file is overwritten by ffmpeg (-y).
  const { outPath, partPath } = reserveExportPaths(EXPORTS_DIR, safe, profile.extension);
  const sess = {
    mode: m, proc: null, fps: rate, profile, name: safe, hasAudio: !!hasAudio,
    dir, wav: null, partPath, outPath,
    stderr: "", done: null, lastTouch: Date.now(),
    err: () => sess.stderr.trim().split("\n").filter(Boolean).slice(-3)
      .map((l) => l.trim()).join(" · "),
  };
  sess.writeLock = Promise.resolve();
  exportSessions.set(id, sess);
  return { id, mode: m, profile: profile.id, label: profile.label, summary: profileSummary(profile) };
}
/* Encode into the paths reserved at /begin; rename .part → final on clean exit
   so an aborted render never leaves something that looks like a finished file. */
function startEncoder(sess) {
  const proc = spawn("ffmpeg", buildExportArgs(sess.profile, {
    fps: sess.fps, wavPath: sess.wav, outPath: sess.partPath,
  }), { stdio: ["pipe", "ignore", "pipe"] });
  proc.stderr.on("data", (d) => { sess.stderr = (sess.stderr + d).slice(-2000); });
  // EPIPE on end()/late writes is normal once ffmpeg has exited; writeExportFrame
  // attaches its own error listener while a backpressured write is in flight.
  proc.stdin.on("error", () => { });
  sess.proc = proc;
  sess.done = new Promise((res) => proc.on("close", res));
  return proc;
}
/** Write one frame (JPEG or Annex-B NAL) to ffmpeg stdin. If the pipe is full,
 *  wait for drain — but also reject if ffmpeg exits or the stdin errors, so the
 *  HTTP request cannot hang forever after a failed encode. */
function writeExportFrame(sess, body) {
  const proc = sess.proc;
  return new Promise((resolve, reject) => {
    if (!proc || proc.exitCode !== null || proc.killed)
      return reject(new Error("ffmpeg exited: " + sess.err()));

    let settled = false;
    const finish = (fn, arg) => {
      if (settled) return;
      settled = true;
      proc.off("close", onClose);
      proc.stdin.off("error", onErr);
      proc.stdin.off("drain", onDrain);
      fn(arg);
    };
    const onClose = () => finish(reject, new Error("ffmpeg exited: " + (sess.err() || "closed")));
    const onErr = (e) => finish(reject, new Error(e?.message || "ffmpeg stdin error"));
    const onDrain = () => finish(resolve);

    proc.once("close", onClose);
    proc.stdin.once("error", onErr);
    let ok;
    try { ok = proc.stdin.write(body); }
    catch (e) { return finish(reject, e); }
    if (proc.exitCode !== null)
      return finish(reject, new Error("ffmpeg exited: " + sess.err()));
    if (ok) finish(resolve);
    else proc.stdin.once("drain", onDrain);
  });
}
/* One-pass mux for WebCodecs: H.264 elementary stream on stdin + optional WAV.
   Use input `-r` (not only `-framerate`): HW encoders stamp AUs with µs-rounded
   durations (e.g. 33333µs ≈ 1/30), which otherwise become avg_frame_rate
   1000000/33333. `-r` forces CFR PTS so the MP4 matches project.fps exactly. */
function startAnnexbEncoder(sess) {
  const fps = sess.fps;
  const args = [
    "-y", "-hide_banner",
    "-fflags", "+genpts",
    "-f", "h264", "-r", String(fps), "-i", "pipe:0",
  ];
  if (sess.wav) args.push("-i", sess.wav);
  args.push("-map", "0:v:0", "-c:v", "copy", "-bsf:v", COLOR_BSF, ...COLOR_TAGS);
  // do not use -shortest: with unset/generated PTS it drops the audio track
  if (sess.wav) args.push("-map", "1:a:0", "-c:a", "aac", "-b:a", "192k");
  args.push("-movflags", "+faststart+write_colr", sess.partPath);
  attachProc(sess, spawn("ffmpeg", args, { stdio: ["pipe", "ignore", "pipe"] }));
  return sess.proc;
}
function cleanupExport(id) {
  const s = exportSessions.get(id);
  if (!s) return;
  exportSessions.delete(id);
  try { s.proc?.kill(); } catch {}
  try { fs.rmSync(s.dir, { recursive: true, force: true }); } catch {}
  if (s.partPath) try { fs.rmSync(s.partPath, { force: true }); } catch {}
}
function sweepIdleExports() {
  const now = Date.now();
  for (const [id, s] of [...exportSessions]) {
    if (now - (s.lastTouch || 0) > EXPORT_IDLE_MS) cleanupExport(id);
  }
}
function startExportSweep() {
  if (exportSweepTimer) return;
  exportSweepTimer = setInterval(sweepIdleExports, EXPORT_SWEEP_MS);
}
function stopExportSweep() {
  if (!exportSweepTimer) return;
  clearInterval(exportSweepTimer);
  exportSweepTimer = null;
}

/* Static file with HTTP Range support (required for <video> seeking) */
function serveFile(req, res, filePath) {
  fs.stat(filePath, (err, st) => {
    if (err || !st.isFile()) { res.writeHead(404); res.end("Not found"); return; }
    const ext = path.extname(filePath).toLowerCase();
    const type = MIME[ext] || "application/octet-stream";
    const extra = {};
    if (ext === ".svg") {
      extra["Content-Security-Policy"] = "sandbox; default-src 'none'; style-src 'unsafe-inline'";
      extra["X-Content-Type-Options"] = "nosniff";
    }
    const range = req.headers.range;
    if (range) {
      const m = /bytes=(\d*)-(\d*)/.exec(range);
      let start = m && m[1] ? parseInt(m[1]) : 0;
      let end = m && m[2] ? parseInt(m[2]) : st.size - 1;
      start = Math.min(start, st.size - 1); end = Math.min(end, st.size - 1);
      res.writeHead(206, {
        "Content-Type": type, "Accept-Ranges": "bytes",
        "Content-Range": `bytes ${start}-${end}/${st.size}`,
        "Content-Length": end - start + 1, ...extra,
      });
      fs.createReadStream(filePath, { start, end }).pipe(res);
    } else {
      res.writeHead(200, {
        "Content-Type": type, "Content-Length": st.size,
        "Accept-Ranges": "bytes", "Cache-Control": "no-cache", ...extra,
      });
      fs.createReadStream(filePath).pipe(res);
    }
  });
}

/* ── Server ── */
const server = http.createServer(async (req, res) => {
  if (!requestAllowed(req)) {
    sendJSON(res, 403, { error: "forbidden: request must come from this machine (bad Host or Origin header)" });
    return;
  }
  const url = new URL(req.url, "http://localhost");
  const p = decodeURIComponent(url.pathname);
  if ((p === "/auth/recovery" || p === "/auth/confirmed") && req.method === "GET") {
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
    res.end("<!doctype html><meta charset=utf-8><title>Orbit</title><body style=\"background:#111;color:#eee;font:16px sans-serif;padding:32px\">请返回 Orbit 应用完成账户操作。</body>");
    return;
  }
  // never serve dotfiles/dot-directories (.git, .gitignore, …)
  if (p.split(/[\\/]/).some((seg) => seg.startsWith("."))) { res.writeHead(403); res.end(); return; }

  /* API: project */
  if (p === "/api/project" && req.method === "GET") {
    // strip UTF-8 BOM some editors/PowerShell prepend, which breaks JSON.parse
    try { sendJSON(res, 200, JSON.parse(fs.readFileSync(PROJECT_FILE, "utf8").replace(new RegExp("^\\uFEFF"), ""))); }
    catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }
  if (p === "/api/project" && req.method === "PUT") {
    try {
      const body = await readBody(req);
      const data = JSON.parse(body.toString("utf8")); // validate JSON
      /* Optimistic concurrency: a write whose revision isn't newer than what's
         on disk was based on a stale read (someone else — the UI or an external
         tool — saved in between). Reject it instead of clobbering their work.
         ?force=1 skips the check for deliberate overwrites. */
      let cur = {};
      try { cur = JSON.parse(fs.readFileSync(PROJECT_FILE, "utf8").replace(new RegExp("^\\uFEFF"), "")); } catch {}
      if ((data.revision || 0) <= (cur.revision || 0) && url.searchParams.get("force") !== "1") {
        sendJSON(res, 409, { error: "stale revision — project changed since it was read", revision: cur.revision || 0 });
        return;
      }
      const tmp = PROJECT_FILE + ".tmp";
      fs.writeFileSync(tmp, JSON.stringify(data, null, 2));
      fs.renameSync(tmp, PROJECT_FILE);
      sendJSON(res, 200, { ok: true, revision: data.revision });
    } catch (e) { sendJSON(res, 400, { error: String(e) }); }
    return;
  }

  if (p === "/api/resources/categories" && req.method === "GET") {
    try {
      const tabKey = url.searchParams.get("tabKey") || "";
      const q = new URLSearchParams({ select: "id,tab_key,parent_id,name,slug,sort_order,status", tab_key: `eq.${tabKey}`, status: "eq.published", order: "sort_order.asc,name.asc" });
      const result = await supabaseProxy(`/rest/v1/resource_categories?${q}`, req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if ((p === "/api/auth/login" || p === "/api/auth/signup" || p === "/api/auth/recover" || p === "/api/auth/refresh") && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const pathName = p === "/api/auth/login" ? "/functions/v1/auth-login" : p === "/api/auth/signup" ? "/functions/v1/auth-register" : p === "/api/auth/recover" ? "/auth/v1/recover" : "/auth/v1/token?grant_type=refresh_token";
      if (p === "/api/auth/recover") body.redirect_to = "https://auth.edward.uno/?flow=recovery";
      const result = await supabaseAuth(pathName, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 400, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/auth/session" && req.method === "GET") {
    try {
      const result = await supabaseProxy("/auth/v1/user", req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: "auth_unavailable" }); }
    return;
  }
  if (p === "/api/resources/cache-session" && req.method === "POST") {
    try {
      const result = await supabaseProxy("/auth/v1/user", req);
      if (result.status !== 200) { sendJSON(res, result.status, result.value); return; }
      const token = issueResourceCacheSession();
      sendJSON(res, 200, { ok: true }, { "Set-Cookie": `fablecut-resource-session=${token}; HttpOnly; SameSite=Strict; Path=/api/resources; Max-Age=900` });
    } catch { sendJSON(res, 503, { error: "auth_unavailable" }); }
    return;
  }
  if (p === "/api/auth/entitlements" && req.method === "GET") {
    try {
      const result = await supabaseProxy("/functions/v1/auth-entitlement", req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/subscription/catalog" && req.method === "GET") {
    try {
      const result = await supabaseProxy("/functions/v1/subscription-catalog", req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/subscription/banner" && req.method === "GET") {
    try {
      const query = new URLSearchParams({ select: "image_url,title,href", status: "eq.active", order: "sort_order.asc", limit: "1" });
      const result = await supabaseProxy(`/rest/v1/subscription_banners?${query}`, req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/settings/provider-presets" && req.method === "GET") {
    try {
      const result = await supabaseProxy("/functions/v1/model-provider-presets", req);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/settings/preferences" && (req.method === "GET" || req.method === "POST")) {
    try {
      const body = req.method === "POST" ? JSON.parse((await readBody(req)).toString("utf8")) : undefined;
      const result = await supabaseProxy("/functions/v1/preference-sync", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/settings/feedback" && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/desktop-feedback", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/resources/catalog" && req.method === "GET") {
    const startedAt = Date.now();
    try {
      const query = new URLSearchParams(url.searchParams);
      const result = await supabaseProxy(`/functions/v1/resource-catalog?${query}`, req);
      resourceCacheLog("resource_catalog", { tab: query.get("tabKey") || "", filter: query.get("filter") || query.get("sort") || "latest", category: query.get("categoryId") || "all", status: result.status, durationMs: Date.now() - startedAt, count: Array.isArray(result.value) ? result.value.length : Number(result.value?.items?.length || 0) });
      sendJSON(res, result.status, result.value);
    } catch (e) { resourceCacheLog("resource_catalog_error", { status: 503, durationMs: Date.now() - startedAt, reason: String(e.message || e) }); sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/resources/detail" && req.method === "GET") {
    const requestId = resourceRequestId();
    const startedAt = Date.now();
    try {
      const result = await supabaseProxy(`/functions/v1/resource-detail?${url.searchParams}`, req);
      resourceCacheLog("resource_detail", { requestId, resourceId: url.searchParams.get("resourceId") || url.searchParams.get("id") || "unknown", status: result.status, durationMs: Date.now() - startedAt });
      sendJSON(res, result.status, result.value);
    } catch (e) { resourceCacheLog("resource_detail_error", { requestId, status: 503, durationMs: Date.now() - startedAt, reason: safeResourceReason(e) }); sendJSON(res, 503, { error: "resource_detail_unavailable" }); }
    return;
  }
  if (p === "/api/resources/cache" && req.method === "POST") {
    const requestId = resourceRequestId();
    const startedAt = Date.now();
    let body = null;
    try {
      body = JSON.parse((await readBody(req)).toString("utf8"));
      sendJSON(res, 200, await cacheResourcePackage(req, body, requestId));
    } catch (e) { resourceCacheLog("resource_cache_request_error", { requestId, componentId: body?.componentId || "unknown", version: body?.version || "unknown", contentHash: body?.contentHash || "unknown", status: 422, durationMs: Date.now() - startedAt, reason: safeResourceReason(e) }); sendJSON(res, 422, { error: "resource_cache_unavailable" }); }
    return;
  }
  if (p === "/api/resources/cache" && req.method === "DELETE") {
    clearResourceCache();
    sendJSON(res, 200, { ok: true });
    return;
  }
  if (p === "/api/resources/cache-preview" && req.method === "GET") {
    if (!resourceCacheSession(req)) { sendJSON(res, 401, { error: "authentication_required" }); return; }
    const cache = readVerifiedResourceCache(url.searchParams.get("key"));
    if (!cache) { sendJSON(res, 404, { error: "resource_cache_not_found" }); return; }
    serveFile(req, res, path.join(cache.directory, "preview.mp4"));
    return;
  }
  if (p === "/api/resources/cache-entry" && req.method === "GET") {
    if (!resourceCacheSession(req)) { sendJSON(res, 401, { error: "authentication_required" }); return; }
    const cache = readVerifiedResourceCache(url.searchParams.get("key"));
    const entry = url.searchParams.get("path") || "";
    if (!cache || !safeRelativePath(entry)) { sendJSON(res, 404, { error: "resource_cache_entry_not_found" }); return; }
    try {
      const packageManifest = JSON.parse(fs.readFileSync(path.join(cache.directory, "manifest.json"), "utf8"));
      const allowed = verifyRuntimeManifest(cache.runtime, packageManifest);
      if (!allowed.has(entry)) { sendJSON(res, 403, { error: "resource_cache_entry_forbidden" }); return; }
      const file = path.join(cache.directory, "runtime", entry);
      if (!file.startsWith(path.join(cache.directory, "runtime") + path.sep)) { sendJSON(res, 403, { error: "resource_cache_entry_forbidden" }); return; }
      if (entry === "edward-runtime.json") sendJSON(res, 200, { ...cache.runtime, componentId: cache.identity.componentId, version: cache.identity.version, contentHash: cache.identity.contentHash });
      else serveFile(req, res, file);
    } catch { sendJSON(res, 404, { error: "resource_cache_entry_not_found" }); }
    return;
  }
  if (p === "/api/resources/favorite" && req.method === "POST") {
    const requestId = resourceRequestId();
    const startedAt = Date.now();
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/resource-favorite", req, body);
      resourceCacheLog("resource_favorite", { requestId, resourceId: body?.resourceId || "unknown", favorite: body?.favorite === true ? "true" : body?.favorite === false ? "false" : "invalid", status: result.status, durationMs: Date.now() - startedAt });
      sendJSON(res, result.status, result.value);
    } catch (e) { resourceCacheLog("resource_favorite_error", { requestId, status: 503, durationMs: Date.now() - startedAt, reason: safeResourceReason(e) }); sendJSON(res, 503, { error: "resource_favorite_unavailable" }); }
    return;
  }
  if (p === "/api/payment/native" && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/huifu-native-create", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/payment/alipay" && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/alipay-create-order", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/payment/alipay/status" && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/alipay-query-order", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  if (p === "/api/payment/alipay/state" && req.method === "POST") {
    try {
      const body = JSON.parse((await readBody(req)).toString("utf8"));
      const result = await supabaseProxy("/functions/v1/alipay-order-state", req, body);
      sendJSON(res, result.status, result.value);
    } catch (e) { sendJSON(res, 503, { error: String(e.message || e) }); }
    return;
  }
  /* API: media library listing */
  if (p === "/api/media" && req.method === "GET") {
    try {
      const files = fs.readdirSync(MEDIA_DIR)
        .filter(isVisibleResourceName)
        .filter((f) => fs.statSync(path.join(MEDIA_DIR, f)).isFile())
        .map((f) => ({ name: f, src: "/media/" + encodeURIComponent(f), size: fs.statSync(path.join(MEDIA_DIR, f)).size }));
      sendJSON(res, 200, files);
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }

  /* API: default-asset library listing (./library/{sfx,elements,svg,fonts}) */
  if (p === "/api/library" && req.method === "GET") {
    const dir = url.searchParams.get("dir");
    if (!LIBRARY_SUBDIRS.includes(dir)) { sendJSON(res, 400, { error: "dir must be one of " + LIBRARY_SUBDIRS.join("|") }); return; }
    try {
      const base = path.join(LIBRARY_DIR, dir);
      const out = [];
      const walk = (d, rel) => {
        for (const f of fs.readdirSync(d)) {
          if (!isVisibleResourceName(f)) continue;
          const full = path.join(d, f), r = rel ? rel + "/" + f : f;
          const st = fs.statSync(full);
          if (st.isDirectory()) walk(full, r);
          else out.push({
            name: f, rel: r, size: st.size,
            src: "/library/" + dir + "/" + r.split("/").map(encodeURIComponent).join("/"),
          });
        }
      };
      walk(base, "");
      sendJSON(res, 200, out);
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }

  /* Direct user components: manifest-driven, local-only browser modules. */
  if (p === "/api/components" && req.method === "GET") {
    try {
      const components = [];
      for (const name of fs.readdirSync(COMPONENTS_DIR)) {
        if (!isVisibleResourceName(name)) continue;
        const dir = path.join(COMPONENTS_DIR, name);
        if (!fs.statSync(dir).isDirectory()) continue;
        const manifest = readComponentManifest(name);
        if (manifest) components.push(manifest);
      }
      sendJSON(res, 200, components);
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }
  if (p.startsWith("/api/components/") && req.method === "GET") {
    const id = path.basename(p);
    const manifest = readComponentManifest(id);
    if (!manifest) {
      sendJSON(res, 404, { error: "component not found" }); return;
    }
    sendJSON(res, 200, manifest);
    return;
  }

  /* API: upload → saved into ./media */
  if (p === "/api/upload" && req.method === "POST") {
    try {
      let name = safeName(url.searchParams.get("name") || "upload.bin");
      let target = path.join(MEDIA_DIR, name);
      let i = 1;
      const ext = path.extname(name), base = path.basename(name, ext);
      while (fs.existsSync(target)) target = path.join(MEDIA_DIR, `${base}_${i++}${ext}`);
      const body = await readBody(req);
      fs.writeFileSync(target, body);
      await faststart(target);
      sendJSON(res, 200, { ok: true, src: "/media/" + encodeURIComponent(path.basename(target)) });
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }

  /* API: download an HTTPS URL into ./media (same-origin src after import).
     Body {url}. Rejects http/file/local/private targets. Does not register
     project media — the client / MCP tool does that, matching /api/upload. */
  if (p === "/api/import-url" && req.method === "POST") {
    try {
      const opts = JSON.parse((await readBody(req)).toString("utf8") || "{}");
      const ac = new AbortController();
      // IncomingMessage "close" also fires when the request body finishes, which
      // would abort a successful download. ServerResponse "close" with
      // writableEnded still false means the client dropped the connection.
      res.on("close", () => { if (!res.writableEnded) ac.abort(); });
      const { target, name } = await downloadImportUrl(opts.url, MEDIA_DIR, {
        signal: ac.signal,
        // test/rest-api.test.js: HTTP to 127.0.0.1 only (loopback fixture).
        // Does not disable SSRF for LAN / metadata / other private ranges.
        allowLoopback: process.env.FABLECUT_TEST_IMPORT_ALLOW_PRIVATE === "1",
      });
      await faststart(target);
      sendJSON(res, 200, { ok: true, src: "/media/" + encodeURIComponent(name), name });
    } catch (e) {
      if (e && e.code === "ABORT_ERR") { try { res.end(); } catch {} return; }
      const msg = e && e.message ? e.message : String(e);
      const code = /must be https|invalid URL|blocked:|credentials|unsupported|too large|did not return/i.test(msg) ? 400 : 502;
      sendJSON(res, code, { error: msg });
    }
    return;
  }

  /* API: fast export (browser-rendered frames → ffmpeg encode) */
  if (p === "/api/export/ffmpeg" && req.method === "GET") {
    sendJSON(res, 200, { available: HAS_FFMPEG });
    return;
  }
  if (p === "/api/export/profiles" && req.method === "GET") {
    try {
      const detail = url.searchParams.get("detail") === "1";
      sendJSON(res, 200, listProfilesPublic(detail));
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }
  if (p === "/api/export/check-name" && req.method === "GET") {
    const ext = String(url.searchParams.get("ext") || ".mp4").match(/^\.(mp4|mov|m4v|mkv|webm)$/i)?.[0] || ".mp4";
    sendJSON(res, 200, previewExportName(url.searchParams.get("name") || "export", ext));
    return;
  }
  if (p === "/api/export/begin" && req.method === "POST") {
    try {
      const opts = JSON.parse((await readBody(req)).toString("utf8") || "{}");
      const mode = opts.mode === "annexb" ? "annexb" : "jpeg";
      if (mode !== "annexb" && opts.profile) resolveProfile(opts.profile); // 400, not 500, on a bad id — even without ffmpeg
      if (!HAS_FFMPEG) { sendJSON(res, 400, { error: "ffmpeg not found on PATH" }); return; }
      sendJSON(res, 200, await beginExport(opts.fps, opts.name, opts.profile, opts.hasAudio !== false, mode));
    } catch (e) {
      // an unusable profile is the caller's problem, not a server fault
      const bad = /^Unknown encoding profile|was rejected by ffmpeg|export fps required/.test(e.message || "");
      sendJSON(res, bad ? 400 : 500, { error: String(e.message || e) });
    }
    return;
  }
  if (p === "/api/export/frame" && req.method === "POST") {
    const id = url.searchParams.get("id");
    const sess = exportSessions.get(id);
    if (!sess) { sendJSON(res, 404, { error: "no such export session" }); return; }
    try {
      const body = await readBody(req);
      if (!body || !body.length) throw new Error("empty frame");
      if (sess.hasAudio && !sess.wav) {
        sendJSON(res, 409, { error: "audio mix has not been uploaded yet" });
        return;
      }
      // queue behind any in-flight write so concurrent POSTs cannot interleave stdin
      const run = async () => {
        if (!sess.proc) sess.mode === "annexb" ? startAnnexbEncoder(sess) : startEncoder(sess);
        await writeExportFrame(sess, body);
      };
      const writeJob = sess.writeLock.then(run, run);
      sess.writeLock = writeJob.catch(() => {}); // keep the chain alive after a failed write
      await writeJob;
      touchExport(sess);
      sendJSON(res, 200, { ok: true });
    } catch (e) {
      cleanupExport(id);
      sendJSON(res, 500, { error: String(e.message || e) });
    }
    return;
  }
  if (p === "/api/export/audio" && req.method === "POST") {
    const sess = exportSessions.get(url.searchParams.get("id"));
    if (!sess) { sendJSON(res, 404, { error: "no such export session" }); return; }
    try {
      const wavPath = path.join(sess.dir, "audio.wav");
      fs.writeFileSync(wavPath, await readBody(req));
      sess.wav = wavPath;
      touchExport(sess);
      sendJSON(res, 200, { ok: true });
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }
  if (p === "/api/export/end" && req.method === "POST") {
    const id = url.searchParams.get("id");
    const sess = exportSessions.get(id);
    if (!sess) { sendJSON(res, 404, { error: "no such export session" }); return; }
    try {
      if (url.searchParams.get("discard")) { cleanupExport(id); sendJSON(res, 200, { ok: true }); return; }
      touchExport(sess); // keep alive through final mux
      if (!sess.proc) throw new Error("no frames were uploaded");
      sess.proc.stdin.end();
      const code = await sess.done;
      if (code !== 0) throw new Error("ffmpeg encode failed: " + sess.err());
      const out = finalizeExportPath(sess);
      sess.partPath = null; // renamed — cleanup must not delete the finished file
      cleanupExport(id);
      sendJSON(res, 200, { ok: true, src: "/exports/" + encodeURIComponent(path.basename(out)) });
    } catch (e) { cleanupExport(id); sendJSON(res, 500, { error: String(e) }); }
    return;
  }

  /* API: reference analysis → edit blueprint (shots, beats, BPM, energy, music).
     POST body {src:"/media/ref.mp4", threshold?, music?} runs the analysis
     (seconds to ~a minute — decode-bound); GET ?src= returns the cached result. */
  if (p === "/api/analyze" && req.method === "GET") {
    const src = decodeURIComponent(url.searchParams.get("src") || "");
    const f = path.join(ANALYSIS_DIR, path.basename(src, path.extname(src)) + ".json");
    if (!src || !fs.existsSync(f)) { sendJSON(res, 404, { error: "no cached analysis for that src — POST /api/analyze first" }); return; }
    try { sendJSON(res, 200, JSON.parse(fs.readFileSync(f, "utf8"))); }
    catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }
  if (p === "/api/analyze" && req.method === "POST") {
    if (!HAS_FFMPEG) { sendJSON(res, 400, { error: "ffmpeg not found on PATH" }); return; }
    try {
      const opts = JSON.parse((await readBody(req)).toString("utf8") || "{}");
      const name = path.basename(decodeURIComponent(opts.src || ""));
      const file = path.join(MEDIA_DIR, name);
      if (!name || !fs.existsSync(file)) { sendJSON(res, 404, { error: "src must name an existing file under /media/" }); return; }
      const bp = await analyze(file, {
        threshold: opts.threshold,
        music: opts.music !== false,
        musicDir: MEDIA_DIR,
        srcUrl: "/media/" + encodeURIComponent(name),
      });
      if (bp.music) bp.music.src = "/media/" + encodeURIComponent(bp.music.name);
      fs.writeFileSync(path.join(ANALYSIS_DIR, path.basename(name, path.extname(name)) + ".json"),
        JSON.stringify(bp, null, 2));
      sendJSON(res, 200, bp);
    } catch (e) { sendJSON(res, 500, { error: String(e) }); }
    return;
  }

  /* API: SSE live-reload channel */
  if (p === "/api/events") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream", "Cache-Control": "no-store",
      Connection: "keep-alive",
    });
    res.write("data: hello\n\n");
    sseClients.add(res);
    req.on("close", () => sseClients.delete(res));
    return;
  }

  /* Media files */
  if (p.startsWith("/media/")) {
    const file = path.join(MEDIA_DIR, path.basename(p));
    serveFile(req, res, file);
    return;
  }

  /* Finished exports */
  if (p.startsWith("/exports/")) {
    serveFile(req, res, path.join(EXPORTS_DIR, path.basename(p)));
    return;
  }

  /* Library assets (supports subfolders) */
  if (p.startsWith("/library/")) {
    const file = path.normalize(path.join(LIBRARY_DIR, p.slice("/library/".length)));
    if (!file.startsWith(LIBRARY_DIR + path.sep)) { res.writeHead(403); res.end(); return; }
    serveFile(req, res, file);
    return;
  }

  if (p.startsWith("/components/")) {
    const rel = p.slice("/components/".length);
    const file = path.normalize(path.join(COMPONENTS_DIR, rel));
    if (!file.startsWith(COMPONENTS_DIR + path.sep)) { res.writeHead(403); res.end(); return; }
    serveFile(req, res, file);
    return;
  }

  /* Static app files */
  let file = p === "/" ? "/index.html" : p;
  file = path.normalize(path.join(ROOT, file));
  if (!file.startsWith(ROOT + path.sep)) { res.writeHead(403); res.end(); return; }
  serveFile(req, res, file);
});

server.listen(PORT, HOST, () => {
  console.log(`\n  FableCut running →  http://localhost:${PORT}\n`);
  if (!["127.0.0.1", "localhost", "::1"].includes(HOST))
    console.log(`  ⚠ WARNING: HOST=${HOST} exposes the editor (and its file APIs) to the network.\n`);
  console.log(`  project file : ${PROJECT_FILE}`);
  console.log(`  media folder : ${MEDIA_DIR}`);
  console.log(`  library      : ${LIBRARY_DIR} (${LIBRARY_SUBDIRS.join(", ")})`);
  if (DATA_DIR !== APP_DIR) console.log(`  app files    : ${APP_DIR}`);
  console.log(`  ffmpeg       : ${HAS_FFMPEG ? "found (fast export + faststart remux on)" : "not found (real-time export only)"}`);
  const enc = loadEncodeProfiles(true);
  console.log(`  encode prof. : ${PROFILES_FILE} (${Object.keys(enc.profiles).join(", ")} · default ${enc.default})`);
  for (const issue of enc.issues || []) console.log(`     ⚠ ${issue}`);
  console.log("");
  startExportSweep();
});

let shuttingDown = false;
function shutdown() {
  if (shuttingDown) return;
  shuttingDown = true;
  stopExportSweep();
  for (const id of [...exportSessions.keys()]) cleanupExport(id);
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 2000).unref?.();
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
