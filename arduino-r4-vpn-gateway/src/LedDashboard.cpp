#include "LedDashboard.h"

/* The matrix driver and ArduinoGraphics ship with the UNO R4 board package.
 * Guard the includes so this file still compiles (as a no-op renderer) on a
 * host build or a board without the matrix. */
#if __has_include("Arduino_LED_Matrix.h")
  #include "Arduino_LED_Matrix.h"
  #define HAVE_MATRIX 1
  static ArduinoLEDMatrix matrix;
#else
  #define HAVE_MATRIX 0
#endif

#if __has_include(<ArduinoGraphics.h>) && HAVE_MATRIX
  #include <ArduinoGraphics.h>
  #define HAVE_GRAPHICS 1
#else
  #define HAVE_GRAPHICS 0
#endif

LedDashboard::LedDashboard()
    : lastFrameMs_(0), marqueePhase_(0), lastMarqueeMs_(0) {
  clear();
}

void LedDashboard::begin() {
#if HAVE_MATRIX
  matrix.begin();
#endif
  clear();
  flush();
}

void LedDashboard::clear() {
  memset(frame_, 0, sizeof(frame_));
}

void LedDashboard::set(int x, int y, bool on) {
  if (x < 0 || x >= LED_W || y < 0 || y >= LED_H) return;
  frame_[y][x] = on ? 1 : 0;
}

void LedDashboard::flush() {
#if HAVE_MATRIX
  matrix.renderBitmap(frame_, LED_H, LED_W);
#endif
}

void LedDashboard::showBanner() {
#if HAVE_GRAPHICS
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(70);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print(LED_BANNER_TEXT);
  matrix.endText(SCROLL_LEFT);   // blocking scroll, fine at boot
  matrix.endDraw();
#else
  // No graphics lib: a couple of friendly blinks instead of scrolling text.
  uint32_t end = millis() + LED_BANNER_MS;
  bool on = false;
  while (millis() < end) {
    on = !on;
    clear();
    if (on) for (int x = 0; x < LED_W; x++) { set(x, 0, true); set(x, LED_H - 1, true); }
    flush();
    delay(200);
  }
#endif
  clear();
  flush();
}

/* --- WifiConnecting: a vertical scan line sweeping across the panel. ------ */
void LedDashboard::drawScan(uint32_t t) {
  clear();
  int x = (t / 90) % LED_W;
  for (int y = 0; y < LED_H; y++) set(x, y, true);
  // faint trailing dot for a sense of motion
  set((x + LED_W - 1) % LED_W, LED_H / 2, true);
}

/* --- Handshaking: a square ring expanding from the centre, then repeating. */
void LedDashboard::drawPulse(uint32_t t) {
  clear();
  int step = (t / 140) % 5;       // ring radius 0..4
  int cx = LED_W / 2 - 1, cy = LED_H / 2 - 1;
  for (int x = -step; x <= step; x++) {
    set(cx + x, cy - step, true);
    set(cx + x, cy + step, true);
  }
  for (int y = -step; y <= step; y++) {
    set(cx - step, cy + y, true);
    set(cx + step, cy + y, true);
  }
}

/* A 7x7 padlock, drawn with its top-left at (ox, oy). */
void LedDashboard::drawPadlock(int ox, int oy) {
  static const uint8_t lock[7] = {
    0b0011100,  // ..###..
    0b0100010,  // .#...#.
    0b0100010,  // .#...#.
    0b1111111,  // #######
    0b1111111,  // #######
    0b1011101,  // #.###.#
    0b1111111,  // #######
  };
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 7; col++) {
      bool on = (lock[row] >> (6 - col)) & 0x1;
      if (on) set(ox + col, oy + row, true);
    }
  }
}

/* --- Online: padlock + a bottom-row data marquee tied to throughput. ----- */
void LedDashboard::drawOnline(uint32_t t, const VpnMetrics& m) {
  clear();
  drawPadlock(2, 0);   // centred horizontally (cols 2..8), rows 0..6

  // Advance the marquee faster as combined throughput grows.  Cap the speed so
  // it stays readable.  Step interval shrinks from ~220ms (idle) to ~40ms.
  uint32_t rate = m.txRate + m.rxRate;            // bytes/sec
  uint32_t interval = rate > 12000 ? 40 : 220 - (rate / 75);
  if (interval < 40) interval = 40;
  if (millis() - lastMarqueeMs_ >= interval) {
    lastMarqueeMs_ = millis();
    marqueePhase_++;
  }
  // Dashed pattern marching left along the bottom row.
  for (int x = 0; x < LED_W; x++) {
    bool on = ((x + marqueePhase_) % 3) == 0;
    set(x, LED_H - 1, on);
  }
  (void)t;
}

/* --- WifiLost: a small WiFi glyph with a slash, blinking. ----------------- */
void LedDashboard::drawWifiLost(uint32_t t) {
  clear();
  bool blink = (t / 500) % 2;
  if (blink) { flush(); return; }
  // crude wifi arcs growing upward from a base dot at (5,6)
  set(5, 6, true);
  set(4, 5, true); set(6, 5, true);
  set(3, 4, true); set(7, 4, true);
  set(2, 3, true); set(8, 3, true);
  // diagonal slash through it
  for (int i = 0; i < LED_H; i++) set(i + 1, i, true);
}

/* --- Error: a bold X across the whole panel. ------------------------------ */
void LedDashboard::drawX() {
  clear();
  for (int i = 0; i < LED_H; i++) {
    set(i + 2, i, true);          // top-left -> bottom-right
    set(LED_W - 3 - i, i, true);  // top-right -> bottom-left
  }
}

void LedDashboard::update(VpnState state, const VpnMetrics& m) {
  uint32_t now = millis();
  if (now - lastFrameMs_ < (1000 / LED_FPS)) return;   // frame-rate cap (LED_FPS)
  lastFrameMs_ = now;

  switch (state) {
    case VpnState::Boot:
    case VpnState::WifiConnecting: drawScan(now);       break;
    case VpnState::Handshaking:    drawPulse(now);      break;
    case VpnState::Online:         drawOnline(now, m);  break;
    case VpnState::Stalled:
      // reuse the padlock but blink it to signal "quiet"
      clear();
      if ((now / 400) % 2) drawPadlock(2, 0);
      break;
    case VpnState::WifiLost:       drawWifiLost(now);   break;
    case VpnState::Error:          drawX();             break;
  }
  flush();
}
