const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  svg.appendChild(rect); host.appendChild(svg);
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720; const sx = (size?.cssWidth || w) / w; const sy = (size?.cssHeight || h) / h;
    const scale = Math.max(0.05, Number(next.scale ?? 1));
    const rw = w * Number(next.width ?? 0.56) * scale; const rh = h * Number(next.height ?? 0.28) * scale;
    const u = clamp(at / 1, 0, 1);
    const p = clamp(next.progress == null ? 1 - Math.pow(1 - u, 3) : Number(next.progress), 0, 1);
    const radius = Number(next.radius ?? 5) * scale; const perimeter = 2 * (rw + rh);
    Object.assign(svg.style, { position: "absolute", left: `${(w / 2 + Number(next.x ?? 0) - rw / 2) * sx}px`, top: `${(h / 2 + Number(next.y ?? 0) - rh / 2) * sy}px`, width: `${rw * sx}px`, height: `${rh * sy}px`, overflow: "visible", pointerEvents: "none", opacity: Number(next.opacity ?? 1), transform: `rotate(${Number(next.rotation ?? 0)}deg)`, transformOrigin: "center" });
    rect.setAttribute("x", "0"); rect.setAttribute("y", "0"); rect.setAttribute("width", String(rw)); rect.setAttribute("height", String(rh)); rect.setAttribute("rx", String(radius)); rect.setAttribute("fill", "none"); rect.setAttribute("stroke", next.color || "#1683ff"); rect.setAttribute("stroke-width", String(Number(next.borderWidth ?? 2))); rect.setAttribute("stroke-dasharray", String(perimeter)); rect.setAttribute("stroke-dashoffset", String(perimeter * (1 - p)));
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { svg.remove(); } };
}
