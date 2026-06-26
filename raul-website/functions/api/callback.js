// Cloudflare Pages Function: completes GitHub OAuth for Decap CMS.
// GitHub redirects here (/api/callback) with a temporary code, which we
// exchange for an access token and hand back to the Decap admin window.

export async function onRequest(context) {
  const { request, env } = context;
  const url = new URL(request.url);
  const code = url.searchParams.get('code');

  if (!code) {
    return new Response('Missing "code" query parameter.', { status: 400 });
  }

  if (!env.GITHUB_OAUTH_ID || !env.GITHUB_OAUTH_SECRET) {
    return new Response(
      'Missing GITHUB_OAUTH_ID / GITHUB_OAUTH_SECRET environment variables.',
      { status: 500 },
    );
  }

  const tokenRes = await fetch('https://github.com/login/oauth/access_token', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Accept: 'application/json',
      'User-Agent': 'decap-cms-cloudflare-oauth',
    },
    body: JSON.stringify({
      client_id: env.GITHUB_OAUTH_ID,
      client_secret: env.GITHUB_OAUTH_SECRET,
      code,
    }),
  });

  const data = await tokenRes.json();

  const status = data.error ? 'error' : 'success';
  const content = data.error
    ? { error: data.error_description || data.error }
    : { token: data.access_token, provider: 'github' };

  // Decap expects the popup to postMessage the result back to the opener window.
  const payload = JSON.stringify(content).replace(/</g, '\\u003c');
  const page = `<!doctype html><html><head><meta charset="utf-8"></head><body>
<script>
(function () {
  function receiveMessage(e) {
    window.opener.postMessage(
      'authorization:github:${status}:${payload}',
      e.origin
    );
    window.removeEventListener('message', receiveMessage, false);
  }
  window.addEventListener('message', receiveMessage, false);
  window.opener.postMessage('authorizing:github', '*');
})();
</script>
<p>Authorizing… you can close this window.</p>
</body></html>`;

  return new Response(page, {
    headers: { 'Content-Type': 'text/html; charset=utf-8' },
  });
}
