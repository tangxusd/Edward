/* Direct browser component runtime. User modules render real DOM/SVG; no IR. */
const overlay = document.getElementById("userComponentOverlay");
const mounted = new Map();
const mounting = new Map();
let desiredComponentIds = new Set();
let syncGeneration = 0;
const lastPluginUpdateLog = new Map();
const ALLOWED_RUNTIMES = new Set(["react", "gsap", "html-css", "svg"]);
function recordPluginDiagnostic(kind, detail = "") {
  const text = String(detail).replace(/[\r\n]/g, " ").slice(0, 300);
  window.edwardSettings?.recordDiagnostic?.(`plugin_${kind}${text ? ` detail=${text}` : ""}`);
}

function runtimeEntry(manifest, mode) {
  const entry = manifest?.protocol === "edward.web-runtime.v1"
    ? (mode === "export" ? manifest.renderEntry : manifest.previewEntry)
    : manifest?.entry;
  const path = String(entry || "").replace(/^\.\//, "");
  if (!path || path.includes("..") || path.startsWith("/") || path.startsWith("\\") || path.includes(":")) {
    throw new Error("invalid component entry");
  }
  return path;
}

function inlineStyles(source, target) {
  if (!source || !target || !target.style) return;
  const computed = getComputedStyle(source);
  for (const name of computed) target.style.setProperty(name, computed.getPropertyValue(name));
  for (let i = 0; i < source.children.length; i++) inlineStyles(source.children[i], target.children[i]);
}

async function prepareFrame(clips, time, viewport) {
  await syncDirectComponents(clips, time, viewport, "export");
  // React 18 commits root.render asynchronously. Wait two compositor ticks so
  // the SVG is present before the export canvas is captured.
  await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
}

async function captureCompositeFrame(outputSpec) {
  const source = document.getElementById("userComponentOverlay");
  const preview = document.getElementById("preview");
  if (!source || !preview) throw new Error("compositor surface is unavailable");
  const clone = source.cloneNode(true);
  inlineStyles(source, clone);
  clone.querySelectorAll("#safeOverlay,#exportFrameOverlay").forEach((node) => node.remove());
  const visibleHosts = [...source.children].filter((host) => getComputedStyle(host).display !== "none");
  const nativeSvgNodes = visibleHosts.map((host) => host.querySelector("svg")).filter(Boolean);
  const canvas = document.createElement("canvas");
  canvas.width = outputSpec.width; canvas.height = outputSpec.height;
  const context = canvas.getContext("2d");
  if (!context) throw new Error("composite capture canvas unavailable");
  if (!outputSpec.transparent) context.drawImage(preview, 0, 0, canvas.width, canvas.height);
  // Native annotation components are SVG documents. Rasterize them directly
  // instead of putting them inside foreignObject; WebEngine/ffmpeg paths may
  // otherwise drop the component layer while Card6's HTML happens to survive.
  if (nativeSvgNodes.length > 0) {
    const overlayRect = source.getBoundingClientRect();
    const parts = nativeSvgNodes.map((svgNode) => {
      const rect = svgNode.getBoundingClientRect();
      const worldX = Number(svgNode.dataset.edwardX);
      const worldY = Number(svgNode.dataset.edwardY);
      const worldW = Number(svgNode.dataset.edwardWidth);
      const worldH = Number(svgNode.dataset.edwardHeight);
      const worldViewportW = Number(svgNode.dataset.edwardViewportWidth);
      const worldViewportH = Number(svgNode.dataset.edwardViewportHeight);
      const hasWorldGeometry = [worldX, worldY, worldW, worldH, worldViewportW, worldViewportH].every(Number.isFinite) && worldViewportW > 0 && worldViewportH > 0;
      const w = hasWorldGeometry ? worldW * outputSpec.width / worldViewportW : (rect.width / overlayRect.width) * outputSpec.width;
      const h = hasWorldGeometry ? worldH * outputSpec.height / worldViewportH : (rect.height / overlayRect.height) * outputSpec.height;
      const x = hasWorldGeometry ? outputSpec.width / 2 + worldX * outputSpec.width / worldViewportW - w / 2 : ((rect.left - overlayRect.left) / overlayRect.width) * outputSpec.width;
      const y = hasWorldGeometry ? outputSpec.height / 2 - worldY * outputSpec.height / worldViewportH - h / 2 : ((rect.top - overlayRect.top) / overlayRect.height) * outputSpec.height;
      const rotation = hasWorldGeometry ? Number(svgNode.dataset.edwardRotation || 0) : 0;
      const viewBox = svgNode.getAttribute("viewBox") || `0 0 ${rect.width} ${rect.height}`;
      const cx = x + w / 2, cy = y + h / 2;
      return `<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${viewBox}" preserveAspectRatio="none" overflow="visible" transform="rotate(${rotation} ${cx} ${cy})">${svgNode.innerHTML}</svg>`;
    }).join("");
    const directSvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${outputSpec.width}" height="${outputSpec.height}" overflow="visible"><rect width="100%" height="100%" fill="none"/>${parts}</svg>`;
    const directImage = new Image();
    await new Promise((resolve, reject) => { directImage.onload = resolve; directImage.onerror = reject; directImage.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(directSvg); });
    context.drawImage(directImage, 0, 0, outputSpec.width, outputSpec.height);
  }
  const legacyHosts = visibleHosts.filter((host) => !host.querySelector(":scope > svg") && !host.querySelector("svg"));
  if (!legacyHosts.length) return canvas;
  const legacyIds = new Set(legacyHosts.map((host) => host.dataset.clipId));
  [...clone.children].forEach((host) => { if (!legacyIds.has(host.dataset.clipId)) host.remove(); });
  if (!clone.children.length) return canvas;
  clone.style.position = "relative";
  clone.style.inset = "auto";
  clone.style.left = "0px";
  clone.style.top = "0px";
  clone.style.right = "auto";
  clone.style.bottom = "auto";
  clone.style.width = `${preview.width}px`;
  clone.style.height = `${preview.height}px`;
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${outputSpec.width}" height="${outputSpec.height}" viewBox="0 0 ${preview.width} ${preview.height}"><foreignObject width="100%" height="100%" x="0" y="0"><div xmlns="http://www.w3.org/1999/xhtml" style="width:${preview.width}px;height:${preview.height}px">${clone.outerHTML}</div></foreignObject></svg>`;
  const encoded = new TextEncoder().encode(svg);
  let binary = "";
  for (const byte of encoded) binary += String.fromCharCode(byte);
  const url = "data:image/svg+xml;base64," + btoa(binary);
  try {
    if (typeof createImageBitmap === "function") {
      try {
        const bitmap = await createImageBitmap(new Blob([svg], {type: "image/svg+xml"}));
        context.drawImage(bitmap, 0, 0, canvas.width, canvas.height);
        bitmap.close();
        return canvas;
      } catch (_) {
        // Fall through to the Image decoder for WebEngine versions that do not
        // support SVG Blob decoding through createImageBitmap.
      }
    }
    const image = new Image();
    image.decoding = "sync";
    await new Promise((resolve, reject) => {
      image.onload = resolve;
      image.onerror = () => reject(new Error("composite capture image load failed"));
      image.src = url;
    });
    if (!(image.naturalWidth > 0 && image.naturalHeight > 0))
      throw new Error("composite capture image has zero dimensions");
    context.drawImage(image, 0, 0, canvas.width, canvas.height);
    return canvas;
  } finally { /* data URL has no object URL to revoke */ }
}

function cacheEntryUrl(cacheKey, entry) {
  return `/api/resources/cache-entry?key=${encodeURIComponent(cacheKey)}&path=${encodeURIComponent(entry)}`;
}

async function loadManifest(id = "demo", resourceRef = null) {
  const url = resourceRef?.source === "cached" && resourceRef.cacheKey
    ? cacheEntryUrl(resourceRef.cacheKey, "edward-runtime.json")
    : `/api/components/${encodeURIComponent(id)}`;
  const r = await fetch(url);
  if (!r.ok) throw new Error(`component manifest unavailable: ${r.status}`);
  return r.json();
}

async function mountDirectComponent(id, props = {}, time = 0, mode = "preview", viewport = null, resourceRef = null) {
  if (!overlay) return null;
  recordPluginDiagnostic("mount_start", `component=${id} mode=${mode}`);
  const manifest = await loadManifest(id, resourceRef);
  if (!ALLOWED_RUNTIMES.has(manifest.runtime || "html-css")) throw new Error("unsupported native component runtime");
  const validation = resourceRef?.source === "cached"
    ? window.fablecutComponentVersioning?.validateComponentInstance(manifest, { props, resourceRef }) : null;
  if (validation && !validation.ok) throw new Error(validation.reason);
  const entryPath = runtimeEntry(manifest, mode);
  if (manifest.style) {
    const link = document.createElement("link");
    link.rel = "stylesheet";
    const stylePath = manifest.style.replace(/^\.\//, "");
    link.href = resourceRef?.source === "cached" && resourceRef.cacheKey
      ? cacheEntryUrl(resourceRef.cacheKey, stylePath)
      : `/components/${encodeURIComponent(id)}/${stylePath}`;
    document.head.appendChild(link);
  }
  const host = document.createElement("div");
  host.dataset.userComponent = id;
  host.style.zIndex = "10";
  overlay.appendChild(host);
  const entryUrl = resourceRef?.source === "cached" && resourceRef.cacheKey
    ? cacheEntryUrl(resourceRef.cacheKey, entryPath)
    : `/components/${encodeURIComponent(id)}/${entryPath}`;
  const mod = await import(entryUrl);
  if (typeof mod.mount !== "function") throw new Error("user component must export mount({host, props, time})");
  const projectFps = Number(viewport?.fps || manifest.fps || 30);
  const frame = window.fablecutComponentVersioning?.componentFrameAt(time * projectFps, { projectFps }, manifest) ?? Math.round(time * manifest.fps);
  const instance = await mod.mount({ host, props, time: frame / manifest.fps, mode, viewport: { ...viewport, frame } });
  recordPluginDiagnostic("mount_complete", `component=${id} mode=${mode}`);
  return { manifest, host, instance, entryPath, mode, resourceCacheKey: resourceRef?.cacheKey || null };
}
function applyTransitionMask(host, props = {}) {
  const amount = Math.max(0, Math.min(1, Number(props._wipe ?? props._iris ?? 0)));
  if (!(amount > 0)) { host.style.clipPath = ""; return; }
  if (props._iris != null) {
    host.style.clipPath = `circle(${Math.max(0.01, (1 - amount) * 71)}% at 50% 50%)`;
    return;
  }
  const direction = props._wipeDir || "left";
  if (direction === "right") host.style.clipPath = `inset(0 0 0 ${amount * 100}%)`;
  else if (direction === "up") host.style.clipPath = `inset(0 0 ${amount * 100}% 0)`;
  else if (direction === "down") host.style.clipPath = `inset(${amount * 100}% 0 0 0)`;
  else host.style.clipPath = `inset(0 ${amount * 100}% 0 0)`;
}
async function syncDirectComponents(clips, time, viewport, mode = "preview") {
  if (!overlay) return;
  const cssRect = overlay.getBoundingClientRect();
  const renderViewport = { ...(viewport || {}), cssWidth: cssRect.width || viewport?.width, cssHeight: cssRect.height || viewport?.height };
  const generation = ++syncGeneration;
  const activeClips = (clips || []).filter((clip) => clip.kind === "component");
  const activeIds = new Set(activeClips.map((clip) => clip.id));
  desiredComponentIds = activeIds;
  // Remove stale mounted hosts before awaiting any new module. This makes a
  // playhead change atomic from the user's point of view and prevents a prior
  // component from being shown under a newly active clip.
  for (const [id, entry] of mounted) {
    if (!activeIds.has(id)) {
      entry.instance.destroy?.();
      entry.host.remove();
      mounted.delete(id);
    }
  }
  // Hide stale layers synchronously. The async mount/update path must never
  // leave a component from the previous playhead visible in an empty interval.
  for (const [id, entry] of mounted) entry.host.style.display = activeIds.has(id) ? "flex" : "none";
  const active = new Set();
  for (const clip of activeClips) {
    if (generation !== syncGeneration) return;
    active.add(clip.id);
    let entry = mounted.get(clip.id);
    if (entry && (entry.componentId !== (clip.componentId || "demo") || entry.mode !== mode || entry.resourceCacheKey !== (clip.resourceRef?.cacheKey || null))) {
      entry.instance.destroy?.(); entry.host.remove(); mounted.delete(clip.id); entry = null;
    }
    if (!entry) {
      let pending = mounting.get(clip.id);
      if (!pending) {
        pending = mountDirectComponent(clip.componentId || "demo", clip.props || {}, time - clip.start, mode, renderViewport, clip.resourceRef || null);
        mounting.set(clip.id, pending);
      }
      try { entry = await pending; } catch (error) {
        recordPluginDiagnostic("mount_error", `component=${clip.componentId || "demo"} message=${error?.message || error}`);
        throw error;
      } finally {
        if (mounting.get(clip.id) === pending) mounting.delete(clip.id);
      }
      if (entry && desiredComponentIds.has(clip.id)) mounted.set(clip.id, entry);
      else if (entry) {
        entry.instance.destroy?.();
        entry.host.remove();
      }
    }
    if (generation !== syncGeneration && !desiredComponentIds.has(clip.id)) {
      if (entry && !mounted.has(clip.id)) {
        entry.instance.destroy?.();
        entry.host.remove();
      }
      return;
    }
    if (entry) {
      entry.componentId = clip.componentId || "demo";
      entry.host.dataset.clipId = clip.id;
      entry.host.style.display = "flex";
      applyTransitionMask(entry.host, clip.props);
      const updateLogKey = `${clip.id}:${mode}`;
      const now = performance.now();
      if (mode === "export" || now - (lastPluginUpdateLog.get(updateLogKey) || 0) > 500) {
        lastPluginUpdateLog.set(updateLogKey, now);
        recordPluginDiagnostic("update", `component=${entry.componentId} clip=${clip.id} time=${Number(time - clip.start).toFixed(3)}`);
      }
      const localTime = time - clip.start;
      const projectFps = Number(renderViewport?.fps || entry.manifest?.fps || 30);
      const frame = window.fablecutComponentVersioning?.componentFrameAt(localTime * projectFps, { projectFps }, entry.manifest) ?? Math.round(localTime * entry.manifest.fps);
      await entry.instance.update?.(clip.props || {}, frame / entry.manifest.fps, { ...renderViewport, frame }, mode);
    }
  }
  if (generation !== syncGeneration) return;
  for (const [id, entry] of mounted) {
    if (!active.has(id)) { entry.instance.destroy?.(); entry.host.remove(); mounted.delete(id); }
  }
}
function updateDirectComponent(clip, time = 0, mode = "preview", viewport = null) {
  const entry = mounted.get(clip?.id);
  if (entry) {
    const canvas = document.getElementById("preview");
    const base = viewport || { width: canvas?.width || 1280, height: canvas?.height || 720 };
    const rect = overlay?.getBoundingClientRect();
    const renderViewport = { ...base, cssWidth: base.cssWidth || rect?.width || base.width, cssHeight: base.cssHeight || rect?.height || base.height };
    applyTransitionMask(entry.host, clip.props);
    const localTime = time - (clip.start || 0);
    const projectFps = Number(renderViewport?.fps || entry.manifest?.fps || 30);
    const frame = window.fablecutComponentVersioning?.componentFrameAt(localTime * projectFps, { projectFps }, entry.manifest) ?? Math.round(localTime * entry.manifest.fps);
    entry.instance.update?.(clip.props || {}, frame / entry.manifest.fps, { ...renderViewport, frame }, mode);
  }
}

window.fablecutDirectComponents = { mountDirectComponent, syncDirectComponents, updateDirectComponent, prepareFrame, captureCompositeFrame };
