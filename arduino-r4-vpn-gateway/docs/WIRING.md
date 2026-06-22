# Wiring

Good news: **there is no wiring.** Everything this project uses is built into
the Arduino UNO R4 WiFi.

```
        ┌─────────────────────────────────────────┐
        │            Arduino UNO R4 WiFi           │
        │                                          │
        │   ● ● ● ● ● ● ● ● ● ● ● ●   ← 12x8 LED    │
        │   ● ● ● ● ● ● ● ● ● ● ● ●     matrix      │
        │   ● ● ● ● ● ● ● ● ● ● ● ●    (built in)   │
        │                                          │
        │   [ ESP32-S3 WiFi radio (built in) ]     │
        │                                          │
        │   USB-C ──────────────► power + serial   │
        └─────────────────────────────────────────┘
```

## What you need

| Item | Why |
|------|-----|
| Arduino UNO R4 WiFi | the gateway itself (LED matrix + WiFi on board) |
| USB-C cable | power and flashing/serial |
| 5 V USB power source | a phone charger or USB battery works for headless use |
| 2.4 GHz WiFi network | the R4's radio is 2.4 GHz only |

## Powering it headless

Once flashed, the board does not need a computer. Plug the USB-C into any 5 V
source. The LED matrix tells you what's happening, and the status page is on
your LAN at the board's IP (printed to serial at boot, or check your router's
DHCP table). Give it a static DHCP lease so the address never changes.

## Optional add-ons (not required)

* **Qwiic OLED** (I²C on the Qwiic connector) — if you want a text status
  display in addition to the matrix. Not used by this firmware out of the box.
* **Enclosure** — any UNO-form-factor case with a cutout over the LED matrix.

## Reading the LED matrix

| Animation | Meaning |
|-----------|---------|
| Scan line sweeping left↔right | searching / connecting to WiFi |
| Square pulse from the centre | WireGuard handshake in progress |
| Padlock + marching bottom row | **secure & online** — marquee speed tracks throughput |
| Padlock blinking | tunnel up but quiet (stalled handshake) |
| WiFi glyph with a slash, blinking | WiFi lost, retrying |
| Big X | error — check your config / keys |
