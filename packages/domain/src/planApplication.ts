import type { AiEditPlan } from './aiPlan.js';
import type { Project } from './project.js';

export function applyAiPlan(project: Project, plan: AiEditPlan): Project {
  const tracks = { ...project.tracks };
  for (const track of ['subtitles', 'cards', 'graphics'] as const) tracks[track] = { ...tracks[track], clips: [] };
  for (const [index, clip] of plan.clips.entries()) {
    tracks[clip.track] = { ...tracks[clip.track], clips: [...tracks[clip.track].clips, { id: `ai-${clip.track}-${index}`, start: clip.start, duration: clip.duration, content: clip.content, styleId: clip.styleId, locked: false }] };
  }
  return { ...project, tracks };
}
