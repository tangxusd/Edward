"use strict";

(function registerComponentVersioning(root) {
  const HASH_RE = /^[a-f0-9]{64}$/;
  const VERSION_RE = /^\d+\.\d+\.\d+$/;

  function parseCacheKey(value) {
    const match = /^([a-z0-9][a-z0-9._-]{2,127})@(\d+\.\d+\.\d+)#([a-f0-9]{64})$/.exec(String(value || ""));
    return match ? { componentId: match[1], version: match[2], contentHash: match[3] } : null;
  }

  function componentProperties(manifest) {
    return manifest?.propsSchema?.properties && typeof manifest.propsSchema.properties === "object"
      ? manifest.propsSchema.properties : {};
  }

  function validateValue(spec, value) {
    if (value == null) return true;
    if (spec.type === "number" && (!Number.isFinite(value) || (spec.minimum != null && value < spec.minimum) || (spec.maximum != null && value > spec.maximum))) return false;
    if (spec.type === "string" && typeof value !== "string") return false;
    return true;
  }

  function validateComponentInstance(manifest, instance) {
    if (!manifest || manifest.protocol !== "edward.web-runtime.v1") return { ok: false, reason: "运行时清单无效" };
    const properties = componentProperties(manifest);
    const editable = new Set(manifest.editableProperties || []);
    for (const [key, value] of Object.entries(instance?.props || {})) {
      if (!editable.has(key) || !properties[key]) return { ok: false, reason: `未知属性：${key}` };
      if (!validateValue(properties[key], value)) return { ok: false, reason: `属性值无效：${key}` };
    }
    const tracks = new Set(manifest.editableTracks || []);
    for (const key of Object.keys(instance?.keyframes || {})) {
      if (!tracks.has(key)) return { ok: false, reason: `未授权关键帧：${key}` };
    }
    const ref = instance?.resourceRef;
    if (ref && (ref.target !== "web.runtime" || !/^[a-z0-9][a-z0-9._-]{2,127}$/.test(String(ref.componentId || "")) || !VERSION_RE.test(String(ref.version || "")) || !HASH_RE.test(String(ref.contentHash || ""))))
      return { ok: false, reason: "组件版本锁定无效" };
    if (ref?.source === "cached") {
      const cacheIdentity = parseCacheKey(ref.cacheKey);
      if (!cacheIdentity || cacheIdentity.componentId !== ref.componentId || cacheIdentity.version !== ref.version || cacheIdentity.contentHash !== ref.contentHash)
        return { ok: false, reason: "组件缓存键无效" };
      if (manifest.componentId !== ref.componentId || manifest.version !== ref.version || manifest.contentHash !== ref.contentHash)
        return { ok: false, reason: "组件身份不匹配" };
    }
    return { ok: true };
  }

  function componentFrameAt(projectFrame, clip, manifest) {
    const projectFps = Number(clip?.projectFps || manifest?.fps || 30);
    const authoringFps = Number(manifest?.fps || projectFps);
    return Math.round(Number(projectFrame || 0) * authoringFps / projectFps);
  }

  async function upgradeComponentInstance(instance, fromManifest, toManifest, options = {}) {
    const upgrade = toManifest?.upgrade;
    if (instance?.resourceRef?.target !== "web.runtime" || toManifest?.target && toManifest.target !== "web.runtime") throw new Error("组件目标不兼容");
    if (!upgrade?.migrationEntry || !Array.isArray(upgrade.compatibleFrom) || !upgrade.compatibleFrom.includes(instance?.resourceRef?.version)) throw new Error("目标版本未声明可升级路径");
    if (typeof options.loadMigration !== "function" || typeof options.validateFrames !== "function") throw new Error("升级运行时未就绪");
    const snapshot = structuredClone(instance);
    const migration = await options.loadMigration(upgrade.migrationEntry);
    if (typeof migration?.migrate !== "function") throw new Error("升级入口无效");
    const result = await migration.migrate(structuredClone(instance), { fromManifest, toManifest });
    const valid = validateComponentInstance(toManifest, result);
    if (!valid.ok) throw new Error(valid.reason);
    await options.validateFrames(result, toManifest, [0, Math.floor((toManifest.durationInFrames - 1) / 2), toManifest.durationInFrames - 1]);
    return { ...result, undoSnapshot: snapshot };
  }

  const api = { validateComponentInstance, componentFrameAt, upgradeComponentInstance, parseCacheKey };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  root.fablecutComponentVersioning = api;
})(typeof window !== "undefined" ? window : globalThis);
