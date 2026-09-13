/* Direct browser component runtime. User modules render real DOM/SVG; no IR. */
const overlay = document.getElementById("userComponentOverlay");
const mounted = new Map();

function inlineStyles(source, target) {
  const computed = getComputedStyle(source);
  for (const name of computed) target.style.setProperty(name, computed.getPropertyValue(name));
  for (let i = 0; i < source.children.length; i++) inlineStyles(source.children[i], target.children[i]);
}

async function prepareFrame(clips, time, viewport) {
  await syncDirectComponents(clips, time);
  for (const clip of clips || []) {
    if (clip.kind !== "component") continue;
    const entry = mounted.get(clip.id);
    if (!entry) throw new Error(`component ${clip.id} is not mounted`);
    await entry.instance.update?.(clip.props || {}, time - clip.start, viewport);
  }
  await new Promise((resolve) => requestAnimationFrame(() => resolve()));
}

async function captureCompositeFrame(outputSpec) {
  const source = document.getElementById("monitorZoomInner");
  const preview = document.getElementById("preview");
  if (!source || !preview) throw new Error("compositor surface is unavailable");
  const clone = source.cloneNode(true);
  const cloneCanvas = clone.querySelector("canvas");
  if (cloneCanvas) {
    const image = document.createElement("img");
    image.src = preview.toDataURL("image/png");
    image.width = preview.width; image.height = preview.height;
    image.style.cssText = getComputedStyle(preview).cssText || "display:block";
    cloneCanvas.replaceWith(image);
  }
  clone.querySelectorAll("#safeOverlay,#exportFrameOverlay").forEach((node) => node.remove());
  inlineStyles(source, clone);
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${outputSpec.width}" height="${outputSpec.height}" viewBox="0 0 ${preview.width} ${preview.height}"><foreignObject width="100%" height="100%" x="0" y="0"><div xmlns="http://www.w3.org/1999/xhtml" style="width:${preview.width}px;height:${preview.height}px">${clone.outerHTML}</div></foreignObject></svg>`;
  const blob = new Blob([svg], { type: "image/svg+xml;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  try {
    const image = new Image();
    await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = reject; image.src = url; });
    const canvas = document.createElement("canvas");
    canvas.width = outputSpec.width; canvas.height = outputSpec.height;
    canvas.getContext("2d").drawImage(image, 0, 0, canvas.width, canvas.height);
    return canvas;
  } finally { URL.revokeObjectURL(url); }
}

async function loadManifest(id = "demo") {
  const r = await fetch(`/api/components/${encodeURIComponent(id)}`);
  if (!r.ok) throw new Error(`component manifest unavailable: ${r.status}`);
  return r.json();
}

async function mountDirectComponent(id, props = {}) {
  if (!overlay) return null;
  const manifest = await loadManifest(id);
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
  const mod = await import(`/components/${encodeURIComponent(id)}/${manifest.entry.replace(/^\.\//, "")}`);
  if (typeof mod.mount !== "function") throw new Error("user component must export mount({host, props, time})");
  const instance = await mod.mount({ host, props, time: 0 });
  return { manifest, host, instance };
}
async function syncDirectComponents(clips, time) {
  if (!overlay) return;
  const active = new Set();
  for (const clip of clips || []) {
    if (clip.kind !== "component") continue;
    active.add(clip.id);
    let entry = mounted.get(clip.id);
    if (!entry) { entry = await mountDirectComponent(clip.componentId || "demo", clip.props || {}); if (entry) mounted.set(clip.id, entry); }
    if (entry) { entry.host.style.display = "flex"; entry.instance.update?.(clip.props || {}, time - clip.start); }
  }
  for (const [id, entry] of mounted) { if (!active.has(id)) { entry.instance.destroy?.(); entry.host.remove(); mounted.delete(id); } }
}
function updateDirectComponent(clip, time = 0) {
  const entry = mounted.get(clip?.id);
  if (entry) entry.instance.update?.(clip.props || {}, time - (clip.start || 0));
}

window.fablecutDirectComponents = { mountDirectComponent, syncDirectComponents, updateDirectComponent, prepareFrame, captureCompositeFrame };
