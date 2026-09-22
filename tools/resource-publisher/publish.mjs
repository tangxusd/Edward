import fs from "node:fs/promises";
import crypto from "node:crypto";
import path from "node:path";
import { createRequire } from "node:module";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const require = createRequire(import.meta.url);
const { readZipBuffer } = require("../../third_party/FableCut/zip-extract.js");
const execFileAsync = promisify(execFile);
const MAX_PREVIEW_BYTES = 20 * 1024 * 1024;

const TABS = new Set(["media", "text", "audio", "cards", "chart", "background", "annotation", "number"]);
const RUNTIMES = new Set(["react", "html-css", "gsap", "svg"]);
const ID_RE = /^[a-z0-9][a-z0-9._-]{2,127}$/;
const VERSION_RE = /^\d+\.\d+\.\d+$/;
const HASH_RE = /^[a-f0-9]{64}$/;
const TRACK_RE = /^[A-Za-z][A-Za-z0-9_.-]{0,127}$/;

function validCapabilities(value) {
  return !!value && typeof value === "object" && ["preview", "export", "editable", "audio", "transparent"].every((key) => typeof value[key] === "boolean") && value.preview && value.export;
}

function validTimeline(value) {
  return !!value && typeof value === "object" && Number.isInteger(value.authoringFps) && value.authoringFps > 0 && Number.isInteger(value.durationFrames) && value.durationFrames > 0 && value.frameRounding === "nearest";
}

function validStringList(value) {
  return Array.isArray(value) && value.length > 0 && value.every((item) => typeof item === "string" && TRACK_RE.test(item)) && new Set(value).size === value.length;
}

function validEditableTracks(value, properties) {
  const instanceTracks = new Set(["x", "y", "scale", "rotation", "opacity"]);
  return validStringList(value) && value.every((track) => instanceTracks.has(track) || properties.includes(track.split(".")[0]));
}

function validAssets(value) {
  return Array.isArray(value) && value.length > 0 && value.every((asset) => asset && typeof asset === "object" && typeof asset.path === "string" && asset.path.length > 0 && !asset.path.startsWith("/") && !asset.path.split(/[\\/]/).includes("..") && typeof asset.mimeType === "string" && Number.isInteger(asset.bytes) && asset.bytes >= 0 && HASH_RE.test(String(asset.sha256 || "")));
}

export function validateManifest(manifest) {
  const errors = [];
  if (!manifest || typeof manifest !== "object") return ["manifest must be an object"];
  if (!ID_RE.test(String(manifest.component_id || ""))) errors.push("component_id is invalid");
  if (!TABS.has(manifest.tab_key)) errors.push("tab_key is invalid");
  if (!String(manifest.category_id || "").match(/^[0-9a-f-]{36}$/i)) errors.push("category_id must be a UUID");
  if (!String(manifest.name || "").trim()) errors.push("name is required");
  if (!VERSION_RE.test(String(manifest.version || ""))) errors.push("version is invalid");
  if (!RUNTIMES.has(manifest.runtime)) errors.push("runtime is invalid");
  if (manifest.target !== "web.runtime") errors.push("target is invalid");
  if (!validCapabilities(manifest.capabilities)) errors.push("capabilities is invalid");
  if (!validTimeline(manifest.timeline)) errors.push("timeline is invalid");
  if (!validStringList(manifest.editableProperties)) errors.push("editableProperties is invalid");
  if (!validEditableTracks(manifest.editableTracks, Array.isArray(manifest.editableProperties) ? manifest.editableProperties : [])) errors.push("editableTracks is invalid");
  if (!validAssets(manifest.assets)) errors.push("assets is invalid");
  return errors;
}

export async function sha256File(filePath) {
  const hash = crypto.createHash("sha256");
  hash.update(await fs.readFile(filePath));
  return hash.digest("hex");
}

function sha256(bytes) {
  return crypto.createHash("sha256").update(bytes).digest("hex");
}

export function validatePackage(manifest, files, previewBytes) {
  const errors = [];
  const runtimeBytes = files.get("edward-runtime.json");
  if (!runtimeBytes) return ["edward-runtime.json is required"];
  let runtime;
  try { runtime = JSON.parse(runtimeBytes.toString("utf8")); } catch { return ["edward-runtime.json is invalid"]; }
  if (runtime.protocol !== "edward.web-runtime.v1" || runtime.runtime !== manifest.runtime) errors.push("runtime contract is invalid");
  const runtimeProperties = Array.isArray(runtime.editableProperties) ? runtime.editableProperties : [];
  const runtimeTracks = Array.isArray(runtime.editableTracks) ? runtime.editableTracks : [];
  const sameList = (a, b) => Array.isArray(a) && Array.isArray(b) && a.length === b.length && a.every((value, index) => value === b[index]);
  const schemaValid = runtime.propsSchema && runtime.propsSchema.type === "object" && runtime.propsSchema.additionalProperties === false && runtime.propsSchema.properties && typeof runtime.propsSchema.properties === "object";
  if (runtime.fps !== manifest.timeline?.authoringFps || runtime.durationInFrames !== manifest.timeline?.durationFrames || !sameList(runtimeProperties, manifest.editableProperties) || !sameList(runtimeTracks, manifest.editableTracks) || !schemaValid) errors.push("runtime contract does not match manifest");
  if (schemaValid && [...runtimeProperties, ...runtimeTracks.map((track) => track.split(".")[0])].some((property) => !Object.hasOwn(runtime.propsSchema.properties, property))) errors.push("runtime properties schema is incomplete");
  if (!runtime.previewEntry || !runtime.renderEntry || !Array.isArray(runtime.editableProperties) || !Array.isArray(runtime.editableTracks) || !runtime.capabilities?.preview || !runtime.capabilities?.export) errors.push("runtime entries are invalid");
  for (const entry of [runtime.previewEntry, runtime.renderEntry]) if (typeof entry !== "string" || !files.has(entry)) errors.push("runtime entry is missing");
  for (const asset of manifest.assets || []) {
    const bytes = files.get(asset.path);
    if (!bytes) errors.push(`asset is missing: ${asset.path}`);
    else if (bytes.byteLength !== asset.bytes || sha256(bytes) !== asset.sha256) errors.push(`asset hash is invalid: ${asset.path}`);
  }
  if (!Buffer.isBuffer(previewBytes) || previewBytes.length < 12 || previewBytes.subarray(4, 8).toString("ascii") !== "ftyp") errors.push("preview must be a valid MP4");
  if (Buffer.isBuffer(previewBytes) && previewBytes.length > MAX_PREVIEW_BYTES) errors.push("preview is too large");
  return [...new Set(errors)];
}

export async function validatePreviewFile(previewPath) {
  const { stdout } = await execFileAsync("ffprobe", ["-v", "error", "-select_streams", "v:0", "-show_entries", "stream=codec_name,width,height,duration", "-of", "json", previewPath]);
  const stream = JSON.parse(stdout).streams?.[0];
  const width = Number(stream?.width), height = Number(stream?.height), duration = Number(stream?.duration);
  if (stream?.codec_name !== "h264" || !Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0 || Math.max(width, height) > 854 || width % 2 !== 0 || height % 2 !== 0 || !Number.isFinite(duration) || duration <= 0 || duration > 15) throw new Error("preview_contract_invalid");
  return { codec: stream.codec_name, width, height, duration };
}

async function deleteObject(url, key, objectPath) {
  await request(`${url}/storage/v1/object/resource-packages/${objectPath}`, key, { method: "DELETE" });
}

async function request(url, key, options = {}) {
  const response = await fetch(url, { ...options, headers: { apikey: key, Authorization: `Bearer ${key}`, ...(options.headers || {}) } });
  const text = await response.text();
  let value; try { value = JSON.parse(text); } catch { value = text; }
  if (!response.ok) throw new Error(`Supabase request failed (${response.status}): ${typeof value === "string" ? value.slice(0, 200) : JSON.stringify(value)}`);
  return value;
}

export async function publish({ manifestPath, packagePath, previewPath }) {
  const url = String(process.env.SUPABASE_URL || "").replace(/\/$/, "");
  const key = process.env.SUPABASE_SERVICE_ROLE_KEY || "";
  if (!url || !key) throw new Error("SUPABASE_URL and SUPABASE_SERVICE_ROLE_KEY are required");
  const manifest = JSON.parse(await fs.readFile(manifestPath, "utf8"));
  const errors = validateManifest(manifest);
  if (errors.length) throw new Error(`invalid manifest: ${errors.join(", ")}`);
  if (!previewPath || path.extname(previewPath).toLowerCase() !== ".mp4") throw new Error("previewPath must be an MP4 file");
  const contentHash = await sha256File(packagePath);
  const prefix = `${manifest.component_id}/${manifest.version}`;
  const packageBytes = await fs.readFile(packagePath);
  const manifestBytes = Buffer.from(JSON.stringify(manifest, null, 2));
  const previewBytes = await fs.readFile(previewPath);
  await validatePreviewFile(previewPath);
  const packageErrors = validatePackage(manifest, new Map(readZipBuffer(packageBytes).map(({ name, bytes }) => [name, bytes])), previewBytes);
  if (packageErrors.length) throw new Error(`invalid package: ${packageErrors.join(", ")}`);
  const upload = async (filePath, objectPath, contentType) => request(`${url}/storage/v1/object/resource-packages/${objectPath}`, key, { method: "POST", headers: { "Content-Type": contentType, "x-upsert": "false" }, body: filePath });
  const uploadedPaths = [];
  const previewObject = `${prefix}/preview.mp4`;
  try {
    for (const [bytes, objectPath, contentType] of [[manifestBytes, `${prefix}/manifest.json`, "application/json"], [packageBytes, `${prefix}/package.zip`, "application/zip"], [previewBytes, previewObject, "video/mp4"]]) {
      await upload(bytes, objectPath, contentType);
      uploadedPaths.push(objectPath);
    }
    await request(`${url}/rest/v1/rpc/publish_resource_version`, key, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ p_component_id: manifest.component_id, p_target: manifest.target, p_tab_key: manifest.tab_key, p_category_id: manifest.category_id, p_name: manifest.name, p_summary: manifest.summary || "", p_detail_markdown: manifest.detail_markdown || "", p_version: manifest.version, p_content_hash: contentHash, p_manifest_path: `${prefix}/manifest.json`, p_package_path: `${prefix}/package.zip`, p_preview_video_path: previewObject, p_file_size: packageBytes.byteLength, p_mime_type: "application/zip", p_compatibility: manifest.compatibility || {} }) });
  } catch (error) {
    await Promise.allSettled(uploadedPaths.map((objectPath) => deleteObject(url, key, objectPath)));
    throw error;
  }
  return { component_id: manifest.component_id, version: manifest.version, content_hash: contentHash };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const args = Object.fromEntries(process.argv.slice(2).reduce((all, value, index, values) => value.startsWith("--") ? all.concat([[value.slice(2), values[index + 1]]]) : all, []));
  publish({ manifestPath: args.manifest, packagePath: args.package, previewPath: args.preview }).then((result) => console.log(JSON.stringify(result))).catch((error) => { console.error(error.message); process.exitCode = 1; });
}
