const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

function values(props, time, viewport) {
  const width = viewport?.width || 1280;
  const height = viewport?.height || 720;
  const duration = Math.max(0.1, Number(props.duration ?? 3));
  const u = clamp(time / 1, 0, 1);
  const progress = props.progress == null ? 1 - Math.pow(1 - u, 3) : clamp(Number(props.progress), 0, 1);
  const scale = Math.max(0.05, Number(props.scale ?? 1));
  return { width, height, cssWidth: viewport?.cssWidth || width, cssHeight: viewport?.cssHeight || height, progress, x: Number(props.x ?? 0), y: Number(props.y ?? 0), boxWidth: width * Number(props.width ?? 0.56) * scale, boxHeight: height * Number(props.height ?? 0.28) * scale, borderWidth: Number(props.borderWidth ?? 2), radius: Number(props.radius ?? 5) * scale, color: props.color || "#1683ff" };
}

export function mount({ host, props, time, viewport }) {
  if (!window.React || !window.ReactDOM) throw new Error("React runtime is unavailable");
  const root = window.ReactDOM.createRoot(host);
  const render = (next, at, size) => {
    const v = values(next, at, size);
    const sx = v.cssWidth / v.width, sy = v.cssHeight / v.height;
    const perimeter = 2 * (v.boxWidth + v.boxHeight);
    const rect = window.React.createElement("rect", { x: 0, y: 0, width: v.boxWidth, height: v.boxHeight, rx: v.radius, fill: "none", stroke: v.color, strokeWidth: v.borderWidth, strokeDasharray: perimeter, strokeDashoffset: perimeter * (1 - v.progress) });
    root.render(window.React.createElement("svg", { style: { position: "absolute", left: `${(v.width / 2 + v.x - v.boxWidth / 2) * sx}px`, top: `${(v.height / 2 + v.y - v.boxHeight / 2) * sy}px`, width: `${v.boxWidth * sx}px`, height: `${v.boxHeight * sy}px`, overflow: "visible", pointerEvents: "none", opacity: Number(next.opacity ?? 1), transform: `rotate(${Number(next.rotation ?? 0)}deg)`, transformOrigin: "center" }, viewBox: `0 0 ${v.boxWidth} ${v.boxHeight}` }, rect));
  };
  render(props, time, viewport);
  return { update(next, at, size) { render(next, at, size); }, destroy() { root.unmount(); } };
}
