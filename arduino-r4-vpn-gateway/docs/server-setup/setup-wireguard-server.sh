#!/usr/bin/env bash
#
# setup-wireguard-server.sh
# ---------------------------------------------------------------------------
# Bootstraps a WireGuard server that the Arduino R4 VPN gateway dials into.
# Tested on Debian/Ubuntu. Run as root on a VPS or always-on home Linux box.
#
#   sudo ./setup-wireguard-server.sh
#
# It will:
#   1. install WireGuard
#   2. generate the server + Arduino key pairs
#   3. write /etc/wireguard/wg0.conf
#   4. enable IP forwarding and bring the tunnel up
#
# At the end it prints the two values you must paste into src/config.h:
#   WG_PRIVATE_KEY      (the Arduino's private key)
#   WG_PEER_PUBLIC_KEY  (the server's public key)
# ---------------------------------------------------------------------------
set -euo pipefail

WG_DIR=/etc/wireguard
WG_IF=wg0
SERVER_TUN_IP=10.6.0.1/24
ARDUINO_TUN_IP=10.6.0.2/32
LISTEN_PORT=51820

if [[ $EUID -ne 0 ]]; then
  echo "Please run as root (sudo $0)." >&2
  exit 1
fi

echo "==> Installing WireGuard..."
if command -v apt-get >/dev/null; then
  apt-get update -qq
  apt-get install -y wireguard qrencode >/dev/null
else
  echo "This helper assumes apt. Install 'wireguard' with your package manager, then re-run." >&2
  exit 1
fi

# Detect the default uplink interface for the NAT rule.
UPLINK=$(ip route show default | awk '/default/ {print $5; exit}')
echo "==> Detected uplink interface: ${UPLINK:-eth0}"
UPLINK=${UPLINK:-eth0}

mkdir -p "$WG_DIR"
chmod 700 "$WG_DIR"
cd "$WG_DIR"

echo "==> Generating keys..."
umask 077
wg genkey | tee server_private.key | wg pubkey > server_public.key
wg genkey | tee arduino_private.key | wg pubkey > arduino_public.key

SERVER_PRIV=$(cat server_private.key)
SERVER_PUB=$(cat server_public.key)
ARDUINO_PUB=$(cat arduino_public.key)
ARDUINO_PRIV=$(cat arduino_private.key)

echo "==> Writing ${WG_DIR}/${WG_IF}.conf..."
cat > "${WG_IF}.conf" <<EOF
[Interface]
Address    = ${SERVER_TUN_IP}
ListenPort = ${LISTEN_PORT}
PrivateKey = ${SERVER_PRIV}
PostUp   = iptables -A FORWARD -i %i -j ACCEPT; iptables -A FORWARD -o %i -j ACCEPT; iptables -t nat -A POSTROUTING -o ${UPLINK} -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -D FORWARD -o %i -j ACCEPT; iptables -t nat -D POSTROUTING -o ${UPLINK} -j MASQUERADE

[Peer]
# Arduino UNO R4 WiFi gateway
PublicKey  = ${ARDUINO_PUB}
AllowedIPs = ${ARDUINO_TUN_IP}
EOF
chmod 600 "${WG_IF}.conf"

echo "==> Enabling IP forwarding..."
sysctl -w net.ipv4.ip_forward=1 >/dev/null
grep -q "net.ipv4.ip_forward=1" /etc/sysctl.conf || echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf

echo "==> Bringing up ${WG_IF}..."
systemctl enable "wg-quick@${WG_IF}" >/dev/null 2>&1 || true
wg-quick down "${WG_IF}" >/dev/null 2>&1 || true
wg-quick up "${WG_IF}"

cat <<EOF

============================================================================
 WireGuard server is up on UDP ${LISTEN_PORT}.

 Paste these into arduino-r4-vpn-gateway/src/config.h:

   #define WG_PRIVATE_KEY     "${ARDUINO_PRIV}"
   #define WG_PEER_PUBLIC_KEY "${SERVER_PUB}"
   #define WG_ENDPOINT_HOST   "<this server's public hostname or IP>"
   #define WG_ENDPOINT_PORT   ${LISTEN_PORT}
   #define WG_LOCAL_IP        "10.6.0.2"

 Don't forget to forward/allow UDP ${LISTEN_PORT} to this host if it sits
 behind a firewall or home router.
============================================================================
EOF
