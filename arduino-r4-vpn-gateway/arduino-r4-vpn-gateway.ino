/*
 * Arduino UNO R4 WiFi — VPN Gateway
 * ===================================================================
 * Turns an UNO R4 WiFi into a WireGuard tunnel endpoint for your home network,
 * using the on-board 12x8 LED matrix as a live status display and a tiny web
 * page for details.
 *
 * Three cooperating modules, all non-blocking:
 *   - VpnGateway   : WiFi association + WireGuard tunnel state machine
 *   - LedDashboard : renders the current state/throughput on the LED matrix
 *   - StatusServer : serves a status web page + JSON on the LAN
 *
 * First flash:  leave VPN_SIMULATION defined in src/config.h, upload, and watch
 * the dashboard come alive with no server required.  When you are ready for a
 * real tunnel, fill in your WireGuard keys/endpoint and comment that line out.
 *
 * See README.md for wiring (there is none beyond USB power!), server setup,
 * and the honest scope/limitations of running a VPN on this class of board.
 */
#include "src/config.h"
#include "src/VpnGateway.h"
#include "src/LedDashboard.h"
#include "src/StatusServer.h"

VpnGateway   gateway;
LedDashboard dashboard;
StatusServer status;

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) { /* wait briefly for USB serial */ }

  Serial.println();
  Serial.println(F("=== Arduino UNO R4 WiFi — VPN Gateway ==="));
#ifdef VPN_SIMULATION
  Serial.println(F("Mode: SIMULATION (no WireGuard server needed)"));
#else
  Serial.println(F("Mode: LIVE WireGuard tunnel"));
#endif

#if LED_DASHBOARD_ENABLED
  dashboard.begin();
  dashboard.showBanner();
#endif

  if (!gateway.begin()) {
    Serial.println(F("FATAL: gateway config invalid (check WireGuard keys in config.h)"));
  }

  status.begin();
}

void loop() {
  gateway.update();

#if LED_DASHBOARD_ENABLED
  dashboard.update(gateway.state(), gateway.metrics());
#endif

  status.handleClient(gateway);

  // Log state transitions and a periodic heartbeat to the serial monitor.
  static VpnState lastState = VpnState::Boot;
  static uint32_t lastLog = 0;
  if (gateway.state() != lastState) {
    lastState = gateway.state();
    Serial.print(F("[state] -> "));
    Serial.println(vpnStateName(gateway.state()));
    if (gateway.state() == VpnState::Online) {
      Serial.print(F("  LAN http://"));
      Serial.println(gateway.lanIp());
    }
  }
  if (millis() - lastLog > 5000) {
    lastLog = millis();
    VpnMetrics m = gateway.metrics();
    Serial.print(F("[hb] "));
    Serial.print(vpnStateName(gateway.state()));
    Serial.print(F("  rssi="));   Serial.print(m.rssi);
    Serial.print(F("dBm  dn="));  Serial.print(m.rxRate);
    Serial.print(F("B/s up="));   Serial.print(m.txRate);
    Serial.print(F("B/s  hs="));  Serial.println(m.handshakes);
  }
}
