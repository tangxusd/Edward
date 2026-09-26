const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  if (!window.gsap) throw new Error("GSAP runtime is unavailable");
  const node = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  const drawRect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  node.appendChild(drawRect);
  host.appendChild(node);
  const state = { progress: 0 };
  const timeline = window.gsap.timeline({ paused: true }).to(state, { progress: 1, duration: 1, ease: "power3.out" });
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720; const sx = (size?.cssWidth || w) / w; const sy = (size?.cssHeight || h) / h;
    const scale = Math.max(0.05, Number(next.scale ?? 1));
    const bw = w * Number(next.width ?? 0.56) * scale; const bh = h * Number(next.height ?? 0.28) * scale;
    timeline.seek(Math.max(0, Number(next.progress == null ? at : next.progress)));
    const p = clamp(state.progress, 0, 1); const radius = Number(next.radius ?? 5) * scale;
    Object.assign(node.style, { position: "absolute", left: `${(w / 2 + Number(next.x ?? 0) - bw / 2) * sx}px`, top: `${(h / 2 + Number(next.y ?? 0) - bh / 2) * sy}px`, width: `${bw * sx}px`, height: `${bh * sy}px`, overflow: "visible", pointerEvents: "none", opacity: Number(next.opacity ?? 1), transform: `rotate(${Number(next.rotation ?? 0)}deg)`, transformOrigin: "center" });
    node.setAttribute("viewBox", `0 0 ${bw} ${bh}`);
    drawRect.setAttribute("x", "0"); drawRect.setAttribute("y", "0"); drawRect.setAttribute("width", String(bw)); drawRect.setAttribute("height", String(bh)); drawRect.setAttribute("rx", String(radius)); drawRect.setAttribute("fill", "none"); drawRect.setAttribute("stroke", next.color || "#1683ff"); drawRect.setAttribute("stroke-width", String(Number(next.borderWidth ?? 2)));
    const dashLength = Math.max(p, 0.0001);
    drawRect.setAttribute("pathLength", "1"); drawRect.setAttribute("stroke-dasharray", `${dashLength} 1`); drawRect.setAttribute("stroke-dashoffset", "0");
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { timeline.kill(); node.remove(); } };
}
