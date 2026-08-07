import { z } from 'zod';

export const ResourceTypeSchema = z.enum([
  'background',
  'card-style',
  'timeline-style',
  'chart-style',
]);

export const ResourceSchema = z.object({
  id: z.string().min(1),
  type: ResourceTypeSchema,
  name: z.string().min(1),
  category: z.string().min(1),
  thumbnailPath: z.string().min(1).optional(),
  favorite: z.boolean(),
  style: z.unknown(),
  fileHash: z.string().min(1),
  updatedAt: z.string().datetime(),
});

export type Resource = z.infer<typeof ResourceSchema>;
