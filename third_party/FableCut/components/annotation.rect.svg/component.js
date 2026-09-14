const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  svg.appendChild(rect); host.appendChild(svg);
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720;
    const rw = w * Number(next.width ?? 0.56); const rh = h * Number(next.height ?? 0.28);
    const duration = Math.max(0.1, Number(next.duration ?? 2));
    const p = clamp(next.progress == null ? at / duration : Number(next.progress), 0, 1);
    const radius = Number(next.radius ?? 5); const perimeter = 2 * (rw + rh);
    Object.assign(svg.style, { position: "absolute", left: `${Number(next.x ?? 0.5) * w - rw / 2}px`, top: `${Number(next.y ?? 0.5) * h - rh / 2}px`, width: `${rw}px`, height: `${rh}px`, overflow: "visible", pointerEvents: "none" });
    rect.setAttribute("x", "0"); rect.setAttribute("y", "0"); rect.setAttribute("width", String(rw)); rect.setAttribute("height", String(rh)); rect.setAttribute("rx", String(radius)); rect.setAttribute("fill", "none"); rect.setAttribute("stroke", next.color || "#1683ff"); rect.setAttribute("stroke-width", String(Number(next.borderWidth ?? 2))); rect.setAttribute("stroke-dasharray", String(perimeter)); rect.setAttribute("stroke-dashoffset", String(perimeter * (1 - p)));
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { svg.remove(); } };
}
