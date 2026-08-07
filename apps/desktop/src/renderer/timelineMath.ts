export type Rect = { x: number; y: number; width: number; height: number };

export function snap(value: number, guides: number[], threshold = 8): number {
  const guide = guides.find((candidate) => Math.abs(candidate - value) <= threshold);
  return guide ?? value;
}

function axisCandidates(start: number, size: number): Array<{ position: number; offset: number }> {
  return [
    { position: start, offset: 0 },
    { position: start + size / 2, offset: size / 2 },
    { position: start + size, offset: size },
  ];
}

function snapAxis(start: number, size: number, guides: number[], threshold: number): { start: number; guides: number[] } {
  for (const candidate of axisCandidates(start, size)) {
    const guide = guides.find((value) => Math.abs(value - candidate.position) <= threshold);
    if (guide !== undefined) return { start: guide - candidate.offset, guides: [guide] };
  }
  return { start, guides: [] };
}

export function snapRectToGuides(rect: Rect, bounds: Rect, peers: Rect[], threshold = 8): { rect: Rect; guides: { x: number[]; y: number[] } } {
  const bounded = {
    ...rect,
    x: Math.max(bounds.x, Math.min(bounds.x + bounds.width - rect.width, rect.x)),
    y: Math.max(bounds.y, Math.min(bounds.y + bounds.height - rect.height, rect.y)),
  };
  const xGuides = [bounds.x + bounds.width / 2, ...peers.flatMap((peer) => axisCandidates(peer.x, peer.width).map((candidate) => candidate.position))];
  const yGuides = [bounds.y + bounds.height / 2, ...peers.flatMap((peer) => axisCandidates(peer.y, peer.height).map((candidate) => candidate.position))];
  const x = snapAxis(bounded.x, bounded.width, xGuides, threshold);
  const y = snapAxis(bounded.y, bounded.height, yGuides, threshold);
  return { rect: { ...bounded, x: x.start, y: y.start }, guides: { x: x.guides, y: y.guides } };
}

export function resizeWithAspectRatio(rect: Rect, width: number, freeResize: boolean): Rect {
  if (freeResize) return { ...rect, width };
  return { ...rect, width, height: width / (rect.width / rect.height) };
}

export function distributeHorizontally(bounds: Rect, count: 2 | 3, gap: number): Rect[] {
  const width = (bounds.width - gap * (count - 1)) / count;
  return Array.from({ length: count }, (_, index) => ({ x: bounds.x + index * (width + gap), y: bounds.y, width, height: bounds.height }));
}
