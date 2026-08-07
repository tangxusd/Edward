import { z } from 'zod';

import {
  TrackSchema,
  type TimelineClip,
  type Track,
  type TrackId,
} from './timeline.js';

export const DEFAULT_BACKGROUND_RESOURCE_ID = 'default-background';

export const MediaKindSchema = z.enum(['audio', 'video']);

export const ProjectSchema = z.object({
  id: z.string().min(1),
  scriptPath: z.string().min(1),
  media: z.object({
    path: z.string().min(1),
    kind: MediaKindSchema,
  }),
  defaultBackgroundResourceId: z.string().min(1),
  archivedAt: z.string().datetime().optional(),
  tracks: z.object({
    mainMedia: TrackSchema,
    background: TrackSchema,
    subtitles: TrackSchema,
    cards: TrackSchema,
    graphics: TrackSchema,
  }),
});

export const CreateProjectInputSchema = z.object({
  id: z.string().min(1).optional(),
  scriptPath: z.string().min(1),
  mediaPath: z.string().min(1),
  mediaKind: MediaKindSchema,
  defaultBackgroundResourceId: z.string().min(1).optional(),
});

export type Project = z.infer<typeof ProjectSchema>;
export type CreateProjectInput = z.infer<typeof CreateProjectInputSchema>;

function createTrack(id: TrackId, clips: TimelineClip[] = []): Track {
  return { id, clips };
}

function createProjectId(): string {
  return `project-${crypto.randomUUID()}`;
}

export function createProject(input: CreateProjectInput): Project {
  const parsed = CreateProjectInputSchema.parse(input);
  const defaultBackgroundResourceId =
    parsed.defaultBackgroundResourceId ?? DEFAULT_BACKGROUND_RESOURCE_ID;
  const mainMediaClip: TimelineClip = {
    id: 'main-media',
    start: 0,
    duration: 0,
    content: { path: parsed.mediaPath },
    styleId: 'main-media',
    locked: false,
  };
  const backgroundClips: TimelineClip[] =
    parsed.mediaKind === 'audio'
      ? [
          {
            id: 'default-background',
            start: 0,
            duration: 0,
            content: { resourceId: defaultBackgroundResourceId },
            styleId: defaultBackgroundResourceId,
            locked: false,
          },
        ]
      : [];

  return ProjectSchema.parse({
    id: parsed.id ?? createProjectId(),
    scriptPath: parsed.scriptPath,
    media: {
      path: parsed.mediaPath,
      kind: parsed.mediaKind,
    },
    defaultBackgroundResourceId,
    tracks: {
      mainMedia: createTrack('mainMedia', [mainMediaClip]),
      background: createTrack('background', backgroundClips),
      subtitles: createTrack('subtitles'),
      cards: createTrack('cards'),
      graphics: createTrack('graphics'),
    },
  });
}
