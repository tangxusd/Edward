import { z } from 'zod';

const ResourceTypeSchema = z.enum(['background', 'card-style', 'timeline-style', 'chart', 'style-pack']);
const AssetPathSchema = z.string().min(1).refine(
  (path) => {
    const normalized = path.replace(/\\/g, '/');
    return !normalized.startsWith('/') && !normalized.includes(':') && !normalized.split('/').includes('..') && !normalized.includes('\0');
  },
  'asset paths must stay inside the package',
);

export const StylePackageManifestSchema = z.object({
  version: z.literal(1),
  id: z.string().min(1),
  name: z.string().min(1),
  type: ResourceTypeSchema,
  category: z.string().min(1),
  preview: AssetPathSchema.optional(),
  assets: z.array(AssetPathSchema).default([]),
  entryScript: z.never().optional(),
});

export type StylePackageManifest = z.infer<typeof StylePackageManifestSchema>;

export function parseStylePackageManifest(value: unknown): StylePackageManifest {
  return StylePackageManifestSchema.parse(value);
}
