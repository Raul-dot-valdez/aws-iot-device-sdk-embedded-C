#include "StatusServer.h"

StatusServer::StatusServer()
#if STATUS_HAVE_WIFI
    : server_(STATUS_SERVER_PORT),
#else
    :
#endif
      started_(false) {}

void StatusServer::begin() {
#if STATUS_HAVE_WIFI && STATUS_SERVER_ENABLED
  server_.begin();
  started_ = true;
#endif
}

void StatusServer::sendJson(Stream& out, const VpnGateway& gw) {
  VpnMetrics m = gw.metrics();
  out.print("{");
  out.print("\"state\":\"");        out.print(vpnStateName(gw.state())); out.print("\",");
  out.print("\"lan_ip\":\"");       out.print(gw.lanIp());               out.print("\",");
  out.print("\"tunnel_ip\":\"");    out.print(gw.localTunnelIp());       out.print("\",");
  out.print("\"rssi_dbm\":");       out.print(m.rssi);                   out.print(",");
  out.print("\"handshakes\":");     out.print(m.handshakes);             out.print(",");
  out.print("\"last_handshake_ms\":"); out.print(m.lastHandshakeAgoMs);  out.print(",");
  out.print("\"tx_bytes\":");       out.print(m.txBytes);                out.print(",");
  out.print("\"rx_bytes\":");       out.print(m.rxBytes);                out.print(",");
  out.print("\"tx_rate_bps\":");    out.print(m.txRate);                 out.print(",");
  out.print("\"rx_rate_bps\":");    out.print(m.rxRate);                 out.print(",");
  out.print("\"uptime_ms\":");      out.print(m.uptimeMs);
  out.print("}");
}

void StatusServer::sendHtml(Stream& out) {
  // One self-contained page.  It polls /api/status every second and repaints.
  out.print(F(
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>R4 VPN Gateway</title><style>"
    "body{font:15px system-ui,sans-serif;background:#0b0f14;color:#e6edf3;margin:0;padding:24px}"
    ".card{max-width:560px;margin:0 auto;background:#11161d;border:1px solid #222c38;"
    "border-radius:14px;padding:24px}"
    "h1{font-size:18px;margin:0 0 4px}.sub{color:#8b97a7;font-size:12px;margin-bottom:18px}"
    ".dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:8px;vertical-align:middle}"
    ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:14px}"
    ".k{color:#8b97a7;font-size:12px}.v{font-size:16px;font-weight:600}"
    "code{background:#0b0f14;padding:2px 6px;border-radius:6px}"
    "</style></head><body><div class=card>"
    "<h1><span id=dot class=dot></span><span id=state>...</span></h1>"
    "<div class=sub>Arduino UNO R4 WiFi &middot; WireGuard gateway</div>"
    "<div class=grid>"
    "<div><div class=k>LAN address</div><div class=v><code id=lan>-</code></div></div>"
    "<div><div class=k>Tunnel address</div><div class=v><code id=tun>-</code></div></div>"
    "<div><div class=k>WiFi signal</div><div class=v id=rssi>-</div></div>"
    "<div><div class=k>Handshakes</div><div class=v id=hs>-</div></div>"
    "<div><div class=k>Down / Up</div><div class=v id=rate>-</div></div>"
    "<div><div class=k>Total RX / TX</div><div class=v id=tot>-</div></div>"
    "<div><div class=k>Tunnel uptime</div><div class=v id=up>-</div></div>"
    "<div><div class=k>Last handshake</div><div class=v id=lh>-</div></div>"
    "</div></div><script>"
    "const C={online:'#3fb950',handshaking:'#d29922','wifi-connecting':'#d29922',"
    "stalled:'#d29922','wifi-lost':'#f85149',error:'#f85149',boot:'#8b97a7'};"
    "function hb(b){if(b<1024)return b+' B';if(b<1048576)return (b/1024).toFixed(1)+' KB';"
    "return (b/1048576).toFixed(2)+' MB';}"
    "function hr(b){return hb(b)+'/s';}"
    "function ms(t){let s=Math.floor(t/1000);if(s<60)return s+'s';"
    "let m=Math.floor(s/60);s%=60;if(m<60)return m+'m '+s+'s';"
    "let h=Math.floor(m/60);m%=60;return h+'h '+m+'m';}"
    "async function tick(){try{let r=await fetch('/api/status');let d=await r.json();"
    "document.getElementById('state').textContent=d.state;"
    "document.getElementById('dot').style.background=C[d.state]||'#8b97a7';"
    "document.getElementById('lan').textContent=d.lan_ip;"
    "document.getElementById('tun').textContent=d.tunnel_ip;"
    "document.getElementById('rssi').textContent=d.rssi_dbm+' dBm';"
    "document.getElementById('hs').textContent=d.handshakes;"
    "document.getElementById('rate').textContent=hr(d.rx_rate_bps)+'  /  '+hr(d.tx_rate_bps);"
    "document.getElementById('tot').textContent=hb(d.rx_bytes)+'  /  '+hb(d.tx_bytes);"
    "document.getElementById('up').textContent=ms(d.uptime_ms);"
    "document.getElementById('lh').textContent=d.state=='online'?ms(d.last_handshake_ms)+' ago':'-';"
    "}catch(e){document.getElementById('state').textContent='unreachable';}}"
    "tick();setInterval(tick,1000);"
    "</script></body></html>"));
}

void StatusServer::handleClient(const VpnGateway& gw) {
#if STATUS_HAVE_WIFI && STATUS_SERVER_ENABLED
  if (!started_) return;
  WiFiClient client = server_.available();
  if (!client) return;

  // Read the request line only; we route on the path and ignore headers.
  String reqLine;
  uint32_t deadline = millis() + 1000;
  while (client.connected() && millis() < deadline) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') break;
      if (c != '\r') reqLine += c;
      if (reqLine.length() > 128) break;  // guard against junk
    }
  }
  // Drain the rest of the headers so the client is happy.
  while (client.available()) client.read();

  bool wantsJson = reqLine.indexOf("/api/status") >= 0;
  if (wantsJson) {
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n"));
    sendJson(client, gw);
  } else {
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                   "Connection: close\r\n\r\n"));
    sendHtml(client);
  }
  client.flush();
  delay(2);
  client.stop();
#else
  (void)gw;
#endif
}
