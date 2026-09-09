export function mount({ host, props, time }) {
  const node = document.createElement("div");
  node.className = "fablecut-demo-component";
  host.appendChild(node);
  function update(next = props, t = time || 0) {
    node.textContent = next.title || "";
    node.style.color = next.color || "#fff";
    node.style.transform = `translateX(${Number(next.x) || 0}px) rotate(${Math.sin(t * 4) * 3}deg)`;
  }
  update(props, time);
  return { update, destroy: () => node.remove() };
}
