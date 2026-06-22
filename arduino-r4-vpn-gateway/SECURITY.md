# Security

This project handles a VPN tunnel and credentials, so it is built around
current (2026) security practices. This document is the threat model, the
hardening you get out of the box, and the checklist for deploying it safely.

## Threat model — what this does and does not protect

**Protects:** the confidentiality and integrity of traffic *inside* the
WireGuard tunnel between the board and your server, using WireGuard's modern
cryptography (Curve25519 key exchange, ChaCha20-Poly1305 AEAD, BLAKE2s). The
tunnel is mutually authenticated by public keys and silent to unauthenticated
packets.

**Does not protect against:**
- A physically stolen board. Whoever holds it can read its flash and recover
  the device private key. Treat the board as a single credential and *rotate
  its key if it leaves your control* (see Key management).
- A compromised WiFi network upstream of the board (the tunnel still protects
  the payload, but availability/metadata are exposed).
- The status web page being exposed to a hostile network — keep it on the LAN.

This is a **single-peer gateway endpoint**, not a multi-tenant VPN concentrator.

## What's hardened out of the box

**Secrets stay out of git.** `config.h` ships only placeholders. Real secrets
go in a git-ignored `secrets.h` (`src/secrets.h.example` is the template), which
`config.h` includes automatically. `.gitignore` blocks `secrets.h` and `*.key`,
and CI fails if a real key or a `secrets.h` is ever committed.

**Fail-closed configuration.** In live mode the firmware refuses to start the
tunnel (latches an Error state shown as a big X on the LED) if the WireGuard
keys are still placeholders, so a misconfigured device never runs "open."

**Status server is least-exposure by design:**
- Read-only — no control surface, no way to change the tunnel over HTTP.
- Response headers: `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`,
  `Referrer-Policy: no-referrer`, `Cache-Control: no-store`, and a strict
  `Content-Security-Policy` (`default-src 'none'`, inline style/script only,
  `connect-src 'self'`).
- **No wildcard CORS** — the page is same-origin, so no `Access-Control-Allow-Origin: *`.
- Optional bearer-token gate (`STATUS_SERVER_TOKEN`): when set, every request
  must present the token via `Authorization: Bearer <token>` or `?token=<token>`,
  and the server fails closed (HTTP 401) otherwise.
- Request parsing is bounded (1 s deadline, 256-byte line cap, 40-header cap,
  fixed buffers) so a slow or malformed client can't tie up the single loop.

**Supply chain.** The project's CI workflow runs with least-privilege
permissions (`contents: read`), pins the `actions/checkout` action to a commit
SHA, disables credential persistence, and includes a secret-scan step. See
`.github/workflows/arduino-r4-vpn-gateway.yml`.

## Secure deployment checklist

On the **board / repo**:
- [ ] Put secrets in `src/secrets.h` (copied from `secrets.h.example`), never in `config.h`.
- [ ] Confirm `git status` shows no `secrets.h` and no `*.key` staged.
- [ ] Give the board a static DHCP lease; keep the status page on the LAN only — never port-forward it.
- [ ] On a shared/untrusted LAN, set `STATUS_SERVER_TOKEN` (a long random string).
- [ ] Consider setting `WG_PRESHARED_KEY` for an extra symmetric layer (requires a PSK-capable WireGuard binding).

On the **server**:
- [ ] Run `docs/server-setup/harden-server.sh` (firewall, fail2ban, automatic security updates).
- [ ] SSH with keys only; disable password authentication.
- [ ] Lock the peer's `AllowedIPs` to a single `/32` (the script already does this).
- [ ] Keep `wireguard` and the OS patched; rotate keys periodically.

## Key management

- The device key pair is the board's identity. Generate it on a trusted machine,
  store the private key only in `secrets.h`, and add only the **public** key to
  the server.
- **Rotate** (generate a new pair, update `secrets.h` and the server `[Peer]`)
  if the board is lost, sold, RMA'd, or flashed by anyone you don't trust.
- Prefer per-device keys; never reuse one key across multiple boards.

## Reporting a vulnerability

This is a hobbyist/educational project. If you find a security issue, please
open a private report to the repository owner rather than a public issue, and
allow reasonable time to fix before disclosure. Do not include real keys or
passwords in any report.
