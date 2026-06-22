# 🔐 Arduino UNO R4 WiFi — VPN Gateway

Turn an **Arduino UNO R4 WiFi** into a [WireGuard](https://www.wireguard.com/)
tunnel endpoint for your home network, and use its built-in **12×8 LED matrix**
as a live, glanceable VPN status display — padlock when secure, a marching
data-flow marquee whose speed tracks real throughput, distinct animations for
every state. A tiny built-in web page gives you the numbers.

> **Flash it in 60 seconds with zero infrastructure.** Ships in a *simulation
> mode* that animates the whole dashboard and status page without a WireGuard
> server, so you can prove your hardware works before touching keys or a VPS.

```
   WifiConnecting        Handshaking            Online (busy)
   · · · █ · · · · ·      · · · · · · · ·        · · ███ · · · ·   ← padlock
   · · · █ · · · · ·      · · · ███ · · ·        · · █ · █ · · ·
   · · · █ · · · · ·      · · █ · · █ · ·        · ███████ · · ·
   · · · █ · · · · ·  →   · · █ · · █ · ·   →    · ███████ · · ·
   · · · █ · · · · ·      · · · ███ · · ·        · █ ███ █ · · ·
   (scan line sweeps)     (pulse expands)       █ · █ · █ · █ ·   ← data marquee
```

---

## Why this is interesting (and the honest version)

A "VPN on a microcontroller" sounds impossible — and a full router-grade VPN
*is* out of scope for a 48 MHz Cortex-M4. But WireGuard was designed to be
small, and the embedded community has done remarkable work shrinking it. This
project stands on that work and adds a genuinely useful, beautiful **status
head-end**:

* **What it does well:** acts as a single-peer WireGuard *gateway endpoint* —
  a dedicated, always-on, low-power box that holds an encrypted tunnel to your
  WireGuard server/VPS and surfaces its health on the LED matrix and a web
  page. Think "a physical, ambient VPN indicator + tunnel keeper" you can leave
  plugged into a USB charger in the hallway.
* **What it is *not*:** a gigabit router, a multi-client VPN concentrator, or a
  drop-in replacement for a Raspberry Pi running `wg-quick`. Throughput on this
  class of MCU is modest (think single-digit Mbit/s at best), and routing your
  *entire* LAN's traffic through it is not the goal. See
  [Scope & limitations](#-scope--limitations).

The design is deliberately layered so the *creative, always-working* part (the
dashboard, the web UI, the state machine) is decoupled from the *hardware-
dependent* part (the actual crypto tunnel) — which is exactly why simulation
mode can give you the full experience instantly.

## Open-source shoulders we stand on

This project is glue + UX on top of excellent existing work — exactly the
"explore hundreds of open-source codes" spirit:

| Building block | Role here |
|---|---|
| [WireGuard](https://www.wireguard.com/) protocol (Noise / Curve25519 / ChaCha20-Poly1305) | the tunnel itself |
| [WireGuard-ESP32-Arduino](https://github.com/ciniml/WireGuard-ESP32-Arduino) (Kenta Ida) | reference `WireGuard` Arduino class our live path is written against |
| [`Arduino_LED_Matrix`](https://docs.arduino.cc/tutorials/uno-r4-wifi/led-matrix/) (R4 core) | drives the 12×8 panel |
| [`ArduinoGraphics`](https://github.com/arduino-libraries/ArduinoGraphics) | scrolling boot banner |
| [`WiFiS3`](https://github.com/arduino/ArduinoCore-renesas) (R4 core) | WiFi station + HTTP server |
| [`wg-quick` / `wireguard-tools`](https://git.zx2c4.com/wireguard-tools/) | the server side (`docs/server-setup/`) |

## Repository layout

```
arduino-r4-vpn-gateway/
├── arduino-r4-vpn-gateway.ino   # main sketch (Arduino IDE entry point)
├── platformio.ini               # PlatformIO build (UNO R4 WiFi)
├── src/
│   ├── config.h                 # ← edit this: WiFi + WireGuard + options
│   ├── VpnGateway.{h,cpp}        # WiFi + tunnel state machine (+ simulator)
│   ├── LedDashboard.{h,cpp}      # 12×8 matrix animations
│   └── StatusServer.{h,cpp}      # read-only HTTP status page + JSON
├── examples/
│   └── led_patterns_demo/        # preview every animation, no WiFi needed
└── docs/
    ├── ARCHITECTURE.md           # how the pieces fit
    ├── WIRING.md                 # (spoiler: there's no wiring)
    └── server-setup/             # WireGuard server config + bootstrap script
```

---

## Quick start — simulation mode (no server needed)

1. **Install the board core.** Arduino IDE → Boards Manager → *Arduino UNO R4
   Boards*. (PlatformIO users: it's in `platformio.ini`.)
2. **Install ArduinoGraphics** (Library Manager) for the scrolling banner.
3. **Open** `arduino-r4-vpn-gateway.ino`. Leave `#define VPN_SIMULATION` as-is
   in `src/config.h`.
4. *(Optional)* put your real WiFi SSID/password in `config.h` so the status
   web page is reachable. Without it, the LED demo still runs.
5. **Select** board *Arduino UNO R4 WiFi*, **Upload**.
6. Watch the LED matrix: banner → scan line → pulse → padlock with a marching
   marquee. Open the Serial Monitor (115200) to see the state log and the LAN
   URL. Browse to `http://<board-ip>/` for the dashboard.

Just want to preview the animations? Open
`examples/led_patterns_demo/led_patterns_demo.ino` — it cycles through every
state with no WiFi or config at all.

## Live tunnel mode

1. **Stand up a WireGuard server.** On a VPS or always-on Linux box:
   ```bash
   sudo docs/server-setup/setup-wireguard-server.sh
   ```
   It installs WireGuard, generates both key pairs, writes `/etc/wireguard/
   wg0.conf`, enables forwarding, brings the tunnel up, and prints the exact
   values to paste into `config.h`. (Prefer to do it by hand? See
   `docs/server-setup/wg-server.conf.example`.)
2. **Fill in `src/config.h`:** `WG_PRIVATE_KEY`, `WG_PEER_PUBLIC_KEY`,
   `WG_ENDPOINT_HOST`, `WG_ENDPOINT_PORT`, `WG_LOCAL_IP`, and your WiFi creds.
3. **Comment out `#define VPN_SIMULATION`.**
4. **Add a WireGuard library** that provides a `WireGuard` class with
   `begin()/is_peer_up()/tx_bytes()/rx_bytes()/last_handshake_ago_ms()`
   (e.g. [WireGuard-ESP32-Arduino](https://github.com/ciniml/WireGuard-ESP32-Arduino)
   on ESP32-class targets). If it's missing, the build stops with a helpful
   message instead of failing cryptically.
5. **Upload.** The matrix shows the handshake pulse, then the padlock once the
   peer is up. Verify on the server with `sudo wg show` — you should see a
   recent handshake and growing transfer counters for the Arduino peer.

> **A note on targets.** The live tunnel needs a WireGuard implementation
> exposed to the sketch. That stack is most mature on ESP32-class boards today;
> on the R4, the firmware drives the **dashboard, state machine, and web UI**
> exactly as written, and the tunnel binds to whatever WireGuard library you
> provide for your build target. The seam in `VpnGateway.cpp` is the single
> place that touches the tunnel, so porting is contained to one file.

---

## The LED dashboard

| Animation | State | Meaning |
|---|---|---|
| Scrolling `R4 VPN` | boot | firmware starting |
| Vertical scan line sweeping | `wifi-connecting` | joining WiFi |
| Square pulse from centre | `handshaking` | WireGuard handshake in flight |
| **Padlock + marching bottom row** | `online` | secure; marquee speed = throughput |
| Padlock blinking | `stalled` | tunnel quiet, will recover |
| WiFi glyph + slash, blinking | `wifi-lost` | offline, retrying |
| Big X | `error` | bad config/keys (see serial) |

The marquee isn't decorative: its step interval shrinks from ~220 ms (idle)
toward ~40 ms as smoothed throughput rises, so a busy tunnel visibly *races*.

## The status page

`http://<board-ip>/` serves a self-contained, auto-refreshing dashboard
(state dot, LAN/tunnel IPs, RSSI, handshakes, live down/up rates, totals,
uptime, last-handshake age). Machine-readable JSON is at
`http://<board-ip>/api/status` — handy for Home Assistant, Grafana, or a
`curl` in a cron job.

```json
{ "state":"online", "lan_ip":"192.168.1.50", "tunnel_ip":"10.6.0.2",
  "rssi_dbm":-58, "handshakes":3, "last_handshake_ms":21000,
  "tx_bytes":148213, "rx_bytes":402991, "tx_rate_bps":900,
  "rx_rate_bps":1640, "uptime_ms":372000 }
```

---

## 🔒 Keeping your keys out of git

`config.h` holds secrets for convenience. Before committing your own copy:

* move secrets into a `secrets.h` and `#include` it from `config.h`, then add
  `secrets.h` to `.gitignore` (a starter `.gitignore` is included), **or**
* keep `config.h` untracked locally (`git update-index --skip-worktree`).

Never commit a real `WG_PRIVATE_KEY` or WiFi password. The values shipped here
are obvious placeholders.

## 📏 Scope & limitations

This is an honest list, not fine print:

* **Single peer, modest throughput.** One tunnel to one server; expect
  single-digit Mbit/s at best. Don't expect to stream 4K through it.
* **Endpoint, not a whole-LAN router.** It keeps and displays a tunnel; it does
  not do kernel-grade NAT/routing for every device on your network. For
  road-warrior style "this gadget is on my home VPN," it's a great fit.
* **2.4 GHz WiFi only**, by the R4 radio's nature.
* **Live tunnel needs a WireGuard library for your target** (see above). The
  dashboard/web/state-machine layers are fully implemented and run today; the
  crypto is delegated to vetted libraries rather than hand-rolled — *don't roll
  your own crypto.*
* **The status page is read-only and unauthenticated.** Keep it on your trusted
  LAN; don't port-forward it to the internet.

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Big **X** on the matrix | unedited WireGuard keys in `config.h` (real mode) — paste real keys |
| Stuck on the **scan line** | WiFi creds wrong, or 5 GHz-only SSID — the R4 is 2.4 GHz |
| Padlock never appears (real mode) | server unreachable: check `WG_ENDPOINT_HOST/PORT`, UDP 51820 forwarding, `sudo wg show` |
| Web page unreachable | grab the IP from serial; ensure you're on the same LAN; set a static DHCP lease |
| Builds fail on `#error ... WireGuard` | you turned off simulation without adding a WireGuard library — add one or re-enable `VPN_SIMULATION` |

## License

MIT — see [`LICENSE`](LICENSE). Third-party components keep their own licenses;
see [`NOTICE`](NOTICE).
