# 02 — Firmware Architecture

The internal design of the meter firmware: hardware context, layered module
architecture, the RTOS task model, boot/secure-boot flow, fault handling, and
memory/storage layout. The design is **single-image, variant-configurable**
(see [03](03-variant-configuration.md)) and built on the AWS IoT Device SDK
libraries in `../libraries/`.

---

## 1. Reference hardware context

A representative dual-MCU metering platform (the firmware is portable across
specific silicon via the `platform/` abstraction in this repo):

```mermaid
flowchart LR
    subgraph METROLOGY["Metrology Sub-system (sealed, MID/ANSI)"]
        AFE["Analog Front End<br/>(ΣΔ ADCs per phase)"]
        MMCU["Metrology MCU/DSP<br/>RMS, P/Q/S, harmonics"]
    end
    subgraph APP["Application Sub-system"]
        APMCU["Application MCU<br/>(Cortex-M / RISC-V, RTOS)"]
        SE["Secure Element<br/>(PKCS#11, X.509, keys)"]
        FLASH["External Flash<br/>(A/B slots, logs)"]
    end
    subgraph IO["I/O & Power"]
        RELAY["Service Relay<br/>(connect/disconnect)"]
        COMMS["Comms Module<br/>Cellular / Wi-Fi / PLC / RF"]
        RTC["RTC + Super-cap<br/>(last-gasp hold-up)"]
        DI["Tamper / DI / DO"]
    end

    AFE --> MMCU -->|sealed serial bus| APMCU
    APMCU --- SE
    APMCU --- FLASH
    APMCU --- RELAY
    APMCU --- COMMS
    APMCU --- RTC
    APMCU --- DI
```

The **metrology MCU** is sealed and certified for accuracy; it never touches the
network. The **application MCU** runs this firmware and the SDK, and treats
metrology reads as authenticated, read-only inputs.

---

## 2. Layered module architecture

```mermaid
flowchart TB
    subgraph A["Application Layer — variant-aware features"]
        A1[Metering & Registers]
        A2[Tariff / ToU Engine]
        A3[Net-Metering & DER]
        A4[Demand Response / V2G Controller]
        A5[Event & Power-Quality Log]
        A6[Service Disconnect Manager]
        A7[Local Display / HAN]
    end
    subgraph M["Middleware Layer — device services"]
        M1[Telemetry Manager]
        M2[Shadow Manager]
        M3[Jobs Agent]
        M4[OTA Agent]
        M5[Provisioning Agent]
        M6[Defender Agent]
        M7[Config & Variant Manager]
        M8[Time Sync / NTP]
    end
    subgraph C["Connectivity Layer — AWS IoT Device SDK"]
        C1[coreMQTT]
        C2[coreHTTP]
        C3[coreJSON]
        C4[backoffAlgorithm]
        C5[corePKCS11]
    end
    subgraph P["Platform Abstraction Layer (PAL)"]
        P1[Transport: TLS/Sockets]
        P2[OS: RTOS tasks/queues/timers]
        P3[Storage: KV / file / A-B slots]
        P4[Metrology HAL]
        P5[Crypto / Secure Element HAL]
        P6[Comms HAL: cell/wifi/plc/rf]
    end
    subgraph H["Hardware / RTOS"]
        H1[FreeRTOS or equivalent]
        H2[Drivers / BSP]
    end

    A --> M --> C --> P --> H
    M7 -. configures .-> A
```

**Layer contracts**

- **Application** depends only on Middleware interfaces; it is where variant
  behavior lives. It never calls the SDK directly.
- **Middleware** is the home of the *device services* — each maps to one AWS IoT
  capability and owns one SDK context. It is variant-agnostic; the Config &
  Variant Manager (M7) feeds it policy.
- **Connectivity** is the unmodified AWS IoT Device SDK. We add no protocol code
  here — we configure and call it.
- **PAL** is the only layer that knows the silicon. Porting = re-implementing PAL.
  This mirrors the existing `../platform/` directory of this repository.

---

## 3. Module responsibilities

### Application layer

| Module | Responsibility | Variant sensitivity |
|--------|----------------|--------------------|
| Metering & Registers | Maintain billing registers (kWh in/out, kVARh, kVA demand) from metrology reads | # phases, demand register on/off |
| Tariff / ToU Engine | Apply active rate schedule (flat/ToU/RTP/CPP), maintain per-rate registers | rate complexity, # registers |
| Net-Metering & DER | Four-quadrant accounting; separate import/export; PV/battery sub-metering | export accounting depth |
| DR / V2G Controller | Execute curtailment & V2G authorization windows; signal HEMS/EVSE | commercial: building DR; res: appliance DR |
| Event & PQ Log | Sags/swells, THD, frequency, tamper, reverse-energy alarms | PQ depth, harmonic order |
| Service Disconnect | Safe relay open/close with interlocks, remote + local | always present; commercial adds load-limit |
| Local Display / HAN | Optional LCD + Home-Area-Network interface | res: in-home display; comm: BACnet/Modbus |

### Middleware layer (device services)

| Module | SDK library it drives | Doc |
|--------|----------------------|-----|
| Telemetry Manager | `coreMQTT` + `coreJSON` | [05 §2](05-aws-iot-integration.md) |
| Shadow Manager | `device-shadow` | [05 §3](05-aws-iot-integration.md), [08](08-data-model-shadow.md) |
| Jobs Agent | `jobs` | [05 §4](05-aws-iot-integration.md), [07](07-ota-and-lifecycle.md) |
| OTA Agent | `ota` (+ `coreHTTP`, `corePKCS11`) | [07 §3](07-ota-and-lifecycle.md) |
| Provisioning Agent | `fleet-provisioning` | [07 §1](07-ota-and-lifecycle.md) |
| Defender Agent | `device-defender` | [06 §5](06-security-architecture.md) |
| Config & Variant Manager | reads Shadow; persists to PAL storage | [03](03-variant-configuration.md) |
| Time Sync | SNTP via PAL; disciplines RTC | needed for ToU & last-gasp timestamps |

---

## 4. RTOS task model

Tasks are prioritized so that **revenue accuracy and safety** preempt
connectivity, and connectivity preempts background management.

```mermaid
flowchart TB
    subgraph HIGH["High priority — deterministic"]
        T1["Metrology Acquisition Task<br/>(reads sealed MCU, updates registers)"]
        T2["Safety/Relay Task<br/>(disconnect interlocks, tamper)"]
        T3["Last-Gasp Task<br/>(armed by brown-out IRQ)"]
    end
    subgraph MED["Medium priority — connectivity"]
        T4["MQTT Agent Task<br/>(coreMQTT, keep-alive, in/out queues)"]
        T5["Telemetry Task<br/>(serialize + publish)"]
        T6["Shadow/Jobs Task<br/>(config + commands)"]
    end
    subgraph LOW["Low priority — background"]
        T7["OTA Agent Task"]
        T8["Defender Task"]
        T9["Logging / Diagnostics Task"]
        T10["Time-Sync Task"]
    end

    T1 --> T5
    T4 --> T5
    T4 --> T6
    T6 -->|trigger| T7
```

**Concurrency rules**

- A single **MQTT Agent task** owns the `coreMQTT` context; all other tasks
  enqueue publish/subscribe requests to it (thread-safe serialization). This is
  the pattern demonstrated by the `mqtt` demos in `../demos/mqtt/`.
- Metrology and safety tasks never block on the network. They write to
  power-fail-safe storage; the network layer reads asynchronously.
- `backoffAlgorithm` governs every reconnect/retry with jitter to prevent fleet
  thundering-herd after a regional outage.

---

## 5. Boot & secure-boot flow

```mermaid
sequenceDiagram
    participant ROM as Boot ROM
    participant BL as Secure Bootloader
    participant SE as Secure Element
    participant FW as Firmware (active slot)
    participant CFG as Config/Variant Mgr
    participant CLOUD as AWS IoT

    ROM->>BL: verify bootloader signature (ROT)
    BL->>SE: request image-verify key
    BL->>FW: verify active-slot signature
    alt signature invalid
        BL->>FW: roll back to known-good slot
    end
    BL->>FW: jump to firmware
    FW->>SE: open PKCS#11 session, load device cert
    FW->>CFG: load persisted variant + grid profile
    alt unprovisioned
        FW->>CLOUD: Fleet Provisioning (claim cert)
        CLOUD-->>FW: permanent cert + thing name
    end
    FW->>CLOUD: MQTT connect (mTLS)
    FW->>CLOUD: Shadow GET (reconcile desired config)
    CFG->>FW: activate variant feature set
    FW->>CLOUD: begin telemetry
```

Secure boot, A/B slots, and rollback are detailed in
[06 §1](06-security-architecture.md) and [07 §3](07-ota-and-lifecycle.md).

---

## 6. Memory & storage layout

| Region | Contents | Properties |
|--------|----------|-----------|
| Internal flash — bootloader | Secure bootloader, root-of-trust public key | immutable / write-locked |
| Internal flash — slot A | Firmware image A | signed, executable |
| Internal flash — slot B | Firmware image B (OTA target) | signed, swap on success |
| Secure element | Device private key, certs, counters | never leaves SE |
| External flash — config | Variant, grid profile, calibration, shadow cache | wear-leveled, CRC, A/B copies |
| External flash — registers | Billing registers, ToU buckets | **power-fail atomic** (journaled) |
| External flash — logs | Event log, PQ log, last-gasp queue | ring buffer, signed records |
| RTC-backed RAM | Time, last-gasp staging, demand window | super-cap retained |

Billing registers and the event log are **append-only and integrity-protected**
(per-record signature/MAC from the secure element) so that a compromised
application image cannot silently rewrite revenue history.

---

## 7. Fault handling & resilience

- **Network loss** → telemetry buffered in the log ring (store-and-forward);
  flushed on reconnect with original timestamps. `backoffAlgorithm` paces retries.
- **Power loss** → brown-out IRQ arms the Last-Gasp task; super-cap holds up long
  enough to publish a final outage message (or hand it to the DCU).
- **Bad config** → Config Manager validates against a schema before activation;
  invalid desired-state is rejected and reported back via the Shadow `reported`
  document, never applied.
- **OTA failure** → A/B slots + watchdog-gated "first-boot health check"; a new
  image that fails to check in within N minutes triggers automatic rollback.
- **Clock loss** → records flagged `time_unsynced`; ToU falls back to last known
  schedule; MDMS estimates per standard rules.

Continue to [03 — Variant Configuration](03-variant-configuration.md).
