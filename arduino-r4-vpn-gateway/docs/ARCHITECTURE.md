# Architecture

The firmware is three small, non-blocking modules driven from `loop()`. None of
them block longer than a single socket poll, so the LED animations stay smooth
even while the tunnel is doing work.

```
                 ┌──────────────────────────────────────────────┐
                 │                  loop()                       │
                 └───────────────┬──────────────┬───────────────┘
                                 │              │
                  gateway.update()        dashboard.update()   status.handleClient()
                                 │              │                       │
                 ┌───────────────▼───┐  ┌───────▼────────┐   ┌──────────▼─────────┐
                 │   VpnGateway      │  │  LedDashboard  │   │   StatusServer     │
                 │  WiFi + WireGuard │  │  12x8 matrix   │   │  HTTP / JSON page  │
                 │  state machine    │  │  animations    │   │  (read-only)       │
                 └───────┬───────────┘  └────────────────┘   └────────────────────┘
                         │ state + metrics (shared snapshot)
                         ▼
            VpnState ∈ { Boot, WifiConnecting, Handshaking,
                         Online, Stalled, WifiLost, Error }
```

## State machine

`VpnGateway` is the brain. It exposes a `VpnState` and a `VpnMetrics` snapshot
that the dashboard and web server read each tick.

```
 Boot ──► WifiConnecting ──► Handshaking ──► Online
              ▲   │                            │  ▲
              │   └────────► WifiLost ◄────────┘  │
              └───────────────(retry)             │
                                 Stalled ─────────┘  (handshake went quiet,
                                                      recovers on next handshake)
   Error  ◄── invalid config / tunnel begin() failed (latched)
```

* **Boot** – objects constructed, banner shown.
* **WifiConnecting** – `WiFi.begin()` kicked; polled non-blocking. In
  simulation mode, if no WiFi associates within 15 s the firmware presses on so
  the LED demo still runs on a bare board.
* **Handshaking** – WiFi is up; the WireGuard handshake is in flight.
* **Online** – peer is up and the last handshake is recent. Metrics flow.
* **Stalled** – we were Online but the peer hasn't handshaken lately
  (real mode only). Recovers automatically.
* **WifiLost** – dropped off WiFi; backs off ~5 s and retries the whole chain.
* **Error** – latched on fatal config problems (e.g. unedited WireGuard keys).

## The tunnel seam

The tunnel lives behind a compile-time seam in `VpnGateway.cpp`:

* `VPN_SIMULATION` defined → a built-in simulator fabricates handshakes and
  bursty, smoothed traffic so every downstream consumer (LED + web) works with
  zero infrastructure. Great for first flash and for demos.
* `VPN_SIMULATION` undefined → the real path calls a `WireGuard` class
  (`begin / is_peer_up / tx_bytes / rx_bytes / last_handshake_ago_ms`). If no
  such library is installed the build stops with a clear `#error`.

This keeps the rest of the codebase identical in both modes — the dashboard
never knows whether the bytes are real.

## Metrics & rate smoothing

`sampleRates()` recomputes throughput twice a second from byte deltas and
applies exponential smoothing (`rate = (3*rate + instant) / 4`). That's what
makes the LED marquee "breathe" with traffic instead of strobing, and what the
web page graphs.

## Why this shape

* **Non-blocking everywhere** so one slow path never freezes the others.
* **Single shared snapshot** (`VpnState` + `VpnMetrics`) instead of callbacks —
  trivial to reason about on a single-core MCU.
* **Compile-time backend selection** keeps the binary lean: you never pay for
  the simulator in a live build, or for WireGuard in a demo build.

See [`../README.md`](../README.md) for scope and the honest limitations of
running a VPN on this class of hardware.
