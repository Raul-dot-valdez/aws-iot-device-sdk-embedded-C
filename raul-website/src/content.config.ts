import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

// The "blog" collection. Each Markdown file in src/content/blog becomes a post.
// Decap CMS (the /admin panel) writes new posts here automatically, so the
// fields below must match the fields configured in public/admin/config.yml.
const blog = defineCollection({
  loader: glob({ pattern: '**/*.md', base: './src/content/blog' }),
  schema: z.object({
    title: z.string(),
    description: z.string().optional(),
    // Accepts the ISO date string that Decap CMS writes.
    date: z.coerce.date(),
    draft: z.boolean().optional().default(false),
  }),
});

export const collections = { blog };
