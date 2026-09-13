import fs from "node:fs/promises";
import crypto from "node:crypto";
import path from "node:path";

const TABS = new Set(["media", "text", "audio", "cards", "chart", "background", "annotation", "number"]);
const ID_RE = /^[a-z0-9][a-z0-9._-]{2,127}$/;
const VERSION_RE = /^\d+\.\d+\.\d+$/;
const TARGET_RE = /^[a-z][a-z0-9._:-]{1,127}$/;

export function validateManifest(manifest) {
  const errors = [];
  if (!manifest || typeof manifest !== "object") return ["manifest must be an object"];
  if (!ID_RE.test(String(manifest.component_id || ""))) errors.push("component_id is invalid");
  if (!TABS.has(manifest.tab_key)) errors.push("tab_key is invalid");
  if (!String(manifest.category_id || "").match(/^[0-9a-f-]{36}$/i)) errors.push("category_id must be a UUID");
  if (!String(manifest.name || "").trim()) errors.push("name is required");
  if (!VERSION_RE.test(String(manifest.version || ""))) errors.push("version is invalid");
  if (manifest.target !== undefined && !TARGET_RE.test(String(manifest.target))) errors.push("target is invalid");
  return errors;
}

export async function sha256File(filePath) {
  const hash = crypto.createHash("sha256");
  hash.update(await fs.readFile(filePath));
  return hash.digest("hex");
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
  const contentHash = await sha256File(packagePath);
  const prefix = `${manifest.component_id}/${manifest.version}`;
  const packageBytes = await fs.readFile(packagePath);
  const manifestBytes = Buffer.from(JSON.stringify(manifest, null, 2));
  const upload = async (filePath, objectPath, contentType) => request(`${url}/storage/v1/object/resource-packages/${objectPath}`, key, { method: "POST", headers: { "Content-Type": contentType, "x-upsert": "false" }, body: filePath });
  await upload(manifestBytes, `${prefix}/manifest.json`, "application/json");
  await upload(packageBytes, `${prefix}/package.zip`, "application/zip");
  let previewObject = null;
  if (previewPath) {
    const previewBytes = await fs.readFile(previewPath);
    previewObject = `${prefix}/${path.basename(previewPath)}`;
    await upload(previewBytes, previewObject, "application/octet-stream");
  }
  const [resource] = await request(`${url}/rest/v1/resources?on_conflict=component_id`, key, { method: "POST", headers: { "Content-Type": "application/json", Prefer: "resolution=merge-duplicates,return=representation" }, body: JSON.stringify({ component_id: manifest.component_id, target: manifest.target || "resolve.fusion", tab_key: manifest.tab_key, category_id: manifest.category_id, name: manifest.name, summary: manifest.summary || "", detail_markdown: manifest.detail_markdown || "", status: "published", visibility: "public", published_at: new Date().toISOString() }) });
  await request(`${url}/rest/v1/resource_versions`, key, { method: "POST", headers: { "Content-Type": "application/json", Prefer: "return=minimal" }, body: JSON.stringify({ resource_id: resource.id, version: manifest.version, content_hash: contentHash, manifest_path: `${prefix}/manifest.json`, package_path: `${prefix}/package.zip`, preview_image_path: previewObject, file_size: packageBytes.byteLength, mime_type: "application/zip", published_at: new Date().toISOString(), compatibility: manifest.compatibility || {} }) });
  return { component_id: manifest.component_id, version: manifest.version, content_hash: contentHash };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const args = Object.fromEntries(process.argv.slice(2).reduce((all, value, index, values) => value.startsWith("--") ? all.concat([[value.slice(2), values[index + 1]]]) : all, []));
  publish({ manifestPath: args.manifest, packagePath: args.package, previewPath: args.preview }).then((result) => console.log(JSON.stringify(result))).catch((error) => { console.error(error.message); process.exitCode = 1; });
}
