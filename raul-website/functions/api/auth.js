// Cloudflare Pages Function: starts the GitHub OAuth login for Decap CMS.
// Reached at /api/auth — Decap opens this in a popup when you click "Login".
//
// Requires two environment variables set in the Cloudflare Pages project:
//   GITHUB_OAUTH_ID      = your GitHub OAuth App "Client ID"
//   GITHUB_OAUTH_SECRET  = your GitHub OAuth App "Client secret"  (used in callback.js)

export async function onRequest(context) {
  const { request, env } = context;
  const url = new URL(request.url);

  if (!env.GITHUB_OAUTH_ID) {
    return new Response(
      'Missing GITHUB_OAUTH_ID environment variable in Cloudflare Pages.',
      { status: 500 },
    );
  }

  const redirectUri = `${url.origin}/api/callback`;

  const authorize = new URL('https://github.com/login/oauth/authorize');
  authorize.searchParams.set('client_id', env.GITHUB_OAUTH_ID);
  authorize.searchParams.set('redirect_uri', redirectUri);
  // "repo" scope lets the CMS commit posts to your repository.
  authorize.searchParams.set('scope', 'repo,user');
  authorize.searchParams.set('state', crypto.randomUUID());

  return Response.redirect(authorize.toString(), 302);
}
