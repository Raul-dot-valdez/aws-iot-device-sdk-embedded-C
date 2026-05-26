# Arduino UNO R4 WiFi - WiFi HTTP Server

Firmware that turns an **Arduino UNO R4 WiFi** into a WiFi **HTTP server**.
The board joins your existing WiFi network (station mode) and serves a control
page on port 80. Any phone on the same network just opens the board's IP in a
browser — **no app required** — and can scroll text across the built-in
**12x8 LED dot-matrix** or show status icons.

## Hardware

- Arduino UNO R4 WiFi (Renesas RA4M1 MCU + ESP32-S3 radio module)
- On-board 12x8 LED matrix (no extra wiring required)

## How it works

1. On boot the board connects to the WiFi network in `arduino_secrets.h`.
2. It prints its IP address to the USB serial monitor (115200 baud) and shows
   a heart icon on the matrix once connected.
3. The phone (on the same network) browses to `http://<board-ip>/`.
4. The page offers a text field — submitted text scrolls across the matrix —
   plus quick buttons to show heart / check icons.

Requests are plain HTTP `GET`s, e.g. `GET /?msg=hello` or `GET /?icon=heart`,
so you can also drive it from `curl` or any HTTP client.

## Configure

Copy the template, then fill in your network credentials. The real
`arduino_secrets.h` is git-ignored so your password is never committed:

```bash
cp arduino_secrets.h.example arduino_secrets.h
```

Edit `arduino_secrets.h`:

```c
#define SECRET_SSID "your-wifi-ssid"
#define SECRET_PASS "your-wifi-password"
```

Both the Arduino and the phone must be on the **same** WiFi network. Note the
UNO R4 WiFi radio supports **2.4 GHz** networks only.

## Build & flash

1. Install the **Arduino IDE 2.x** (or `arduino-cli`).
2. In *Boards Manager*, install **"Arduino UNO R4 Boards"**. This bundles the
   `WiFiS3` and `Arduino_LED_Matrix` libraries — no extra installs needed.
3. Open `arduino-wifi-server.ino`, select board **Arduino UNO R4 WiFi** and the
   correct port, then **Upload**.

With `arduino-cli`:

```bash
arduino-cli core install arduino:renesas_uno
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi arduino-wifi-server
arduino-cli upload  --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 arduino-wifi-server
```

## Use from a phone

1. Open the serial monitor after upload to read the board's IP address.
2. On your phone's browser, go to `http://<board-ip>/`.
3. Type a message and tap **Scroll on LED matrix**, or use the icon buttons.
