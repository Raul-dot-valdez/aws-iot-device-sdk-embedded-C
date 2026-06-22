# How to use it — the simple version

This guide is for a normal user. No deep networking knowledge needed.

## What is this, in one sentence?

A tiny Arduino box that keeps a **secure (VPN) connection** to your own server
and shows you — on its little light panel and a web page — whether that secure
connection is up and how busy it is.

## The two parts

```
   YOUR ARDUINO BOX                    YOUR SERVER
   (at home, on USB power)             (a cheap cloud VPS, or a Linux PC)
   ┌──────────────────┐    secure      ┌──────────────────────┐
   │  UNO R4 WiFi      │   encrypted    │  WireGuard server     │
   │  + status light   │═══tunnel══════▶│  (you set this up     │
   │                   │                │   once)               │
   └──────────────────┘                └──────────────────────┘
        ▲
        │ shows status on the LED panel
        │ and on a web page (open it on your phone)
```

* **The server** is set up **once**. It's the "other end" of the tunnel.
* **The box** is the Arduino. It dials the server and holds the secure link.

## Setting it up (you do this once)

1. **Start the server.** On a cloud server or a spare Linux machine, run the
   included script:
   ```
   sudo docs/server-setup/setup-wireguard-server.sh
   ```
   When it finishes it prints a few lines for you to copy.

2. **Paste those lines into one file: `src/config.h`.** That's the only file you
   ever edit. Put in your WiFi name/password and the keys the script printed.

3. **Plug the Arduino into your laptop** and click **Upload** in the Arduino IDE
   (see the main README for the click-by-click).

4. **Unplug from the laptop, plug into any USB charger.** Done. It runs by
   itself from now on.

> Want to try the box first with **nothing** set up? Leave the one line
> `#define VPN_SIMULATION` switched on, upload, and the lights + web page come
> alive in a pretend mode so you can see it working. Turn that line off later
> for the real tunnel.

## Using it every day

There's nothing to do. Plug it into power and:

* **Glance at the light panel** to know the status (see the table below).
* **Open the web page** on your phone — go to the box's address (your router
  shows it, or it's printed when connected). You'll see a clean status screen:
  connected or not, signal strength, and live up/down speed.

That's it. It reconnects on its own if WiFi or power blips.

## Reading the light

| What you see | What it means |
|---|---|
| Sweeping line | looking for WiFi |
| Pulsing square | making the secure connection |
| **Padlock + moving dots along the bottom** | **connected & secure** (dots move faster when busier) |
| Padlock blinking | connected but quiet |
| WiFi symbol with a slash | lost WiFi, retrying |
| Big X | something's wrong — check your settings |
| Blinking corners | "elevated" — unusual activity on the status page |
| Flashing exclamation **!** | "alert" — the box thinks it's being probed |

## Changing settings later — no code, just `config.h`

Everything you might want to change lives in **one file, `src/config.h`**, as
clearly labelled switches. Change a value, re-upload, done — you never touch the
program logic. The common ones:

| Setting in `config.h` | What it does |
|---|---|
| `WIFI_SSID`, `WIFI_PASSWORD` | which WiFi the box joins |
| `WIFI_USE_DHCP` / `WIFI_STATIC_IP` | let the router pick an address, or fix one |
| `WG_ENDPOINT_HOST`, `WG_ENDPOINT_PORT` | where your server is |
| `WG_PRIVATE_KEY`, `WG_PEER_PUBLIC_KEY` | the secure keys (from the script) |
| `WG_LOCAL_IP` | the box's address inside the tunnel |
| `WG_KEEPALIVE_SECONDS` | how often it pings to stay connected |
| `STATUS_SERVER_ENABLED`, `STATUS_SERVER_PORT` | turn the web page on/off, change its port |
| `LED_DASHBOARD_ENABLED` | turn the light panel on/off |
| `LED_BANNER_TEXT`, `LED_BANNER_MS` | the start-up scrolling message |
| `LED_FPS` | how smooth the animations are |
| `WIFI_CONNECT_TIMEOUT_MS` | how long to wait for WiFi before retrying |
| `WIFI_RETRY_BACKOFF_MS` | how long to pause before reconnecting |
| `THREAT_DETECTION_ENABLED` | the "being probed" tripwire on/off, and its sensitivity |
| `VPN_SIMULATION` | demo mode on/off (no server needed when on) |

Because every knob is a setting and not a code change, upgrading to a new
version of the firmware later won't lose your tweaks — they're all in this one
file.

## Common questions

* **Does it need a computer running?** No. After the one-time upload, any USB
  charger powers it.
* **Does it need an operating system / RTOS on the board?** No. It runs
  bare-metal on the Arduino — just plug in and go.
* **Is my password safe?** Keep `config.h` private (see the README note on
  moving secrets into a `secrets.h` that git ignores). Don't share the file.
* **Where do I see the web page?** On any device on the same WiFi, open the
  box's IP address in a browser.

For the deeper "how it works" and the honest limits of a VPN on this size of
chip, see [`../README.md`](../README.md) and
[`ARCHITECTURE.md`](ARCHITECTURE.md).
