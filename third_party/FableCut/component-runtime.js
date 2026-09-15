/* Direct browser component runtime. User modules render real DOM/SVG; no IR. */
const overlay = document.getElementById("userComponentOverlay");
const mounted = new Map();
const mounting = new Map();
let desiredComponentIds = new Set();
let syncGeneration = 0;
const ALLOWED_RUNTIMES = new Set(["react", "gsap", "html-css", "svg"]);

function inlineStyles(source, target) {
  if (!source || !target || !target.style) return;
  const computed = getComputedStyle(source);
  for (const name of computed) target.style.setProperty(name, computed.getPropertyValue(name));
  for (let i = 0; i < source.children.length; i++) inlineStyles(source.children[i], target.children[i]);
}

async function prepareFrame(clips, time, viewport) {
  await syncDirectComponents(clips, time, viewport, "export");
  await new Promise((resolve) => requestAnimationFrame(() => resolve()));
}

async function captureCompositeFrame(outputSpec) {
  const source = document.getElementById("userComponentOverlay");
  const preview = document.getElementById("preview");
  if (!source || !preview) throw new Error("compositor surface is unavailable");
  const clone = source.cloneNode(true);
  inlineStyles(source, clone);
  clone.querySelectorAll("#safeOverlay,#exportFrameOverlay").forEach((node) => node.remove());
  const visibleHosts = [...source.children].filter((host) => getComputedStyle(host).display !== "none");
  const nativeSvgNodes = visibleHosts.map((host) => host.querySelector(":scope > svg") || host.querySelector("svg")).filter(Boolean);
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${outputSpec.width}" height="${outputSpec.height}" viewBox="0 0 ${preview.width} ${preview.height}"><foreignObject width="100%" height="100%" x="0" y="0"><div xmlns="http://www.w3.org/1999/xhtml" style="width:${preview.width}px;height:${preview.height}px">${clone.outerHTML}</div></foreignObject></svg>`;
  const encoded = new TextEncoder().encode(svg);
  let binary = "";
  for (const byte of encoded) binary += String.fromCharCode(byte);
  const url = "data:image/svg+xml;base64," + btoa(binary);
  const canvas = document.createElement("canvas");
  canvas.width = outputSpec.width; canvas.height = outputSpec.height;
  const context = canvas.getContext("2d");
  if (!context) throw new Error("composite capture canvas unavailable");
  context.drawImage(preview, 0, 0, canvas.width, canvas.height);
  // Native annotation components are SVG documents. Rasterize them directly
  // instead of putting them inside foreignObject; WebEngine/ffmpeg paths may
  // otherwise drop the component layer while Card6's HTML happens to survive.
  if (nativeSvgNodes.length === visibleHosts.length && nativeSvgNodes.length > 0) {
    const overlayRect = source.getBoundingClientRect();
    const parts = nativeSvgNodes.map((svgNode) => {
      const rect = svgNode.getBoundingClientRect();
      const x = ((rect.left - overlayRect.left) / overlayRect.width) * outputSpec.width;
      const y = ((rect.top - overlayRect.top) / overlayRect.height) * outputSpec.height;
      const w = (rect.width / overlayRect.width) * outputSpec.width;
      const h = (rect.height / overlayRect.height) * outputSpec.height;
      const viewBox = svgNode.getAttribute("viewBox") || `0 0 ${rect.width} ${rect.height}`;
      return `<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${viewBox}" preserveAspectRatio="none">${svgNode.innerHTML}</svg>`;
    }).join("");
    const directSvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${outputSpec.width}" height="${outputSpec.height}">${parts}</svg>`;
    const directImage = new Image();
    await new Promise((resolve, reject) => { directImage.onload = resolve; directImage.onerror = reject; directImage.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(directSvg); });
    context.drawImage(directImage, 0, 0, outputSpec.width, outputSpec.height);
    return canvas;
  }
  const previewRect = preview.getBoundingClientRect();
  const nativeSvgs = [...source.querySelectorAll(":scope > [data-user-component] > svg")].filter((node) => getComputedStyle(node).display !== "none");
  for (const node of nativeSvgs) {
    const box = node.getBoundingClientRect();
    const markup = new XMLSerializer().serializeToString(node);
    const image = new Image();
    await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = reject; image.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(markup); });
    context.drawImage(image,
      (box.left - previewRect.left) * outputSpec.width / previewRect.width,
      (box.top - previewRect.top) * outputSpec.height / previewRect.height,
      box.width * outputSpec.width / previewRect.width,
      box.height * outputSpec.height / previewRect.height);
  }
  // Native SVG hosts are already painted above; leave only legacy DOM hosts in
  // the foreignObject fallback so they are not painted twice.
  nativeSvgs.forEach((node) => node.parentElement?.remove());
  if (!clone.children.length) return canvas;
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

async function loadManifest(id = "demo") {
  const r = await fetch(`/api/components/${encodeURIComponent(id)}`);
  if (!r.ok) throw new Error(`component manifest unavailable: ${r.status}`);
  return r.json();
}

async function mountDirectComponent(id, props = {}, time = 0, mode = "preview", viewport = null) {
  if (!overlay) return null;
  const manifest = await loadManifest(id);
  if (!ALLOWED_RUNTIMES.has(manifest.runtime || "html-css")) throw new Error("unsupported native component runtime");
  const entryPath = String(manifest.entry || "").replace(/^\.\//, "");
  if (!entryPath || entryPath.includes("..") || entryPath.startsWith("/")) throw new Error("invalid component entry");
  if (manifest.style) {
    const link = document.createElement("link");
    link.rel = "stylesheet";
    link.href = `/components/${encodeURIComponent(id)}/${manifest.style.replace(/^\.\//, "")}`;
    document.head.appendChild(link);
  }
  const host = document.createElement("div");
  host.dataset.userComponent = id;
  host.style.zIndex = "10";
  overlay.appendChild(host);
  const mod = await import(`/components/${encodeURIComponent(id)}/${entryPath}`);
  if (typeof mod.mount !== "function") throw new Error("user component must export mount({host, props, time})");
  const instance = await mod.mount({ host, props, time, mode, viewport });
  return { manifest, host, instance };
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
    if (entry && entry.componentId !== (clip.componentId || "demo")) {
      entry.instance.destroy?.(); entry.host.remove(); mounted.delete(clip.id); entry = null;
    }
    if (!entry) {
      let pending = mounting.get(clip.id);
      if (!pending) {
        pending = mountDirectComponent(clip.componentId || "demo", clip.props || {}, time - clip.start, mode, renderViewport);
        mounting.set(clip.id, pending);
      }
      try { entry = await pending; } finally {
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
      await entry.instance.update?.(clip.props || {}, time - clip.start, renderViewport, mode);
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
    entry.instance.update?.(clip.props || {}, time - (clip.start || 0), renderViewport, mode);
  }
}

window.fablecutDirectComponents = { mountDirectComponent, syncDirectComponents, updateDirectComponent, prepareFrame, captureCompositeFrame };
