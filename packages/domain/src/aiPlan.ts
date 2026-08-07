import { z } from 'zod';

export const AiPlanClipSchema = z.object({ track: z.enum(['subtitles', 'cards', 'graphics']), start: z.number().nonnegative(), duration: z.number().positive(), content: z.unknown(), styleId: z.string().min(1) });
export const AiEditPlanSchema = z.object({ clips: z.array(AiPlanClipSchema), summary: z.string().min(1) });
export type AiPlanClip = z.infer<typeof AiPlanClipSchema>;
export type AiEditPlan = z.infer<typeof AiEditPlanSchema>;

export function validateAiEditPlan(value: unknown, mediaDurationMs: number): AiEditPlan {
  const plan = AiEditPlanSchema.parse(value);
  for (const clip of plan.clips) if ((clip.start + clip.duration) * 1000 > mediaDurationMs) throw new Error('clip exceeds media duration');
  return plan;
}
