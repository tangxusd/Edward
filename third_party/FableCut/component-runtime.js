/* Direct browser component runtime. User modules render real DOM/SVG; no IR. */
const overlay = document.getElementById("userComponentOverlay");

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

window.fablecutDirectComponents = { mountDirectComponent };
mountDirectComponent("demo").catch((error) => {
  console.error("[FableCut] direct component failed", error);
});
