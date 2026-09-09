/* Direct browser component runtime. User modules render real DOM/SVG; no IR. */
const overlay = document.getElementById("userComponentOverlay");
const mounted = new Map();

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

window.fablecutDirectComponents = { mountDirectComponent, syncDirectComponents, updateDirectComponent };
