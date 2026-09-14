const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

function values(props, time, viewport) {
  const width = viewport?.width || 1280;
  const height = viewport?.height || 720;
  const duration = Math.max(0.1, Number(props.duration ?? 3));
  const u = clamp(time / 0.5, 0, 1);
  const progress = props.progress == null ? 1 - Math.pow(1 - u, 3) : clamp(Number(props.progress), 0, 1);
  const scale = Math.max(0.05, Number(props.scale ?? 1));
  return { width, height, progress, x: Number(props.x ?? 0.5), y: Number(props.y ?? 0.5), boxWidth: width * Number(props.width ?? 0.56) * scale, boxHeight: height * Number(props.height ?? 0.28) * scale, borderWidth: Number(props.borderWidth ?? 2), radius: Number(props.radius ?? 5) * scale, color: props.color || "#1683ff" };
}

export function mount({ host, props, time, viewport }) {
  if (!window.React || !window.ReactDOM) throw new Error("React runtime is unavailable");
  const root = window.ReactDOM.createRoot(host);
  const render = (next, at, size) => {
    const v = values(next, at, size);
    root.render(window.React.createElement("div", { style: { position: "absolute", left: `${v.x * v.width - v.boxWidth / 2}px`, top: `${v.y * v.height - v.boxHeight / 2}px`, width: `${v.boxWidth}px`, height: `${v.boxHeight}px`, boxSizing: "border-box", border: `${v.borderWidth}px solid ${v.color}`, borderRadius: `${v.radius}px`, clipPath: `inset(0 ${100 - v.progress * 100}% 0 0 round ${v.radius}px)`, pointerEvents: "none" } }));
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { root.unmount(); } };
}
