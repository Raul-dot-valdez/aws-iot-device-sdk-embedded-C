#!/usr/bin/env bash
#
# harden-server.sh
# ---------------------------------------------------------------------------
# Baseline 2026 hardening for the Linux box that runs the WireGuard server.
# Run AFTER setup-wireguard-server.sh, as root, on Debian/Ubuntu.
#
#   sudo ./harden-server.sh
#
# It applies conservative, widely-recommended defaults:
#   - a default-deny firewall that allows only SSH + the WireGuard UDP port
#   - fail2ban to throttle SSH brute-force
#   - unattended automatic security updates
#   - disables SSH password authentication (key-only login)
#
# Review each section before running on a server you depend on. The SSH
# password-auth change is guarded so you don't lock yourself out.
# ---------------------------------------------------------------------------
set -euo pipefail

WG_PORT="${WG_PORT:-51820}"
SSH_PORT="${SSH_PORT:-22}"

if [[ $EUID -ne 0 ]]; then
  echo "Please run as root (sudo $0)." >&2
  exit 1
fi
if ! command -v apt-get >/dev/null; then
  echo "This helper assumes apt (Debian/Ubuntu)." >&2
  exit 1
fi

echo "==> Installing ufw, fail2ban, unattended-upgrades..."
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y ufw fail2ban unattended-upgrades >/dev/null

echo "==> Configuring firewall (default deny inbound; allow SSH + WireGuard)..."
ufw --force reset >/dev/null
ufw default deny incoming
ufw default allow outgoing
ufw allow "${SSH_PORT}/tcp" comment 'SSH'
ufw allow "${WG_PORT}/udp" comment 'WireGuard'
ufw --force enable
ufw status verbose

echo "==> Enabling automatic security updates..."
cat > /etc/apt/apt.conf.d/20auto-upgrades <<'EOF'
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
EOF
systemctl enable --now unattended-upgrades >/dev/null 2>&1 || true

echo "==> Enabling fail2ban (SSH jail)..."
systemctl enable --now fail2ban >/dev/null 2>&1 || true

echo "==> Hardening SSH (disable password auth — key login only)..."
# Only do this if at least one authorized key exists, to avoid lockout.
if grep -rqs . /root/.ssh/authorized_keys /home/*/.ssh/authorized_keys 2>/dev/null; then
  sed -i 's/^#\?PasswordAuthentication .*/PasswordAuthentication no/' /etc/ssh/sshd_config
  sed -i 's/^#\?ChallengeResponseAuthentication .*/ChallengeResponseAuthentication no/' /etc/ssh/sshd_config
  systemctl reload ssh 2>/dev/null || systemctl reload sshd 2>/dev/null || true
  echo "    Password authentication disabled."
else
  echo "    SKIPPED: no SSH authorized_keys found — add a key first, then re-run,"
  echo "    or you could lock yourself out."
fi

cat <<EOF

============================================================================
 Hardening applied.
   - Firewall: only ${SSH_PORT}/tcp (SSH) and ${WG_PORT}/udp (WireGuard) open
   - fail2ban: active
   - Automatic security updates: enabled
   - SSH: password auth disabled if a key was present
 Reminder: keep WireGuard and the OS patched, and rotate keys periodically.
============================================================================
EOF
