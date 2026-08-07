import type { AiEditPlan, AiPlanClip } from './aiPlan.js';
import type { Project } from './project.js';
import type { TimelineClip, TrackId } from './timeline.js';
import { z } from 'zod';

export const AiPlanApplicationModeSchema = z.enum(['unmodified-only', 'new-only', 'replace-all']);
export type AiPlanApplicationMode = z.infer<typeof AiPlanApplicationModeSchema>;

function sourceId(clip: AiPlanClip, index: number): string {
  return `ai-${clip.track}-${index}`;
}

function createAiClip(clip: AiPlanClip, index: number): TimelineClip {
  const aiSourceId = sourceId(clip, index);
  return { id: aiSourceId, aiSourceId, start: clip.start, duration: clip.duration, content: clip.content, styleId: clip.styleId, locked: false };
}

function replaceUnmodified(current: TimelineClip[], incoming: TimelineClip[]): TimelineClip[] {
  const incomingIds = new Set(incoming.map((clip) => clip.aiSourceId));
  const bySource = new Map(current.filter((clip) => clip.aiSourceId).map((clip) => [clip.aiSourceId, clip]));
  const preserved = current.filter((clip) => !clip.aiSourceId || (clip.userEditedAt && !incomingIds.has(clip.aiSourceId)));
  const merged = incoming.map((clip) => {
    const existing = bySource.get(clip.aiSourceId);
    return existing?.userEditedAt ? existing : clip;
  });
  return [...preserved, ...merged].sort((left, right) => left.start - right.start);
}

export function applyAiPlan(project: Project, plan: AiEditPlan, mode: AiPlanApplicationMode = 'unmodified-only'): Project {
  const tracks = { ...project.tracks };
  for (const trackId of ['subtitles', 'cards', 'graphics'] as const satisfies TrackId[]) {
    const incoming = plan.clips.map((clip, index) => ({ clip, index })).filter(({ clip }) => clip.track === trackId).map(({ clip, index }) => createAiClip(clip, index));
    const current = tracks[trackId].clips;
    const clips = mode === 'replace-all'
      ? incoming
      : mode === 'new-only'
        ? [...current, ...incoming.filter((clip) => !current.some((existing) => existing.aiSourceId === clip.aiSourceId))]
        : replaceUnmodified(current, incoming);
    tracks[trackId] = { ...tracks[trackId], clips };
  }
  return { ...project, tracks };
}

export function markClipUserEdited(project: Project, clipId: string, content?: unknown): Project {
  for (const trackId of ['mainMedia', 'background', 'subtitles', 'cards', 'graphics'] as const satisfies TrackId[]) {
    const track = project.tracks[trackId];
    const clipIndex = track.clips.findIndex((clip) => clip.id === clipId);
    if (clipIndex === -1) continue;
    const clips = track.clips.map((clip, index) => index === clipIndex ? { ...clip, ...(content === undefined ? {} : { content }), userEditedAt: new Date().toISOString() } : clip);
    return { ...project, tracks: { ...project.tracks, [trackId]: { ...track, clips } } };
  }
  throw new Error(`clip not found: ${clipId}`);
}

export function setClipStyle(project: Project, clipId: string, styleId: string): Project {
  for (const trackId of ['mainMedia', 'background', 'subtitles', 'cards', 'graphics'] as const satisfies TrackId[]) {
    const track = project.tracks[trackId];
    if (!track.clips.some((clip) => clip.id === clipId)) continue;
    return { ...project, tracks: { ...project.tracks, [trackId]: { ...track, clips: track.clips.map((clip) => clip.id === clipId ? { ...clip, styleId, userEditedAt: new Date().toISOString() } : clip) } } };
  }
  throw new Error(`clip not found: ${clipId}`);
}
