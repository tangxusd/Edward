const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  const node = document.createElement("div");
  node.className = "native-annotation-rect";
  host.appendChild(node);
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720;
    const bw = w * Number(next.width ?? 0.56); const bh = h * Number(next.height ?? 0.28);
    const duration = Math.max(0.1, Number(next.duration ?? 2));
    const p = clamp(next.progress == null ? at / duration : Number(next.progress), 0, 1);
    const radius = Number(next.radius ?? 5);
    Object.assign(node.style, { position: "absolute", boxSizing: "border-box", pointerEvents: "none", left: `${Number(next.x ?? 0.5) * w - bw / 2}px`, top: `${Number(next.y ?? 0.5) * h - bh / 2}px`, width: `${bw}px`, height: `${bh}px`, border: `${Number(next.borderWidth ?? 2)}px solid ${next.color || "#1683ff"}`, borderRadius: `${radius}px`, clipPath: `inset(0 ${100 - p * 100}% 0 0 round ${radius}px)` });
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { node.remove(); } };
}
