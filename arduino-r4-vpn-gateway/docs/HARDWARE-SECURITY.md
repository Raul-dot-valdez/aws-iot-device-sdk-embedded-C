# Hardware Security Charter — the cheapest *trustworthy* VPN box on the RA4M1

A working document for the goal of making the Arduino UNO R4 WiFi (Renesas
**RA4M1**) the lowest-cost VPN endpoint that is still genuinely hard to tamper
with. It is written so a focused group could pick it up and execute. It is
deliberately **honest**: it leans on real silicon features and is explicit about
what this class of chip cannot promise.

> ⚠️ Verify every hardware specific below against the **RA4M1 Group User's
> Manual** (Renesas) and the RA Flexible Software Package (FSP) before relying on
> it. Datasheet wins over this document.

## Mission

> A sub-$30, solder-free WireGuard endpoint whose **private key cannot be
> trivially extracted**, whose **firmware cannot be silently replaced**, and
> whose security posture is **transparent and auditable** — using only the
> RA4M1's on-chip protections plus, optionally, a ~$1 external part.

## The RA4M1, honestly

What it **is**: an Arm **Cortex-M4F** @ 48 MHz, ~256 KB code flash, 8 KB data
flash (EEPROM emulation), 32 KB SRAM. On the UNO R4 WiFi it is paired with a
separate **ESP32-S3** that provides WiFi via Arduino's network firmware.

What it is **not**: it has **no Arm TrustZone** (that is Cortex-M23/M33, e.g.
RA4M2/RA6M4), and it is **not a certified secure element**. It has no
defence-grade countermeasures against fault injection or power/EM side-channel
analysis. "Cheap but safe" here means *raising the bar against remote and casual
physical attackers*, not defeating a funded lab.

**Architecture note that matters:** decide *where the tunnel crypto and the
private key live* — the RA4M1, or the ESP32-S3. The protections below apply to
the RA4M1; if the tunnel runs on the ESP32-S3 instead, the analogous levers are
that chip's **flash encryption** and **secure boot**. Keeping the secret and the
crypto on one chip, with that chip locked down, is the cleanest story.

## Five expert workstreams (the "five groups")

Each is a self-contained track a sub-team could own.

### 1. Key storage & device identity
- Generate the device key on a trusted host; the **public** key goes to the
  server, the **private** key never leaves the device unprotected.
- Today: keep it out of git via `secrets.h` (done). Next: store it in **data
  flash**, not code flash, and gate access (workstreams 3–4).
- Use the RA4M1 factory **128-bit Unique ID** as a non-secret device identifier
  for inventory/attestation (it is readable, so never as a secret/key).

### 2. Boot & firmware integrity
- Goal: the box only runs firmware you signed. The RA4M1 has no hardware secure
  boot, so this is a **measured/verified boot in the bootloader**: hash the app,
  check a signature using a public key fixed in protected flash, refuse on
  mismatch.
- Pair with the **Flash Access Window (FAW)** so the verified region cannot be
  silently reprogrammed in the field.

### 3. Debug-port & flash lockdown (anti-readout) — highest ROI
- The single biggest cheap win: **lock the debug interface**. An open SWD/JTAG
  (or serial boot mode) lets anyone dump flash and read the private key.
- Set the RA4M1 **ID-code / OCD protection** (the OSIS/ID-code option bytes) to
  require a secret to attach a debugger — or to disable debug entirely for
  production units.
- Apply the **Flash Access Window (FAW)** to block erase/program of protected
  blocks, and the **Security MPU** (where available on RA4M1) to hide the
  key/crypto region from the debugger and bus masters (DMA).
- *Trade-off to document loudly:* a permanently locked debug port means a
  bricked unit can't be recovered. Provide a clear "development vs. production"
  build switch.

### 4. Crypto correctness & entropy
- Don't roll your own crypto — use a vetted WireGuard/Noise implementation
  (Curve25519, ChaCha20-Poly1305, BLAKE2s).
- **Entropy is the classic embedded failure.** Seed key/nonce generation from a
  hardware RNG; confirm the RA4M1's on-chip RNG/crypto peripheral in the manual
  and use the FSP driver rather than `random()`. Never derive keys from
  `millis()`/`analogRead()` noise alone.
- Add the optional **pre-shared key** layer (`WG_PRESHARED_KEY`) for
  harvest-now-decrypt-later resistance.

### 5. Physical & supply-chain resilience
- Threat-model the cheap physical device (below) and document residual risk
  honestly.
- Supply chain: reproducible builds, pinned toolchain/libraries, the CI
  secret-scan (done), and signed releases so a flashed binary is traceable.
- Consider an **external secure element** (next section) where the threat model
  justifies the extra part.

## Optional: a ~$1 external secure element (with honest caveats)

A Microchip **ATECC608** over the board's **Qwiic/I²C** connector adds
tamper-resistant key storage for well under a dollar — *but read the caveat*:

- It does **NIST P-256 (secp256r1) ECC**, **not** WireGuard's Curve25519/X25519.
  So it **cannot offload WireGuard's handshake math.** It is excellent for
  P-256-based mutual-TLS identity, attestation, or as **encrypted-at-rest
  storage** for an arbitrary secret, but the X25519 operations still run on the
  RA4M1 (the key touches RAM in use).
- Net: a secure element meaningfully helps if you adopt a P-256 identity layer or
  use it purely for sealed key storage; it is **not** a drop-in "WireGuard in
  hardware." Don't oversell it.

For most builds, **workstream 3 (lock the debug port + FAW + Security MPU)**
delivers the best safety-per-dollar because it costs nothing and directly blocks
key readout.

## Threat model for a cheap physical device

| Attacker | Mitigation here | Residual risk |
|---|---|---|
| Remote (network) | WireGuard mutual auth + encryption; locked, read-only status page | metadata/availability |
| Casual physical (plugs in a debugger) | ID-code lock + FAW + Security MPU block flash readout | — |
| Determined physical (desolders, glitches, side-channel) | out of scope for this silicon | **key extraction possible** — rotate keys if a unit is lost |
| Supply chain (tampered binary) | signed/reproducible builds, secret-scan CI | trust in build host |

## Roadmap

1. **P0 (free, do first):** lock debug/ID-code + Flash Access Window on
   production units; hardware-RNG-seeded key handling; key in data flash.
2. **P1:** verified boot in the bootloader; signed release artifacts.
3. **P2:** optional ATECC608 P-256 identity/attestation layer over Qwiic.
4. **P3:** published threat model + third-party review; reproducible-build
   attestation.

## Contributing / references

- Renesas **RA4M1 Group User's Manual** and the **RA FSP** (security drivers:
  Flash, MPU/Security MPU, RNG/crypto, option-setting memory).
- WireGuard whitepaper and the Noise protocol framework.
- Arm Cortex-M4 MPU documentation (Armv7-M PMSA).

This charter is intentionally falsifiable: if a claim here doesn't match the
RA4M1 manual, the manual is right — open an issue and fix the doc.
