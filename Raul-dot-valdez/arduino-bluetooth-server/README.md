# Arduino UNO R4 WiFi - Bluetooth (BLE) Server

Firmware that turns an **Arduino UNO R4 WiFi** into a Bluetooth Low Energy
**server (peripheral)** that any mobile phone can connect to. The phone sends
text/commands to the board; the board echoes status back over BLE and scrolls
received messages across the built-in **12x8 LED dot-matrix**.

## Hardware

- Arduino UNO R4 WiFi (Renesas RA4M1 MCU + ESP32-S3 radio module)
- On-board 12x8 LED matrix (no extra wiring required)

The UNO R4 WiFi's ESP32-S3 provides the Bluetooth radio. Plain BLE only —
the UNO R4 does not support classic Bluetooth SPP pairing, so use a **BLE**
terminal app on the phone.

## How it works

The sketch advertises a Nordic UART-style BLE service:

| Role              | UUID                                   | Properties           |
|-------------------|----------------------------------------|----------------------|
| Service           | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | -                    |
| RX (phone→board)  | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Write / WriteNoResp  |
| TX (board→phone)  | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Read / Notify        |

- Advertised name: **`UNO-R4-Server`**
- On connect: matrix shows a heart icon and the board notifies a greeting.
- On phone write: text scrolls across the matrix and the board replies `ACK: ...`.
- On disconnect: matrix returns to the idle "wait" icon.

## Build & flash

1. Install the **Arduino IDE 2.x** (or `arduino-cli`).
2. In *Boards Manager*, install **"Arduino UNO R4 Boards"**.
3. In *Library Manager*, install **`ArduinoBLE`**. (`Arduino_LED_Matrix` ships
   with the UNO R4 board package.)
4. Open `arduino-bluetooth-server.ino`, select board **Arduino UNO R4 WiFi**
   and the correct port, then **Upload**.

With `arduino-cli`:

```bash
arduino-cli core install arduino:renesas_uno
arduino-cli lib install ArduinoBLE
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi arduino-bluetooth-server
arduino-cli upload  --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 arduino-bluetooth-server
```

## Connect from a phone

1. Install a BLE terminal app: **nRF Connect** (Android/iOS), **LightBlue**, or
   **Serial Bluetooth Terminal** (BLE mode).
2. Scan and connect to **`UNO-R4-Server`**.
3. Subscribe to notifications on the **TX** characteristic to see board output.
4. Write text to the **RX** characteristic; watch it scroll on the matrix and
   receive an `ACK` notification back.

The USB serial monitor (115200 baud) mirrors all activity for debugging.
