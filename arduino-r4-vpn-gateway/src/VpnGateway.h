/*
 * VpnGateway.h  —  Orchestrates WiFi association and the WireGuard tunnel as a
 * small, non-blocking state machine.  Everything is driven by update(), which
 * you call from loop() as often as possible; nothing here blocks for longer
 * than a single socket poll.
 *
 * The actual tunnel is hidden behind a thin seam so the rest of the firmware
 * (LED dashboard, status server) never needs to know whether it is talking to
 * a real WireGuard interface or the built-in simulator.
 */
#ifndef VPN_GATEWAY_H
#define VPN_GATEWAY_H

#include <Arduino.h>
#include "config.h"

/* Lifecycle of the gateway, surfaced to the dashboard and web UI. */
enum class VpnState : uint8_t {
  Boot,            // powered on, nothing started yet
  WifiConnecting,  // joining home WiFi
  Handshaking,     // WiFi up, negotiating the WireGuard handshake
  Online,          // tunnel up, last handshake recent
  Stalled,         // tunnel was up but handshake has gone quiet
  WifiLost,        // dropped off WiFi, will retry
  Error            // unrecoverable config/runtime error
};

/* A snapshot of live metrics.  Cheap to copy; passed by value to consumers. */
struct VpnMetrics {
  uint32_t txBytes      = 0;   // bytes sent through the tunnel
  uint32_t rxBytes      = 0;   // bytes received through the tunnel
  uint32_t txRate       = 0;   // bytes/sec, smoothed
  uint32_t rxRate       = 0;   // bytes/sec, smoothed
  uint32_t handshakes   = 0;   // successful handshakes since boot
  uint32_t lastHandshakeAgoMs = 0; // ms since last good handshake
  int32_t  rssi         = 0;   // WiFi signal strength, dBm
  uint32_t uptimeMs     = 0;   // ms since the tunnel first came online
};

const char* vpnStateName(VpnState s);

class VpnGateway {
 public:
  VpnGateway();

  /* Bring up WiFi + tunnel objects.  Returns false only on a fatal config
   * problem (e.g. an obviously empty WireGuard key in real mode). */
  bool begin();

  /* Pump the state machine.  Call every loop iteration. */
  void update();

  VpnState state() const { return state_; }
  VpnMetrics metrics() const { return metrics_; }
  const char* localTunnelIp() const { return WG_LOCAL_IP; }
  const char* lanIp() const { return lanIp_; }

 private:
  void enter(VpnState next);
  bool connectWifi();
  void pollWifi();
  void startTunnel();
  void pollTunnel();
  void sampleRates();

  VpnState   state_;
  VpnMetrics metrics_;
  char       lanIp_[16];

  uint32_t   stateEnteredMs_;
  uint32_t   lastSampleMs_;
  uint32_t   prevTx_;
  uint32_t   prevRx_;

#ifdef VPN_SIMULATION
  // Simulator bookkeeping so the demo metrics look organic.
  uint32_t   simHandshakeAtMs_;
#endif
};

#endif /* VPN_GATEWAY_H */
