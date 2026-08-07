import { z } from 'zod';

export const SubtitleStyleSchema = z.object({ fontFamily: z.string().min(1), fontSize: z.number().positive(), color: z.string().min(1), background: z.string().min(1) });
export type SubtitleStyle = z.infer<typeof SubtitleStyleSchema>;
