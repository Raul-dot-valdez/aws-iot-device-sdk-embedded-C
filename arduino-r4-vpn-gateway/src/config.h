/*
 * config.h  —  User configuration for the Arduino UNO R4 WiFi VPN Gateway
 * ---------------------------------------------------------------------------
 * One file to rule them all: every user-tunable value lives here, so day-to-day
 * use and future tuning never require touching the firmware logic.
 *
 * SECURITY (2026): secrets do NOT belong in version control. This file ships
 * with obvious placeholders. The recommended workflow is to put your real
 * secrets in a sibling `secrets.h` (git-ignored) — if present it is included
 * automatically and overrides the placeholders below. See SECURITY.md.
 * ---------------------------------------------------------------------------
 */
#ifndef VPN_GATEWAY_CONFIG_H
#define VPN_GATEWAY_CONFIG_H

/* Pull in a local, git-ignored secrets.h if the user created one. Anything it
 * #defines wins, because every secret below is guarded with #ifndef. */
#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

/* =========================================================================
 * 1. RUN MODE
 * =========================================================================
 * VPN_SIMULATION lets you flash the board and watch the whole LED dashboard
 * + status web page come alive WITHOUT a WireGuard server or the WireGuard
 * library installed. The tunnel is faked with believable, animated metrics.
 *
 *   - Leave it defined for your first flash to prove the hardware works.
 *   - Comment it out to drive a real WireGuard tunnel.
 *
 * SECURITY: simulation mode serves the status page with no real data and no
 * tunnel; it is safe for demos but is not a security boundary. Turn it off for
 * real use.
 * ------------------------------------------------------------------------- */
#define VPN_SIMULATION

/* =========================================================================
 * 2. WIFI (station mode — the gateway joins your home WiFi)
 * ========================================================================= */
#ifndef WIFI_SSID
#define WIFI_SSID         "your-home-ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD     "your-home-password"
#endif

/* Static IP for the gateway on your LAN. Leave WIFI_USE_DHCP defined to let
 * the router assign one instead; a static lease is recommended so the status
 * page is always reachable at the same address. */
#define WIFI_USE_DHCP
#define WIFI_STATIC_IP    192, 168, 1, 50
#define WIFI_GATEWAY      192, 168, 1, 1
#define WIFI_SUBNET       255, 255, 255, 0
#define WIFI_DNS          1, 1, 1, 1

/* =========================================================================
 * 3. WIREGUARD TUNNEL
 * =========================================================================
 * Generate a key pair for the board with:
 *   wg genkey | tee private.key | wg pubkey > public.key
 * Put the board's PRIVATE key here and add the board's PUBLIC key as a [Peer]
 * on the server (see docs/server-setup/).
 *
 * SECURITY: the private key is the device's identity. Treat it like a password,
 * keep it in secrets.h, and rotate it if the board is lost or reflashed by an
 * untrusted party. See SECURITY.md → Key management.
 * ------------------------------------------------------------------------- */

/* The address this device claims INSIDE the tunnel (the WireGuard subnet). */
#ifndef WG_LOCAL_IP
#define WG_LOCAL_IP       "10.6.0.2"
#endif

/* The board's own private key (base64, 44 chars ending in '='). */
#ifndef WG_PRIVATE_KEY
#define WG_PRIVATE_KEY    "PASTE_DEVICE_PRIVATE_KEY_HERE="
#endif

/* The server's public key (base64). */
#ifndef WG_PEER_PUBLIC_KEY
#define WG_PEER_PUBLIC_KEY "PASTE_SERVER_PUBLIC_KEY_HERE="
#endif

/* Optional pre-shared key (base64) for an extra symmetric layer that hardens
 * the tunnel against future ("harvest now, decrypt later") attacks. Leave ""
 * to skip. Requires a WireGuard binding that accepts a PSK. See SECURITY.md. */
#ifndef WG_PRESHARED_KEY
#define WG_PRESHARED_KEY  ""
#endif

/* Public endpoint of your WireGuard server: a hostname or IP, and the UDP
 * port it listens on (51820 by convention). */
#ifndef WG_ENDPOINT_HOST
#define WG_ENDPOINT_HOST  "vpn.example.com"
#endif
#define WG_ENDPOINT_PORT  51820

/* Seconds between keepalive packets. 25 keeps NAT mappings on home routers
 * alive; set 0 to disable. */
#define WG_KEEPALIVE_SECONDS 25

/* =========================================================================
 * 4. STATUS WEB SERVER
 * =========================================================================
 * Read-only status page on your LAN.
 *
 * SECURITY (2026): keep this on your trusted LAN — never port-forward it to the
 * internet. Responses carry hardening headers (nosniff, DENY framing, a strict
 * CSP, no-store) and CORS is NOT wildcarded. For a shared/untrusted LAN, set a
 * token below to require `Authorization: Bearer <token>` (or `?token=<token>`).
 * ------------------------------------------------------------------------- */
#define STATUS_SERVER_ENABLED   1
#define STATUS_SERVER_PORT      80

/* Optional access token. Empty = open on the LAN (default). When set, every
 * request must present it. Put the real value in secrets.h. */
#ifndef STATUS_SERVER_TOKEN
#define STATUS_SERVER_TOKEN     ""
#endif

/* =========================================================================
 * 5. LED MATRIX DASHBOARD (12x8 on the UNO R4 WiFi)
 * ========================================================================= */
/* Master on/off for the LED dashboard (the matrix is monochrome). */
#define LED_DASHBOARD_ENABLED   1

/* How long (ms) the boot banner scrolls before the state machine takes over. */
#define LED_BANNER_MS           2000

/* Scrolling banner text shown at boot. */
#define LED_BANNER_TEXT         " R4 VPN "

/* Animation refresh rate (frames per second). 30 is plenty for this panel. */
#define LED_FPS                 30

/* =========================================================================
 * 6. TIMING & RETRY BEHAVIOUR (advanced — defaults are sensible)
 * =========================================================================
 * The knobs that used to be buried in the code. They live here so tuning the
 * gateway never requires touching the firmware itself.
 * ------------------------------------------------------------------------- */

/* How long (ms) to wait for WiFi to associate before deciding it failed. */
#define WIFI_CONNECT_TIMEOUT_MS 15000

/* How long (ms) to wait after losing WiFi before retrying the whole chain. */
#define WIFI_RETRY_BACKOFF_MS   5000

#endif /* VPN_GATEWAY_CONFIG_H */
