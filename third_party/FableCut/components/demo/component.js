export function mount({ host, props, time }) {
  const node = document.createElement("div");
  node.className = "card-6";
  node.innerHTML = `<div class="card-6__holo"><div class="card-6__layer card-6__layer--back">6</div><div class="card-6__layer card-6__layer--mid">6</div><div class="card-6__layer card-6__layer--front">6</div></div>`;
  host.appendChild(node);
  function update(next = props, t = time || 0) {
    node.style.transform = `translateX(${Number(next.x) || 0}px)`;
    node.querySelectorAll(".card-6__layer").forEach((layer) => {
      layer.style.color = next.color || "#fff";
      if (next.title) layer.textContent = next.title;
    });
    node.dataset.time = String(t);
  }
  update(props, time);
  return { update, destroy: () => node.remove() };
}
