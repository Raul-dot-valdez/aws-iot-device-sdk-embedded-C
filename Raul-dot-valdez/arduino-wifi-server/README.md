# Arduino UNO R4 WiFi - WiFi WebSocket Server

Firmware that turns an **Arduino UNO R4 WiFi** into a combined WiFi
**HTTP + WebSocket server**. The board joins your existing WiFi network
(station mode) and serves a control page on port 80. The page opens a
**WebSocket** back to the board for a live, low-latency, two-way link — any
phone or iPad on the same network just opens the board's IP in a browser
(**no app required**) to scroll text across the built-in **12x8 LED
dot-matrix**, show status icons, and watch the board push messages back in
real time.

The WebSocket layer (RFC 6455 handshake + framing, including SHA-1 and
Base64) is implemented in the sketch itself — **no external WebSocket
library is needed**, since the UNO R4's `WiFiS3` stack has no drop-in option.

## Hardware

- Arduino UNO R4 WiFi (Renesas RA4M1 MCU + ESP32-S3 radio module)
- On-board 12x8 LED matrix (no extra wiring required)

## How it works

1. On boot the board connects to the WiFi network in `arduino_secrets.h`.
2. It prints its IP address to the USB serial monitor (115200 baud) and shows
   a heart icon on the matrix once connected.
3. The phone/iPad (on the same network) browses to `http://<board-ip>/`.
4. The served page opens `ws://<board-ip>/` and gives you a text field, icon
   buttons, and a live log of messages from the board.

### WebSocket protocol

The board speaks plain text frames over the WebSocket:

| Direction      | Message            | Effect                                   |
|----------------|--------------------|------------------------------------------|
| browser→board  | `any text`         | Scrolls the text across the LED matrix   |
| browser→board  | `icon:heart`       | Shows the heart icon                     |
| browser→board  | `icon:check`       | Shows the check icon                     |
| board→browser  | `ACK scroll: ...`  | Acknowledges a scroll request            |
| board→browser  | `ACK icon: ...`    | Acknowledges an icon request             |
| board→browser  | `uptime: <n>s`     | Heartbeat pushed every 5 seconds         |

Any standard WebSocket client works too, e.g. from a laptop:

```js
const ws = new WebSocket('ws://<board-ip>/');
ws.onmessage = e => console.log(e.data);
ws.onopen = () => ws.send('hello from anywhere');
```

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

## Use from a phone or iPad

1. Open the serial monitor after upload to read the board's IP address.
2. On the browser, go to `http://<board-ip>/` — the page connects the
   WebSocket automatically (the log shows `* connected`).
3. Type a message and tap **Send** (or press Enter), or use the icon buttons.
   Watch the LED matrix react and the board's ACK / uptime messages appear in
   the log.
