# Setup status & handoff

This file is a quick orientation for picking the project up in a fresh session.
For full details, read `README.md`.

## What this project is
Personal website + blog for Raul Valdez. **Astro** static site, deployed on
**Cloudflare Pages**, with a no-code blog editor (**Decap CMS**) at `/admin`.
Blog posts are Markdown files in `src/content/blog/`; the menu (Home/Blog/About/
Contact) is in `src/components/Header.astro`.

## Progress checklist
- [x] Website code written and verified (`npm run build` succeeds locally)
- [x] New GitHub repo `Raul-dot-valdez/raul-website` created
- [x] Code pushed into `raul-website` (main branch)
- [ ] **Cloudflare Pages connected** to the repo (preset: Astro, build:
      `npm run build`, output: `dist`)
- [ ] **GitHub OAuth App created** (Homepage = live URL, Callback =
      `<live-url>/api/callback`)
- [ ] **Cloudflare env vars set**: `GITHUB_OAUTH_ID`, `GITHUB_OAUTH_SECRET`
      (then redeploy)
- [ ] Verified login + publishing works at `<live-url>/admin/`

## Values to confirm once the site is live
If the Cloudflare URL is NOT `https://raul-website.pages.dev`, update
`base_url:` in `public/admin/config.yml` to the real URL, then commit & push.
The `repo:` line there must read `Raul-dot-valdez/raul-website`.

## Next action
Connect Cloudflare Pages (README → "Step 2"), then do the GitHub OAuth App and
env vars (README → "Step 3" and "Step 4"). Update the checkboxes above as you go.
