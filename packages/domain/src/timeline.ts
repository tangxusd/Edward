import { z } from 'zod';

export const TrackIdSchema = z.enum([
  'mainMedia',
  'background',
  'subtitles',
  'cards',
  'graphics',
]);

export const TimelineClipSchema = z.object({
  id: z.string().min(1),
  start: z.number().finite().nonnegative(),
  duration: z.number().finite().nonnegative(),
  content: z.unknown(),
  styleId: z.string().min(1),
  locked: z.boolean(),
  layout: z.object({ x: z.number().finite(), y: z.number().finite(), width: z.number().positive(), height: z.number().positive() }).optional(),
  sourceRange: z
    .object({
      start: z.number().finite().nonnegative(),
      end: z.number().finite().nonnegative(),
    })
    .optional(),
});

export const TrackSchema = z.object({
  id: TrackIdSchema,
  clips: z.array(TimelineClipSchema),
});

export type TrackId = z.infer<typeof TrackIdSchema>;
export type TimelineClip = z.infer<typeof TimelineClipSchema>;
export type Track = z.infer<typeof TrackSchema>;

export type ProjectWithTracks = {
  tracks: Record<TrackId, Track>;
};

function updateClip<T extends ProjectWithTracks>(
  project: T,
  clipId: string,
  update: (clip: TimelineClip) => TimelineClip,
): T {
  for (const trackId of TrackIdSchema.options) {
    const track = project.tracks[trackId];
    const clipIndex = track.clips.findIndex((clip) => clip.id === clipId);

    if (clipIndex === -1) {
      continue;
    }

    const clips = track.clips.map((clip, index) =>
      index === clipIndex ? update(clip) : clip,
    );

    return {
      ...project,
      tracks: {
        ...project.tracks,
        [trackId]: {
          ...track,
          clips,
        },
      },
    };
  }

  throw new Error(`clip not found: ${clipId}`);
}

export function moveClip<T extends ProjectWithTracks>(
  project: T,
  clipId: string,
  start: number,
): T {
  if (!Number.isFinite(start) || start < 0) {
    throw new Error('start must be non-negative');
  }

  return updateClip(project, clipId, (clip) => ({ ...clip, start }));
}

export function resizeClip<T extends ProjectWithTracks>(
  project: T,
  clipId: string,
  duration: number,
): T {
  if (!Number.isFinite(duration) || duration <= 0) {
    throw new Error('duration must be positive');
  }

  return updateClip(project, clipId, (clip) => ({ ...clip, duration }));
}

function copyId(track: Track, clipId: string): string {
  const baseId = `${clipId}-copy`;
  let id = baseId;
  let suffix = 2;

  while (track.clips.some((clip) => clip.id === id)) {
    id = `${baseId}-${suffix}`;
    suffix += 1;
  }

  return id;
}

export function copyClip<T extends ProjectWithTracks>(
  project: T,
  clipId: string,
): T {
  for (const trackId of TrackIdSchema.options) {
    const track = project.tracks[trackId];
    const clip = track.clips.find((candidate) => candidate.id === clipId);

    if (!clip) {
      continue;
    }

    return {
      ...project,
      tracks: {
        ...project.tracks,
        [trackId]: {
          ...track,
          clips: [...track.clips, { ...clip, id: copyId(track, clipId) }],
        },
      },
    };
  }

  throw new Error(`clip not found: ${clipId}`);
}

export function deleteClip<T extends ProjectWithTracks>(
  project: T,
  clipId: string,
): T {
  for (const trackId of TrackIdSchema.options) {
    const track = project.tracks[trackId];
    const clips = track.clips.filter((clip) => clip.id !== clipId);

    if (clips.length === track.clips.length) {
      continue;
    }

    return {
      ...project,
      tracks: {
        ...project.tracks,
        [trackId]: {
          ...track,
          clips,
        },
      },
    };
  }

  throw new Error(`clip not found: ${clipId}`);
}
