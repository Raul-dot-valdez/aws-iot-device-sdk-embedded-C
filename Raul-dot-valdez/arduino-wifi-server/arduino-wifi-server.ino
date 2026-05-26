/*
 * Arduino UNO R4 WiFi - WiFi HTTP Server
 *
 * Joins an existing WiFi network (station mode) and runs an HTTP web server
 * on port 80. Any phone on the same network opens the board's IP in a browser
 * and gets a control page -- no app required. Text submitted from the page
 * scrolls across the built-in 12x8 LED dot-matrix, and quick buttons show
 * status icons.
 *
 * Hardware:  Arduino UNO R4 WiFi (Renesas RA4M1 + ESP32-S3 radio, on-board
 *            12x8 LED matrix).
 * Libraries: WiFiS3, Arduino_LED_Matrix (both bundled with the UNO R4 board
 *            package).
 *
 * Set your network credentials in arduino_secrets.h.
 */

#include "WiFiS3.h"
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h"

const char* ssid = SECRET_SSID;
const char* pass = SECRET_PASS;

WiFiServer server(80);
ArduinoLEDMatrix matrix;

// 12x8 frames for status icons (each row is a 12-bit pattern).
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

void scrollText(const String& text) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(70);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print(text.c_str());
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
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
  Serial.println("/ in your phone's browser.");

  showFrame(FRAME_HEART);
  server.begin();
}

// URL-decode a query value (handles %XX and '+').
String urlDecode(const String& src) {
  String out;
  out.reserve(src.length());
  for (unsigned int i = 0; i < src.length(); i++) {
    char c = src[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < src.length()) {
      char hi = src[i + 1];
      char lo = src[i + 2];
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return 0;
      };
      out += (char)((hex(hi) << 4) | hex(lo));
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

void sendPage(WiFiClient& client, const String& notice) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>UNO R4 WiFi Server</title>");
  client.println("<style>body{font-family:sans-serif;margin:24px;max-width:420px}"
                 "input,button{font-size:18px;padding:10px;margin:6px 0;width:100%}"
                 ".row{display:flex;gap:8px}.notice{color:#0a0;font-weight:bold}</style>");
  client.println("</head><body>");
  client.println("<h2>UNO R4 WiFi Server</h2>");
  if (notice.length()) {
    client.print("<p class='notice'>");
    client.print(notice);
    client.println("</p>");
  }
  client.println("<form action='/' method='get'>");
  client.println("<input name='msg' placeholder='Text to scroll on the matrix' autofocus>");
  client.println("<button type='submit'>Scroll on LED matrix</button>");
  client.println("</form>");
  client.println("<div class='row'>");
  client.println("<a href='/?icon=heart' style='flex:1'><button>Heart</button></a>");
  client.println("<a href='/?icon=check' style='flex:1'><button>Check</button></a>");
  client.println("</div>");
  client.println("</body></html>");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the request line, e.g. "GET /?msg=hello HTTP/1.1".
  String requestLine = client.readStringUntil('\n');
  // Drain the rest of the headers.
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) {
      break;
    }
  }

  String notice;
  int sp1 = requestLine.indexOf(' ');
  int sp2 = requestLine.indexOf(' ', sp1 + 1);
  if (sp1 >= 0 && sp2 > sp1) {
    String path = requestLine.substring(sp1 + 1, sp2);
    int q = path.indexOf('?');
    if (q >= 0) {
      String query = path.substring(q + 1);

      int m = query.indexOf("msg=");
      if (m >= 0) {
        int end = query.indexOf('&', m);
        String raw = (end >= 0) ? query.substring(m + 4, end)
                                : query.substring(m + 4);
        String msg = urlDecode(raw);
        if (msg.length()) {
          Serial.print("Scrolling: ");
          Serial.println(msg);
          scrollText(msg);
          notice = "Scrolling: " + msg;
        }
      }

      int ic = query.indexOf("icon=");
      if (ic >= 0) {
        int end = query.indexOf('&', ic);
        String icon = (end >= 0) ? query.substring(ic + 5, end)
                                 : query.substring(ic + 5);
        if (icon == "heart") {
          showFrame(FRAME_HEART);
          notice = "Showing heart";
        } else if (icon == "check") {
          showFrame(FRAME_CHECK);
          notice = "Showing check";
        }
      }
    }
  }

  sendPage(client, notice);
  delay(1);
  client.stop();
}
