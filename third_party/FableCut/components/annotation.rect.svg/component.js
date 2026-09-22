const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  const drawRect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  svg.appendChild(drawRect); host.appendChild(svg);
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720; const sx = (size?.cssWidth || w) / w; const sy = (size?.cssHeight || h) / h;
    const scale = Math.max(0.05, Number(next.scale ?? 1));
    const rw = w * Number(next.width ?? 0.56) * scale; const rh = h * Number(next.height ?? 0.28) * scale;
    const u = clamp(at / 1, 0, 1);
    const p = clamp(next.progress == null ? 1 - Math.pow(1 - u, 3) : Number(next.progress), 0, 1);
    const radius = Number(next.radius ?? 5) * scale;
    const x = Number(next.x ?? 0), y = Number(next.y ?? 0), rotation = Number(next.rotation ?? 0);
    svg.dataset.edwardX = String(x); svg.dataset.edwardY = String(y); svg.dataset.edwardWidth = String(rw); svg.dataset.edwardHeight = String(rh); svg.dataset.edwardViewportWidth = String(w); svg.dataset.edwardViewportHeight = String(h); svg.dataset.edwardRotation = String(rotation);
    Object.assign(svg.style, { position: "absolute", left: `${(w / 2 + x - rw / 2) * sx}px`, top: `${(h / 2 - y - rh / 2) * sy}px`, width: `${rw * sx}px`, height: `${rh * sy}px`, overflow: "visible", pointerEvents: "none", opacity: Number(next.opacity ?? 1), transform: `rotate(${rotation}deg)`, transformOrigin: "center" });
    drawRect.setAttribute("x", "0"); drawRect.setAttribute("y", "0"); drawRect.setAttribute("width", String(rw)); drawRect.setAttribute("height", String(rh)); drawRect.setAttribute("rx", String(radius)); drawRect.setAttribute("fill", "none"); drawRect.setAttribute("stroke", next.color || "#1683ff"); drawRect.setAttribute("stroke-width", String(Number(next.borderWidth ?? 2)));
    const dashLength = Math.max(p, 0.0001);
    drawRect.setAttribute("pathLength", "1"); drawRect.setAttribute("stroke-dasharray", `${dashLength} 1`); drawRect.setAttribute("stroke-dashoffset", "0");
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { svg.remove(); } };
}
