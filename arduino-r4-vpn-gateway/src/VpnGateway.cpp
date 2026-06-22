#include "VpnGateway.h"

/* ---- WiFi backend selection -------------------------------------------------
 * The UNO R4 WiFi ships the WiFiS3 library.  We fall back to the generic
 * <WiFi.h> name so the same code compiles on ESP32-class boards too. */
#if __has_include(<WiFiS3.h>)
  #include <WiFiS3.h>
  #define VPN_HAVE_WIFI 1
#elif __has_include(<WiFi.h>)
  #include <WiFi.h>
  #define VPN_HAVE_WIFI 1
#else
  #define VPN_HAVE_WIFI 0
#endif

/* ---- Tunnel backend selection ----------------------------------------------
 * Real mode needs the WireGuard library (Kenta Ida's WireGuard-ESP32-Arduino
 * exposes a `WireGuard` class).  If it is missing and we are NOT simulating,
 * stop the build with a clear message instead of failing cryptically later. */
#ifndef VPN_SIMULATION
  #if __has_include(<WireGuard.h>)
    #include <WireGuard.h>
    static WireGuard wg;
    #define VPN_HAVE_WIREGUARD 1
  #else
    #error "Real tunnel mode requires the WireGuard library. Install it, or define VPN_SIMULATION in config.h to run the dashboard demo."
  #endif
#endif

#ifndef WIFI_USE_DHCP
static IPAddress makeIp(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return IPAddress(a, b, c, d);
}
#endif

const char* vpnStateName(VpnState s) {
  switch (s) {
    case VpnState::Boot:           return "boot";
    case VpnState::WifiConnecting: return "wifi-connecting";
    case VpnState::Handshaking:    return "handshaking";
    case VpnState::Online:         return "online";
    case VpnState::Stalled:        return "stalled";
    case VpnState::WifiLost:       return "wifi-lost";
    case VpnState::Error:          return "error";
  }
  return "unknown";
}

VpnGateway::VpnGateway()
    : state_(VpnState::Boot),
      stateEnteredMs_(0),
      lastSampleMs_(0),
      prevTx_(0),
      prevRx_(0)
#ifdef VPN_SIMULATION
      , simHandshakeAtMs_(0)
#endif
{
  strncpy(lanIp_, "0.0.0.0", sizeof(lanIp_));
}

void VpnGateway::enter(VpnState next) {
  state_ = next;
  stateEnteredMs_ = millis();
}

bool VpnGateway::begin() {
#ifndef VPN_SIMULATION
  // Fail fast on the most common foot-gun: forgetting to paste the keys.
  if (String(WG_PRIVATE_KEY).indexOf("PASTE_") >= 0 ||
      String(WG_PEER_PUBLIC_KEY).indexOf("PASTE_") >= 0) {
    enter(VpnState::Error);
    return false;
  }
#endif
  enter(VpnState::WifiConnecting);
  return true;
}

bool VpnGateway::connectWifi() {
#if VPN_HAVE_WIFI
#ifndef WIFI_USE_DHCP
  WiFi.config(makeIp(WIFI_STATIC_IP), makeIp(WIFI_DNS),
              makeIp(WIFI_GATEWAY), makeIp(WIFI_SUBNET));
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  return WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

void VpnGateway::pollWifi() {
#if VPN_HAVE_WIFI
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    snprintf(lanIp_, sizeof(lanIp_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    metrics_.rssi = WiFi.RSSI();
    startTunnel();
    return;
  }
#endif
  // Still trying.  Give WiFi a generous window before deciding it failed.
  const uint32_t elapsed = millis() - stateEnteredMs_;
  if (elapsed > 15000) {
#ifdef VPN_SIMULATION
    // No usable WiFi on the bench?  Press on so the LED demo still runs.
    strncpy(lanIp_, "0.0.0.0", sizeof(lanIp_));
    startTunnel();
#else
    enter(VpnState::WifiLost);
#endif
  }
}

void VpnGateway::startTunnel() {
#ifdef VPN_SIMULATION
  simHandshakeAtMs_ = millis() + 1500;  // pretend the handshake takes a moment
  enter(VpnState::Handshaking);
#else
  IPAddress local;
  local.fromString(WG_LOCAL_IP);
  // Matches the common WireGuard-ESP32-Arduino signature:
  //   begin(localIP, privateKey, endpointHost, peerPublicKey, endpointPort)
  // A pre-shared key is configured server-side; this binding doesn't pass one.
  bool ok = wg.begin(local,
                     WG_PRIVATE_KEY,
                     WG_ENDPOINT_HOST,
                     WG_PEER_PUBLIC_KEY,
                     (uint16_t)WG_ENDPOINT_PORT);
  enter(ok ? VpnState::Handshaking : VpnState::Error);
#endif
}

void VpnGateway::pollTunnel() {
#ifdef VPN_SIMULATION
  // --- Simulated tunnel: generate handshakes and plausible traffic. ---
  uint32_t now = millis();
  if (state_ == VpnState::Handshaking && now >= simHandshakeAtMs_) {
    metrics_.handshakes++;
    metrics_.uptimeMs = 0;
    enter(VpnState::Online);
  }
  if (state_ == VpnState::Online) {
    // Bursty traffic: a slow base load plus occasional spikes.
    uint32_t base = 600 + (now % 400);
    uint32_t spike = ((now / 1000) % 7 == 0) ? 9000 : 0;
    metrics_.txBytes += (base + spike) / 8;
    metrics_.rxBytes += (base + spike) / 5;
    metrics_.lastHandshakeAgoMs = now - (simHandshakeAtMs_);
    metrics_.rssi = -50 - (int)((now / 500) % 25);  // wander -50..-75 dBm
    // Re-handshake every ~120s like real WireGuard.
    if (metrics_.lastHandshakeAgoMs > 120000) {
      metrics_.handshakes++;
      simHandshakeAtMs_ = now;
    }
  }
#else
  // --- Real tunnel: read state from the WireGuard interface. ---
  if (wg.is_peer_up()) {
    if (state_ != VpnState::Online) {
      metrics_.handshakes++;
      metrics_.uptimeMs = 0;
    }
    metrics_.lastHandshakeAgoMs = wg.last_handshake_ago_ms();
    metrics_.txBytes = wg.tx_bytes();
    metrics_.rxBytes = wg.rx_bytes();
    enter(VpnState::Online);
  } else if (state_ == VpnState::Online) {
    enter(VpnState::Stalled);
  }
  #if VPN_HAVE_WIFI
  metrics_.rssi = WiFi.RSSI();
  if (WiFi.status() != WL_CONNECTED) enter(VpnState::WifiLost);
  #endif
#endif
}

void VpnGateway::sampleRates() {
  uint32_t now = millis();
  uint32_t dt = now - lastSampleMs_;
  if (dt < 500) return;  // recompute rates twice a second
  lastSampleMs_ = now;

  uint32_t dtx = metrics_.txBytes - prevTx_;
  uint32_t drx = metrics_.rxBytes - prevRx_;
  prevTx_ = metrics_.txBytes;
  prevRx_ = metrics_.rxBytes;

  uint32_t instTx = (dtx * 1000UL) / dt;
  uint32_t instRx = (drx * 1000UL) / dt;
  // Exponential smoothing so the LED bars breathe instead of flickering.
  metrics_.txRate = (metrics_.txRate * 3 + instTx) / 4;
  metrics_.rxRate = (metrics_.rxRate * 3 + instRx) / 4;

  if (state_ == VpnState::Online) metrics_.uptimeMs += dt;
}

void VpnGateway::update() {
  switch (state_) {
    case VpnState::Boot:
      enter(VpnState::WifiConnecting);
      // fallthrough on next tick
      break;

    case VpnState::WifiConnecting:
      connectWifi();   // non-blocking kick; status checked in pollWifi
      pollWifi();
      break;

    case VpnState::WifiLost:
      // Back off a few seconds, then retry the whole chain.
      if (millis() - stateEnteredMs_ > 5000) enter(VpnState::WifiConnecting);
      break;

    case VpnState::Handshaking:
    case VpnState::Online:
    case VpnState::Stalled:
      pollTunnel();
      break;

    case VpnState::Error:
      // Latched.  A power cycle (or fixing config) is required.
      break;
  }

  sampleRates();
}
