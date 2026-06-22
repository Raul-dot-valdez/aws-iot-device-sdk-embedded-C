/*
 * StatusServer.h  —  A dependency-free HTTP status page for the gateway.
 *
 * Serves two routes from the board's LAN IP:
 *   GET /            a self-contained HTML dashboard (auto-refreshing)
 *   GET /api/status  a small JSON blob with live state + metrics
 *
 * It is deliberately tiny and read-only: no tunnel control, no auth, no
 * dependencies beyond the WiFi server class.  handleClient() is non-blocking
 * and services at most one request per call.
 */
#ifndef STATUS_SERVER_H
#define STATUS_SERVER_H

#include <Arduino.h>
#include "VpnGateway.h"

#if __has_include(<WiFiS3.h>)
  #include <WiFiS3.h>
  #define STATUS_HAVE_WIFI 1
#elif __has_include(<WiFi.h>)
  #include <WiFi.h>
  #define STATUS_HAVE_WIFI 1
#else
  #define STATUS_HAVE_WIFI 0
#endif

class StatusServer {
 public:
  StatusServer();
  void begin();
  void handleClient(const VpnGateway& gw);

 private:
#if STATUS_HAVE_WIFI
  WiFiServer server_;
#endif
  bool started_;

  void sendJson(Stream& out, const VpnGateway& gw);
  void sendHtml(Stream& out);
};

#endif /* STATUS_SERVER_H */
