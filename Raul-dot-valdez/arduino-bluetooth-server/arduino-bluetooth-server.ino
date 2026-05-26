/*
 * Arduino UNO R4 WiFi - Bluetooth (BLE) Server
 *
 * Turns the UNO R4 WiFi into a BLE peripheral ("server") that any mobile
 * phone can connect to. The phone writes text/commands to the board and the
 * board echoes status back and scrolls received messages across the built-in
 * 12x8 LED dot-matrix.
 *
 * Hardware:  Arduino UNO R4 WiFi (Renesas RA4M1 + ESP32-S3 radio, on-board
 *            12x8 LED matrix).
 * Libraries: ArduinoBLE, Arduino_LED_Matrix (both bundled with the
 *            UNO R4 board package).
 *
 * BLE layout (Nordic UART-style):
 *   Service           6e400001-b5a3-f393-e0a9-e50e24dcca9e
 *   RX (phone->board) 6e400002-b5a3-f393-e0a9-e50e24dcca9e  Write / WriteNoResp
 *   TX (board->phone) 6e400003-b5a3-f393-e0a9-e50e24dcca9e  Read / Notify
 *
 * Use any BLE terminal app (e.g. nRF Connect, LightBlue, "Serial Bluetooth
 * Terminal") to connect to "UNO-R4-Server", write to RX and subscribe to TX.
 */

#include <ArduinoBLE.h>
#include "Arduino_LED_Matrix.h"

// ---- Nordic UART Service UUIDs ---------------------------------------------
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

static const char* DEVICE_NAME = "UNO-R4-Server";
static const int   RX_BUFFER_SIZE = 128;

BLEService nusService(NUS_SERVICE_UUID);
BLECharacteristic rxCharacteristic(NUS_RX_UUID,
                                   BLEWrite | BLEWriteWithoutResponse,
                                   RX_BUFFER_SIZE);
BLECharacteristic txCharacteristic(NUS_TX_UUID,
                                   BLERead | BLENotify,
                                   RX_BUFFER_SIZE);

ArduinoLEDMatrix matrix;

// 12x8 frames for status icons (each row is a 12-bit value).
const uint8_t FRAME_HEART[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,0,0,0,0,0,1,1,0,0},
  {1,1,1,1,0,0,0,1,1,1,1,0},
  {1,1,1,1,1,0,1,1,1,1,1,0},
  {1,1,1,1,1,1,1,1,1,1,1,0},
  {0,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,1,1,1,1,1,1,1,0,0,0},
  {0,0,0,1,1,1,1,1,0,0,0,0},
};

const uint8_t FRAME_WAIT[8][12] = {
  {0,0,0,0,1,1,1,1,0,0,0,0},
  {0,0,0,0,1,0,0,1,0,0,0,0},
  {0,0,0,0,0,1,1,0,0,0,0,0},
  {0,0,0,0,0,1,1,0,0,0,0,0},
  {0,0,0,0,0,1,1,0,0,0,0,0},
  {0,0,0,0,0,1,1,0,0,0,0,0},
  {0,0,0,0,1,0,0,1,0,0,0,0},
  {0,0,0,0,1,1,1,1,0,0,0,0},
};

void showFrame(const uint8_t frame[8][12]) {
  matrix.renderBitmap(frame, 8, 12);
}

void sendToPhone(const char* msg) {
  txCharacteristic.writeValue((const uint8_t*)msg, strlen(msg));
}

void setup() {
  Serial.begin(115200);
  // Don't block forever if no USB host is attached.
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000)) { }

  matrix.begin();
  showFrame(FRAME_WAIT);

  if (!BLE.begin()) {
    Serial.println("Failed to start BLE!");
    // Flash the matrix to signal a fatal error.
    while (true) {
      matrix.clear();
      delay(200);
      showFrame(FRAME_WAIT);
      delay(200);
    }
  }

  BLE.setLocalName(DEVICE_NAME);
  BLE.setDeviceName(DEVICE_NAME);
  BLE.setAdvertisedService(nusService);

  nusService.addCharacteristic(rxCharacteristic);
  nusService.addCharacteristic(txCharacteristic);
  BLE.addService(nusService);

  rxCharacteristic.setEventHandler(BLEWritten, onRxWritten);

  BLE.advertise();

  Serial.print("BLE server advertising as '");
  Serial.print(DEVICE_NAME);
  Serial.println("'. Waiting for a phone to connect...");
}

void onRxWritten(BLEDevice central, BLECharacteristic characteristic) {
  int len = characteristic.valueLength();
  if (len <= 0) {
    return;
  }

  char buffer[RX_BUFFER_SIZE + 1];
  if (len > RX_BUFFER_SIZE) {
    len = RX_BUFFER_SIZE;
  }
  memcpy(buffer, characteristic.value(), len);
  buffer[len] = '\0';

  Serial.print("From phone: ");
  Serial.println(buffer);

  // Scroll the received message across the dot-matrix.
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(70);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(buffer);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();

  // Acknowledge back to the phone.
  char ack[RX_BUFFER_SIZE + 16];
  snprintf(ack, sizeof(ack), "ACK: %s\n", buffer);
  sendToPhone(ack);
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to: ");
    Serial.println(central.address());
    showFrame(FRAME_HEART);
    sendToPhone("Hello from UNO-R4 BLE server!\n");

    while (central.connected()) {
      BLE.poll();
    }

    Serial.print("Disconnected from: ");
    Serial.println(central.address());
    showFrame(FRAME_WAIT);
  }
}
