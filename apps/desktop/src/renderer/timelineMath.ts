export type Rect = { x: number; y: number; width: number; height: number };

export function snap(value: number, guides: number[], threshold = 8): number {
  const guide = guides.find((candidate) => Math.abs(candidate - value) <= threshold);
  return guide ?? value;
}

export function resizeWithAspectRatio(rect: Rect, width: number, freeResize: boolean): Rect {
  if (freeResize) return { ...rect, width };
  return { ...rect, width, height: width / (rect.width / rect.height) };
}

export function distributeHorizontally(bounds: Rect, count: 2 | 3, gap: number): Rect[] {
  const width = (bounds.width - gap * (count - 1)) / count;
  return Array.from({ length: count }, (_, index) => ({ x: bounds.x + index * (width + gap), y: bounds.y, width, height: bounds.height }));
}
