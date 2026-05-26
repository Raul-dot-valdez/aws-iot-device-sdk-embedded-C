/*
 * Arduino UNO R4 WiFi - WiFi WebSocket Server
 *
 * Joins an existing WiFi network (station mode) and runs a combined HTTP +
 * WebSocket server on port 80. A phone/iPad on the same network opens the
 * board's IP in a browser; the served page opens a WebSocket back to the
 * board for a live, low-latency, bidirectional link -- no app required.
 *
 * Messages from the browser scroll across the built-in 12x8 LED dot-matrix
 * (or "icon:heart" / "icon:check" show status icons). The board pushes an
 * uptime heartbeat and ACKs back over the same socket.
 *
 * Hardware:  Arduino UNO R4 WiFi (Renesas RA4M1 + ESP32-S3 radio, on-board
 *            12x8 LED matrix).
 * Libraries: WiFiS3, Arduino_LED_Matrix (both bundled with the UNO R4 board
 *            package). The WebSocket layer (RFC 6455 handshake + framing,
 *            including SHA-1 and Base64) is implemented here -- no external
 *            WebSocket library is required.
 *
 * Set your network credentials in arduino_secrets.h.
 */

#include "WiFiS3.h"
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h"

const char* ssid = SECRET_SSID;
const char* pass = SECRET_PASS;

#define WS_BUF 256

WiFiServer server(80);
ArduinoLEDMatrix matrix;

// 12x8 status icons (each row is a 12-bit pattern).
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

const uint8_t FRAME_CHECK[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,1,0},
  {0,0,0,0,0,0,0,0,0,1,1,0},
  {0,0,0,0,0,0,0,0,1,1,0,0},
  {0,1,0,0,0,0,0,1,1,0,0,0},
  {0,1,1,0,0,0,1,1,0,0,0,0},
  {0,0,1,1,0,1,1,0,0,0,0,0},
  {0,0,0,1,1,1,0,0,0,0,0,0},
  {0,0,0,0,1,0,0,0,0,0,0,0},
};

void showFrame(const uint8_t frame[8][12]) {
  matrix.renderBitmap(frame, 8, 12);
}

void scrollText(const char* text) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(70);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print(text);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

// ---- SHA-1 (for the WebSocket accept key) ----------------------------------
static uint32_t rol32(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE,
           h3 = 0x10325476, h4 = 0xC3D2E1F0;
  uint64_t ml = (uint64_t)len * 8;
  size_t total = ((len + 8) / 64 + 1) * 64;

  uint8_t buf[128];                 // input here is always small (< ~100 bytes)
  if (total > sizeof(buf)) total = sizeof(buf);
  memset(buf, 0, total);
  memcpy(buf, data, len);
  buf[len] = 0x80;
  for (int i = 0; i < 8; i++) {
    buf[total - 1 - i] = (uint8_t)(ml >> (8 * i));
  }

  for (size_t off = 0; off < total; off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      w[i] = ((uint32_t)buf[off + i * 4] << 24) |
             ((uint32_t)buf[off + i * 4 + 1] << 16) |
             ((uint32_t)buf[off + i * 4 + 2] << 8) |
             ((uint32_t)buf[off + i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
      w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20)      { f = (b & c) | ((~b) & d);          k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDC; }
      else             { f = b ^ c ^ d;                     k = 0xCA62C1D6; }
      uint32_t tmp = rol32(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol32(b, 30); b = a; a = tmp;
    }
    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
  }

  uint32_t hh[5] = { h0, h1, h2, h3, h4 };
  for (int i = 0; i < 5; i++) {
    out[i * 4]     = hh[i] >> 24;
    out[i * 4 + 1] = hh[i] >> 16;
    out[i * 4 + 2] = hh[i] >> 8;
    out[i * 4 + 3] = hh[i];
  }
}

String base64(const uint8_t* d, size_t n) {
  static const char* T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16;
    if (i + 1 < n) v |= (uint32_t)d[i + 1] << 8;
    if (i + 2 < n) v |= d[i + 2];
    out += T[(v >> 18) & 63];
    out += T[(v >> 12) & 63];
    out += (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out += (i + 2 < n) ? T[v & 63] : '=';
  }
  return out;
}

// ---- WebSocket framing -----------------------------------------------------
bool readBytes(WiFiClient& c, uint8_t* buf, size_t n, unsigned long timeout = 3000) {
  size_t got = 0;
  unsigned long start = millis();
  while (got < n) {
    if (!c.connected()) return false;
    if (c.available()) {
      int r = c.read(buf + got, n - got);
      if (r > 0) { got += r; start = millis(); }
    } else {
      if (millis() - start > timeout) return false;
      delay(1);
    }
  }
  return true;
}

void wsSendText(WiFiClient& c, const char* msg) {
  size_t len = strlen(msg);
  uint8_t header[4];
  size_t hl = 0;
  header[hl++] = 0x81;                       // FIN + text opcode
  if (len < 126) {
    header[hl++] = (uint8_t)len;
  } else {
    header[hl++] = 126;
    header[hl++] = (len >> 8) & 0xFF;
    header[hl++] = len & 0xFF;
  }
  c.write(header, hl);
  c.write((const uint8_t*)msg, len);
}

void processMessage(WiFiClient& c, char* msg) {
  Serial.print("WS recv: ");
  Serial.println(msg);

  if (strncmp(msg, "icon:", 5) == 0) {
    const char* name = msg + 5;
    if (strcmp(name, "heart") == 0) {
      showFrame(FRAME_HEART);
      wsSendText(c, "ACK icon: heart");
    } else if (strcmp(name, "check") == 0) {
      showFrame(FRAME_CHECK);
      wsSendText(c, "ACK icon: check");
    } else {
      wsSendText(c, "ERR unknown icon");
    }
    return;
  }

  scrollText(msg);
  char ack[WS_BUF + 16];
  snprintf(ack, sizeof(ack), "ACK scroll: %s", msg);
  wsSendText(c, ack);
}

// Returns false to signal the connection should close.
bool handleFrame(WiFiClient& c) {
  uint8_t h[2];
  if (!readBytes(c, h, 2)) return false;

  uint8_t opcode = h[0] & 0x0F;
  bool masked = h[1] & 0x80;
  uint64_t len = h[1] & 0x7F;

  if (len == 126) {
    uint8_t e[2];
    if (!readBytes(c, e, 2)) return false;
    len = ((uint64_t)e[0] << 8) | e[1];
  } else if (len == 127) {
    uint8_t e[8];
    if (!readBytes(c, e, 8)) return false;
    len = 0;
    for (int i = 0; i < 8; i++) len = (len << 8) | e[i];
  }

  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked && !readBytes(c, mask, 4)) return false;

  static uint8_t payload[WS_BUF];
  uint64_t remaining = len;
  uint64_t idx = 0;
  while (remaining > 0) {
    uint8_t chunk[64];
    size_t n = remaining > sizeof(chunk) ? sizeof(chunk) : (size_t)remaining;
    if (!readBytes(c, chunk, n)) return false;
    for (size_t i = 0; i < n; i++) {
      uint8_t b = masked ? (chunk[i] ^ mask[idx & 3]) : chunk[i];
      if (idx < WS_BUF - 1) payload[idx] = b;
      idx++;
    }
    remaining -= n;
  }
  size_t plen = (len < WS_BUF - 1) ? (size_t)len : WS_BUF - 1;
  payload[plen] = '\0';

  if (opcode == 0x8) {                         // close
    uint8_t cl[2] = {0x88, 0x00};
    c.write(cl, 2);
    return false;
  } else if (opcode == 0x9) {                  // ping -> pong
    uint8_t pong[2] = {0x8A, 0x00};
    c.write(pong, 2);
    return true;
  } else if (opcode == 0xA) {                  // pong
    return true;
  } else if (opcode == 0x1 || opcode == 0x0) { // text / continuation
    processMessage(c, (char*)payload);
  }
  return true;
}

void serveWebSocket(WiFiClient client, const String& key) {
  String concat = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t digest[20];
  sha1((const uint8_t*)concat.c_str(), concat.length(), digest);
  String accept = base64(digest, 20);

  client.print("HTTP/1.1 101 Switching Protocols\r\n");
  client.print("Upgrade: websocket\r\n");
  client.print("Connection: Upgrade\r\n");
  client.print("Sec-WebSocket-Accept: ");
  client.print(accept);
  client.print("\r\n\r\n");

  Serial.println("WebSocket client connected");
  showFrame(FRAME_HEART);
  wsSendText(client, "Connected to UNO-R4 WebSocket server");

  unsigned long lastPush = millis();
  while (client.connected()) {
    while (client.available()) {
      if (!handleFrame(client)) {
        client.stop();
        Serial.println("WebSocket client disconnected");
        return;
      }
    }
    if (millis() - lastPush > 5000) {          // heartbeat: push uptime
      char buf[48];
      snprintf(buf, sizeof(buf), "uptime: %lus", millis() / 1000);
      wsSendText(client, buf);
      lastPush = millis();
    }
    delay(2);
  }
  client.stop();
  Serial.println("WebSocket client disconnected");
}

void sendPage(WiFiClient& client) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("Connection: close\r\n\r\n");
  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>UNO R4 WebSocket Server</title>");
  client.println("<style>body{font-family:sans-serif;margin:24px;max-width:440px}"
                 "input,button{font-size:18px;padding:10px;margin:6px 0}"
                 "#m{width:100%}.row{display:flex;gap:8px}.row button{flex:1}"
                 "#log{white-space:pre-wrap;background:#111;color:#0f0;padding:10px;"
                 "border-radius:6px;height:180px;overflow:auto;font-family:monospace}"
                 "</style></head><body>");
  client.println("<h2>UNO R4 WebSocket Server</h2>");
  client.println("<input id='m' placeholder='Text to scroll on the matrix' autofocus>");
  client.println("<button onclick='send()'>Send</button>");
  client.println("<div class='row'>");
  client.println("<button onclick=\"icon('heart')\">Heart</button>");
  client.println("<button onclick=\"icon('check')\">Check</button>");
  client.println("</div>");
  client.println("<div id='log'></div>");
  client.println("<script>");
  client.println("let ws;");
  client.println("function connect(){");
  client.println(" ws=new WebSocket('ws://'+location.host+'/');");
  client.println(" ws.onopen=()=>log('* connected');");
  client.println(" ws.onclose=()=>{log('* disconnected, retrying...');setTimeout(connect,1500);};");
  client.println(" ws.onmessage=e=>log('<< '+e.data);");
  client.println("}");
  client.println("function send(){let i=document.getElementById('m');"
                 "if(ws&&ws.readyState===1&&i.value){ws.send(i.value);log('>> '+i.value);i.value='';}}");
  client.println("function icon(n){if(ws&&ws.readyState===1){ws.send('icon:'+n);log('>> icon:'+n);}}");
  client.println("function log(t){let l=document.getElementById('log');l.textContent=t+'\\n'+l.textContent;}");
  client.println("document.getElementById('m').addEventListener('keydown',e=>{if(e.key==='Enter')send();});");
  client.println("connect();");
  client.println("</script></body></html>");
}

// Reads request headers; returns true if this is a WebSocket upgrade and sets key.
bool readHeaders(WiFiClient& c, String& wsKey) {
  bool isWs = false;
  unsigned long start = millis();
  while (c.connected()) {
    if (c.available()) {
      String line = c.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) break;
      String lower = line;
      lower.toLowerCase();
      if (lower.startsWith("upgrade:") && lower.indexOf("websocket") >= 0) {
        isWs = true;
      }
      if (lower.startsWith("sec-websocket-key:")) {
        wsKey = line.substring(line.indexOf(':') + 1);
        wsKey.trim();
      }
    } else {
      if (millis() - start > 2000) break;
      delay(1);
    }
  }
  return isWs;
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000)) { }

  matrix.begin();
  showFrame(FRAME_CHECK);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true) { }
  }

  Serial.print("Connecting to '");
  Serial.print(ssid);
  Serial.println("' ...");

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(2000);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected. Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your phone or iPad browser.");

  showFrame(FRAME_HEART);
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  String wsKey;
  bool isWs = readHeaders(client, wsKey);

  if (isWs && wsKey.length()) {
    serveWebSocket(client, wsKey);
  } else {
    sendPage(client);
    delay(1);
    client.stop();
  }
}
