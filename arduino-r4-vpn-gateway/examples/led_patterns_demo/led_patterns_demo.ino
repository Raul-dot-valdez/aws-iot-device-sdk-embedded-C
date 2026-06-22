/*
 * led_patterns_demo.ino
 * ---------------------------------------------------------------------------
 * A standalone sketch that cycles through every VPN dashboard animation on the
 * UNO R4 WiFi's 12x8 LED matrix — no WiFi, no config, no WireGuard needed.
 *
 * Use it to verify the matrix works and to preview each state's look before
 * building the full gateway.  Open the Serial Monitor at 115200 to see which
 * state is on screen.
 *
 * It reuses the real LedDashboard renderer from the parent project, so what you
 * see here is exactly what the gateway will show.
 *
 * Note: we include the renderer's .cpp directly so this example links as a
 * single self-contained sketch (the Arduino IDE only compiles sources in this
 * folder, not the parent project's src/).
 */
#include "../../src/LedDashboard.cpp"

LedDashboard dashboard;

struct Demo { VpnState state; const char* name; uint16_t ms; };

Demo demos[] = {
  { VpnState::WifiConnecting, "wifi-connecting", 3000 },
  { VpnState::Handshaking,    "handshaking",     3000 },
  { VpnState::Online,         "online (busy)",   5000 },
  { VpnState::Stalled,        "stalled",         3000 },
  { VpnState::WifiLost,       "wifi-lost",       3000 },
  { VpnState::Error,          "error",           3000 },
};

void setup() {
  Serial.begin(115200);
  dashboard.begin();
  dashboard.showBanner();
}

void loop() {
  for (auto& d : demos) {
    Serial.print(F("Showing: ")); Serial.println(d.name);
    uint32_t end = millis() + d.ms;
    while (millis() < end) {
      VpnMetrics m;
      // Fake a busy link so the Online marquee animates quickly.
      m.txRate = 8000; m.rxRate = 14000; m.handshakes = 3;
      dashboard.update(d.state, m);
    }
  }
}
