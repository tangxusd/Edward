const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  const node = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  node.appendChild(rect);
  host.appendChild(node);
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720; const sx = (size?.cssWidth || w) / w; const sy = (size?.cssHeight || h) / h;
    const scale = Math.max(0.05, Number(next.scale ?? 1));
    const bw = w * Number(next.width ?? 0.56) * scale; const bh = h * Number(next.height ?? 0.28) * scale;
    const u = clamp(at / 1, 0, 1);
    const p = clamp(next.progress == null ? 1 - Math.pow(1 - u, 3) : Number(next.progress), 0, 1);
    const radius = Number(next.radius ?? 5) * scale;
    Object.assign(node.style, { position: "absolute", left: `${(w / 2 + Number(next.x ?? 0) - bw / 2) * sx}px`, top: `${(h / 2 + Number(next.y ?? 0) - bh / 2) * sy}px`, width: `${bw * sx}px`, height: `${bh * sy}px`, overflow: "visible", pointerEvents: "none", opacity: Number(next.opacity ?? 1), transform: `rotate(${Number(next.rotation ?? 0)}deg)`, transformOrigin: "center" });
    node.setAttribute("viewBox", `0 0 ${bw} ${bh}`); rect.setAttribute("x", "0"); rect.setAttribute("y", "0"); rect.setAttribute("width", String(bw)); rect.setAttribute("height", String(bh)); rect.setAttribute("rx", String(radius)); rect.setAttribute("fill", "none"); rect.setAttribute("stroke", next.color || "#1683ff"); rect.setAttribute("stroke-width", String(Number(next.borderWidth ?? 2))); rect.setAttribute("pathLength", "1"); rect.setAttribute("stroke-dasharray", "1"); rect.setAttribute("stroke-dashoffset", String(1 - p));
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { node.remove(); } };
}
