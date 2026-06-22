/*
 * LedDashboard.h  —  Turns the UNO R4 WiFi's 12x8 LED matrix into a live VPN
 * status display.  Each gateway state gets its own animation so you can read
 * the tunnel's health from across the room without opening the web page:
 *
 *   WifiConnecting -> a scan line sweeping left to right ("searching")
 *   Handshaking    -> a square pulse expanding from the centre ("negotiating")
 *   Online         -> a padlock icon + a bottom-row data marquee whose speed
 *                     tracks real throughput ("secure, and how busy")
 *   Stalled        -> the padlock blinks ("tunnel quiet")
 *   WifiLost       -> a WiFi icon with a slash, blinking ("offline")
 *   Error          -> a big X ("check your config")
 *
 * Rendering is non-blocking: update() throttles itself to ~30 fps and returns
 * immediately the rest of the time, so it is safe to call every loop().
 */
#ifndef LED_DASHBOARD_H
#define LED_DASHBOARD_H

#include <Arduino.h>
#include "VpnGateway.h"
#include "ThreatMonitor.h"

#define LED_W 12
#define LED_H 8

class LedDashboard {
 public:
  LedDashboard();

  void begin();

  /* Blocking, runs once at startup: scrolls LED_BANNER_TEXT for ~LED_BANNER_MS
   * so you get a friendly hello before the state machine takes the display. */
  void showBanner();

  /* Non-blocking frame pump. Pass the current state + metrics each call. When
   * the threat tripwire is Elevated/Alert, a warning is overlaid on the frame. */
  void update(VpnState state, const VpnMetrics& m,
              ThreatLevel threat = ThreatLevel::Normal);

 private:
  void clear();
  void set(int x, int y, bool on);
  void flush();

  void drawScan(uint32_t t);
  void drawPulse(uint32_t t);
  void drawOnline(uint32_t t, const VpnMetrics& m);
  void drawPadlock(int ox, int oy);
  void drawWifiLost(uint32_t t);
  void drawX();
  void drawExclamation();

  uint8_t  frame_[LED_H][LED_W];
  uint32_t lastFrameMs_;
  uint32_t marqueePhase_;   // accumulates faster when throughput is higher
  uint32_t lastMarqueeMs_;
};

#endif /* LED_DASHBOARD_H */
