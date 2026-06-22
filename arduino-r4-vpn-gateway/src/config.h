/*
 * config.h  —  User configuration for the Arduino UNO R4 WiFi VPN Gateway
 * ---------------------------------------------------------------------------
 * Copy nothing, edit everything: every value below is meant to be changed by
 * you before flashing.  Secrets (WiFi password, WireGuard private key) live
 * here only for convenience while you get started.  See README.md → "Keeping
 * your keys out of git" for how to move them into a secrets.h that is ignored
 * by version control.
 *
 * Nothing in this file talks to the hardware directly; it is pure data that
 * the rest of the firmware reads at boot.
 * ---------------------------------------------------------------------------
 */
#ifndef VPN_GATEWAY_CONFIG_H
#define VPN_GATEWAY_CONFIG_H

/* =========================================================================
 * 1. RUN MODE
 * =========================================================================
 * VPN_SIMULATION lets you flash the board and watch the whole LED dashboard
 * + status web page come alive WITHOUT a WireGuard server or the WireGuard
 * library installed.  The tunnel is faked with believable, animated metrics.
 *
 *   - Leave it defined for your first flash to prove the hardware works.
 *   - Comment it out to drive a real WireGuard tunnel (requires the
 *     WireGuard library, see README → Dependencies).
 * ------------------------------------------------------------------------- */
#define VPN_SIMULATION

/* =========================================================================
 * 2. WIFI (station mode — the gateway joins your home WiFi)
 * ========================================================================= */
#define WIFI_SSID         "your-home-ssid"
#define WIFI_PASSWORD     "your-home-password"

/* Static IP for the gateway on your LAN.  Leave WIFI_USE_DHCP defined to let
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
 * These come from your WireGuard server.  Generate a key pair for the board
 * with:  wg genkey | tee private.key | wg pubkey > public.key
 * Put the board's PRIVATE key here and add the board's PUBLIC key as a [Peer]
 * on the server (see docs/server-setup/).
 * ------------------------------------------------------------------------- */

/* The address this device claims INSIDE the tunnel (the WireGuard subnet). */
#define WG_LOCAL_IP       "10.6.0.2"

/* The board's own private key (base64, 44 chars ending in '='). */
#define WG_PRIVATE_KEY    "PASTE_DEVICE_PRIVATE_KEY_HERE="

/* The server's public key (base64). */
#define WG_PEER_PUBLIC_KEY "PASTE_SERVER_PUBLIC_KEY_HERE="

/* Optional pre-shared key for an extra symmetric layer.  Leave "" to skip. */
#define WG_PRESHARED_KEY  ""

/* Public endpoint of your WireGuard server: a hostname or IP, and the UDP
 * port it listens on (51820 by convention). */
#define WG_ENDPOINT_HOST  "vpn.example.com"
#define WG_ENDPOINT_PORT  51820

/* Seconds between keepalive packets.  25 keeps NAT mappings on home routers
 * alive; set 0 to disable. */
#define WG_KEEPALIVE_SECONDS 25

/* =========================================================================
 * 4. STATUS WEB SERVER
 * ========================================================================= */
#define STATUS_SERVER_ENABLED   1
#define STATUS_SERVER_PORT      80

/* =========================================================================
 * 5. LED MATRIX DASHBOARD (12x8 on the UNO R4 WiFi)
 * ========================================================================= */
/* Master brightness gate for the throughput animation (0 = off, 1 = on).
 * The matrix is monochrome, so this is on/off rather than a PWM level. */
#define LED_DASHBOARD_ENABLED   1

/* How long (ms) the boot banner scrolls before the state machine takes over. */
#define LED_BANNER_MS           2000

/* Scrolling banner text shown at boot. */
#define LED_BANNER_TEXT         " R4 VPN "

#endif /* VPN_GATEWAY_CONFIG_H */
