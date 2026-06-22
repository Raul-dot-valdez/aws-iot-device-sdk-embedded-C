#include "ThreatMonitor.h"

const char* threatLevelName(ThreatLevel level) {
  switch (level) {
    case ThreatLevel::Normal:   return "normal";
    case ThreatLevel::Elevated: return "elevated";
    case ThreatLevel::Alert:    return "alert";
  }
  return "unknown";
}

ThreatMonitor::ThreatMonitor()
    : reqScore_(0), authScore_(0), malScore_(0),
      totalReq_(0), totalAuthFail_(0), totalMalformed_(0),
      lastDecayMs_(0), level_(ThreatLevel::Normal)
#ifdef VPN_SIMULATION
      , nextSimProbeMs_(20000)  // first demo probe ~20 s after boot
#endif
{}

void ThreatMonitor::recompute() {
  // Auth failures and malformed bursts are strong "someone is poking me"
  // signals -> Alert. A request-rate spike alone is weaker (could be heavy but
  // legitimate use) -> Elevated.
  if (authScore_ >= THREAT_AUTH_FAIL_ALERT || malScore_ >= THREAT_MALFORMED_ALERT) {
    level_ = ThreatLevel::Alert;
  } else if (reqScore_ >= THREAT_REQ_RATE_ELEVATED) {
    level_ = ThreatLevel::Elevated;
  } else {
    level_ = ThreatLevel::Normal;
  }
}

void ThreatMonitor::report(Event e) {
#if THREAT_DETECTION_ENABLED
  switch (e) {
    case Event::Request:
      if (reqScore_ < THREAT_SCORE_MAX) reqScore_++;
      totalReq_++;
      break;
    case Event::AuthFailure:
      if (authScore_ < THREAT_SCORE_MAX) authScore_++;
      totalAuthFail_++;
      break;
    case Event::Malformed:
      if (malScore_ < THREAT_SCORE_MAX) malScore_++;
      totalMalformed_++;
      break;
  }
  recompute();
#else
  (void)e;
#endif
}

void ThreatMonitor::update() {
#if THREAT_DETECTION_ENABLED
  uint32_t now = millis();

#ifdef VPN_SIMULATION
  // So the tripwire is visible on a bare board with no attacker present,
  // periodically simulate a short probe burst that trips Alert, then decays.
  if (now >= nextSimProbeMs_) {
    for (uint16_t i = 0; i <= THREAT_AUTH_FAIL_ALERT; i++) {
      if (authScore_ < THREAT_SCORE_MAX) authScore_++;
      totalAuthFail_++;
    }
    if (malScore_ < THREAT_SCORE_MAX) malScore_++;
    nextSimProbeMs_ = now + 75000;  // again ~75 s later
    recompute();
  }
#endif

  if (now - lastDecayMs_ >= THREAT_DECAY_MS) {
    lastDecayMs_ = now;
    if (reqScore_)  reqScore_--;
    if (authScore_) authScore_--;
    if (malScore_)  malScore_--;
    recompute();
  }
#endif
}
