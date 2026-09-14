const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
export function mount({ host, props, time, viewport }) {
  if (!window.gsap) throw new Error("GSAP runtime is unavailable");
  const node = document.createElement("div");
  node.style.position = "absolute";
  node.style.boxSizing = "border-box";
  node.style.pointerEvents = "none";
  host.appendChild(node);
  const state = { progress: 0 };
  const timeline = window.gsap.timeline({ paused: true }).to(state, { progress: 1, duration: Math.max(0.1, Number(props.duration ?? 2)), ease: "none" });
  const render = (next, at, size) => {
    const w = size?.width || 1280; const h = size?.height || 720;
    const bw = w * Number(next.width ?? 0.56); const bh = h * Number(next.height ?? 0.28);
    timeline.duration(Math.max(0.1, Number(next.duration ?? 2)));
    timeline.seek(Math.max(0, Number(next.progress == null ? at : next.progress * timeline.duration())));
    const p = clamp(state.progress, 0, 1); const radius = Number(next.radius ?? 5);
    Object.assign(node.style, { left: `${Number(next.x ?? 0.5) * w - bw / 2}px`, top: `${Number(next.y ?? 0.5) * h - bh / 2}px`, width: `${bw}px`, height: `${bh}px`, border: `${Number(next.borderWidth ?? 2)}px solid ${next.color || "#1683ff"}`, borderRadius: `${radius}px`, clipPath: `inset(0 ${100 - p * 100}% 0 0 round ${radius}px)` });
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { timeline.kill(); node.remove(); } };
}
