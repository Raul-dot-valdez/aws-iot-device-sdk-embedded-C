/*
 * ThreatMonitor.h  —  a tiny, O(1) anomaly tripwire ("the box notices it's
 * being probed").
 *
 * It does NOT pretend to be machine learning — on 32 KB of RAM that would be
 * dishonest. It is a set of decaying-score heuristics over the only attack
 * surface this device actually exposes: the status web server. It watches for
 * the *shape* of automated probing — repeated auth failures, malformed/oversized
 * requests, and request-rate spikes — and raises a threat level that the LED
 * dashboard and the status JSON surface so an operator can see it at a glance.
 *
 * Each signal is a small integer score that is bumped on an event and decays
 * over time, so the level naturally returns to Normal once probing stops.
 */
#ifndef THREAT_MONITOR_H
#define THREAT_MONITOR_H

#include <Arduino.h>
#include "config.h"

enum class ThreatLevel : uint8_t { Normal, Elevated, Alert };

const char* threatLevelName(ThreatLevel level);

class ThreatMonitor {
 public:
  enum class Event : uint8_t { Request, AuthFailure, Malformed };

  ThreatMonitor();

  /* Record an observation from the status server. */
  void report(Event e);

  /* Decay scores and (in simulation) inject demo probes. Call every loop(). */
  void update();

  ThreatLevel level() const { return level_; }

  // Cumulative totals since boot, surfaced in the status JSON.
  uint32_t totalRequests() const { return totalReq_; }
  uint32_t totalAuthFailures() const { return totalAuthFail_; }
  uint32_t totalMalformed() const { return totalMalformed_; }

 private:
  void recompute();

  uint16_t reqScore_;
  uint16_t authScore_;
  uint16_t malScore_;
  uint32_t totalReq_;
  uint32_t totalAuthFail_;
  uint32_t totalMalformed_;
  uint32_t lastDecayMs_;
  ThreatLevel level_;
#ifdef VPN_SIMULATION
  uint32_t nextSimProbeMs_;  // when to inject the next demo probe burst
#endif
};

#endif /* THREAT_MONITOR_H */
