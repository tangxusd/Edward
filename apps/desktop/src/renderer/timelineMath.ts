export type Rect = { x: number; y: number; width: number; height: number };

/** Pixels per second for timeline rendering */
export const PIXELS_PER_SECOND = 60;

/** Minimum clip width in pixels */
export const MIN_CLIP_PX = 20;

/** Convert time in seconds to pixel x-position */
export function timeToPixel(time: number): number {
  return time * PIXELS_PER_SECOND;
}

/** Convert pixel x-position to time in seconds */
export function pixelToTime(pixel: number): number {
  return pixel / PIXELS_PER_SECOND;
}

/** Snap a value to nearby guides */
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

/** Generate tick marks for the time ruler */
export function generateTickMarks(totalSeconds: number): Array<{ position: number; label: string; isMajor: boolean }> {
  const ticks: Array<{ position: number; label: string; isMajor: boolean }> = [];
  const interval = 1; // 1 second intervals
  for (let t = 0; t <= totalSeconds; t += interval) {
    const position = timeToPixel(t);
    const minutes = Math.floor(t / 60);
    const seconds = Math.floor(t % 60);
    const label = minutes > 0
      ? `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`
      : `00:${String(seconds).padStart(2, '0')}`;
    ticks.push({ position, label, isMajor: t % 5 === 0 });
  }
  return ticks;
}

/** Get the clip color based on track type */
export function getClipColor(trackId: string): string {
  switch (trackId) {
    case 'graphics':
    case 'cards':
      return '#84527b';
    case 'subtitles':
      return '#3d7671';
    case 'mainMedia':
    case 'background':
    default:
      return '#356e9e';
  }
}