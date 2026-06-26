# raul-website

My personal website and blog. Built with [Astro](https://astro.build) and
deployed for free on [Cloudflare Pages](https://pages.cloudflare.com). Blog
posts are written from a no-code admin panel (Decap CMS) — no editing files to
publish.

- **Home / About / Contact / Blog** menu on every page.
- **Blog** posts live as Markdown in `src/content/blog/` and are published from
  the `/admin` editor.
- **No-code publishing:** write a post in a web editor, click *Publish*, and the
  site rebuilds and goes live automatically.

---

## One-time setup

> The website code is finished. These steps put it on your own GitHub account
> and the public internet. You only do them once.

### Step 1 — Create the repo and push this folder

This folder was built inside another repository. Move it into its own new repo:

1. On GitHub, create a new **empty** repository named **`raul-website`**
   (Private is fine). Do **not** add a README/.gitignore — keep it empty.
2. On your computer, in a terminal, from inside this `raul-website` folder:

   ```bash
   # Start a fresh git history for just this folder
   rm -rf .git
   git init -b main
   git add .
   git commit -m "Initial website"
   git remote add origin https://github.com/Raul-dot-valdez/raul-website.git
   git push -u origin main
   ```

   (If you cloned only this subfolder, you can skip `rm -rf .git`.)

### Step 2 — Connect Cloudflare Pages

1. In the Cloudflare dashboard: **Workers & Pages → Create → Pages → Connect to
   Git**, and select the `raul-website` repo.
2. Use these build settings:
   - **Framework preset:** `Astro`
   - **Build command:** `npm run build`
   - **Build output directory:** `dist`
3. Click **Save and Deploy**. In ~1 minute your site is live at
   `https://raul-website.pages.dev` (your exact URL is shown in the dashboard).

> If your Cloudflare URL is **not** `raul-website.pages.dev`, open
> `public/admin/config.yml` and update the `base_url:` line to your real URL,
> then commit and push.

### Step 3 — Create a GitHub OAuth App (powers the `/admin` login)

The admin panel signs you in with GitHub so it can save posts to the repo.

1. Go to **GitHub → Settings → Developer settings → OAuth Apps → New OAuth App**
   (https://github.com/settings/developers).
2. Fill in:
   - **Application name:** `raul-website admin`
   - **Homepage URL:** `https://raul-website.pages.dev` (your live URL)
   - **Authorization callback URL:** `https://raul-website.pages.dev/api/callback`
3. Click **Register application**, then **Generate a new client secret**.
4. Copy the **Client ID** and **Client secret** — you need them next.

### Step 4 — Add the keys to Cloudflare

1. In Cloudflare: **your Pages project → Settings → Environment variables →
   Production**, and add two variables:
   - `GITHUB_OAUTH_ID` = your Client ID
   - `GITHUB_OAUTH_SECRET` = your Client secret (mark it as a *Secret*)
2. **Redeploy** the project (Deployments → ⋯ → Retry deployment) so the
   variables take effect.

### Done — write your first post

Visit `https://raul-website.pages.dev/admin/`, click **Login with GitHub**, and
write a post. When you click **Publish**, it commits to the repo and Cloudflare
rebuilds the site within a minute or two.

---

## Everyday use

### Writing blog posts (no code)
Go to **`/admin`** on your live site → **Blog Posts → New Blog Post**. Fill in
the title, date, and body, then **Publish**. Set **Draft** on to hide a post
from the site while you work on it.

### Editing the menu or the Home/About/Contact pages
These are part of the site design, so you edit files (or ask me to):
- **Menu links:** `src/components/Header.astro`
- **Home page:** `src/pages/index.astro`
- **About page:** `src/pages/about.astro`
- **Contact page:** `src/pages/contact.astro`

After editing, `git commit` and `git push` — Cloudflare redeploys automatically.

### Adding a new menu item / sub-page
1. Create a file like `src/pages/projects.astro` (copy `about.astro` as a start).
2. Add `{ href: '/projects', label: 'Projects' }` to the list in
   `src/components/Header.astro`.

---

## Run it locally (optional)

Requires [Node.js](https://nodejs.org) 18+.

```bash
npm install      # first time only
npm run dev      # preview at http://localhost:4321
npm run build    # production build into dist/
```

> Note: the `/admin` login only works on the deployed Cloudflare site (it needs
> the OAuth functions and environment variables). Locally you can still preview
> the public pages.

---

## How it fits together

| Piece | What it does |
| --- | --- |
| `src/pages/` | Each file is a page/route (Home, About, Contact, Blog). |
| `src/content/blog/` | Your blog posts, one Markdown file each. |
| `src/content.config.ts` | Defines the fields a blog post can have. |
| `src/layouts/` & `src/components/` | Shared page shell, header menu, footer. |
| `public/admin/` | The Decap CMS no-code editor (`/admin`). |
| `functions/api/` | Cloudflare login functions for the admin panel. |
| `public/uploads/` | Images you upload from the editor. |
