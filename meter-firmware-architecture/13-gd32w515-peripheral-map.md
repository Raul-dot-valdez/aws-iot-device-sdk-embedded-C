# 13 — GD32W515 Peripheral & Pin Map

A board-integration map for the **GigaDevice GD32W515** application core
([12](12-gigadevice-gd32w515-platform.md)): which on-chip peripheral serves each
meter function, the signal direction, and the security domain it belongs to.
This binds the HAL ports of [`reference-src/`](reference-src/) to real silicon
resources.

> Pin labels (PAx/PBx/PCx) are **representative**. The GD32W515 is offered in
> small packages (the Wi-Fi RF occupies die/pins), so confirm the exact pinout
> and peripheral instances against the GD32W515 datasheet and the package you
> select (GD32W515P/T/M) before laying out the board.

---

## 1. Functional block diagram

```mermaid
flowchart TB
    subgraph GD["GD32W515 (Cortex-M33)"]
        SPI0["SPI0"]
        USART0["USART0"]
        USART1["USART1"]
        USART2["USART2"]
        I2C0["I2C0"]
        QSPI["QSPI"]
        GPIOA["GPIO (relay/tamper/pfail)"]
        ADC["ADC / comparator"]
        RTC["RTC + tamper"]
        WIFI["Wi-Fi 4 (internal RF)"]
        SWD["SWD (secure-gated)"]
    end

    AFE["Sealed Metrology AFE<br/>(ADE9000 / MSP430)"] <-->|SPI0 + IRQ| SPI0
    OPT["IR optical head<br/>IEC 62056-21"] <-->|USART0 (IrDA)| USART0
    RS485["RS-485 xcvr<br/>BACnet/Modbus (commercial)"] <-->|USART1| USART1
    PLC["PLC/RF modem<br/>(concentrated topology)"] <-->|USART2 / SPI| USART2
    DISP["Local display / HAN"] <-->|I2C0| I2C0
    NOR["External QSPI NOR<br/>(A/B slots, logs, XIP)"] <-->|QSPI| QSPI
    RELAY["Service relay driver<br/>+ position feedback"] <-->|GPIO| GPIOA
    TAMPER["Tamper switches<br/>(cover/magnet/terminal)"] -->|GPIO/RTC-TAMP| GPIOA
    PFAIL["Brown-out detect<br/>(super-cap rail)"] -->|comparator IRQ| ADC
    SE["(optional) discrete SE"] <-->|I2C0| I2C0
    ANT["Wi-Fi antenna"] --- WIFI
    DBG["Debug probe"] -.->|gated| SWD
```

---

## 2. Peripheral allocation table

| Meter function | GD32W515 peripheral | Signals (representative) | Dir | Security domain | HAL port |
|----------------|---------------------|--------------------------|-----|-----------------|----------|
| Sealed metrology link | **SPI0** + IRQ | PA5 SCK, PA6 MISO, PA7 MOSI, PA4 NSS, PB0 IRQ, PB1 zero-cross | ⇄ | Secure (App RoT reads) | `metrology_hal.h` |
| IR / optical port | **USART0** (IrDA mode) | PA9 TX, PA10 RX | ⇄ | Non-secure (local) | `actuator_hal.h` (opticalPoll) |
| RS-485 fieldbus (commercial) | **USART1** + DE | PB6 TX, PB7 RX, PB5 DE | ⇄ | Non-secure | comms (HAN/BEMS) |
| PLC/RF modem (concentrated) | **USART2** or SPI | PC10/PC11 or shared SPI | ⇄ | Non-secure | comms (alt backhaul) |
| Local display / HAN / opt. SE | **I2C0** | PB8 SCL, PB9 SDA | ⇄ | Mixed (SE = secure) | display / crypto |
| External firmware/log flash | **QSPI** | PB2 CLK, PB10 NCS, PE7–PE10 IO0–3 | ⇄ | Secure (FWU) + NS (logs) | `storage_hal.h` |
| Service relay drive + feedback | **GPIO** | PC0 drive, PC1 latch, PC2 position | ⇄ | **Secure (Actuator Guard)** | `actuator_hal.h` (setRelay) |
| Load-limit contactor (commercial) | **GPIO**/TIMER | PC3 | → | **Secure (Actuator Guard)** | `actuator_hal.h` (setLoadLimit) |
| Tamper inputs | **GPIO + RTC TAMP** | PA0 cover, PA1 magnetic, PA2 terminal, RTC_TAMP0 | → | **Secure (Tamper Monitor)** | tamper service |
| Power-fail / last-gasp | **Comparator/ADC** IRQ | PA3 (super-cap rail sense) | → | Secure-aware | last-gasp task |
| Real-time clock | **RTC** + LSE 32.768 kHz | OSC32_IN/OUT, super-cap backup | — | Secure (time integrity) | clock |
| Wi-Fi | **Integrated RF** | antenna + matching | ⇄ | Non-secure (lwIP/mbedTLS) | transport |
| Debug | **SWD** | SWDIO/SWCLK | ⇄ | **Gated by secure boot / lifecycle** | n/a |

Legend: ⇄ bidirectional, → input/output to the MCU.

---

## 3. Security-domain rationale

The **secure-domain** column is the important part for the cybersecurity model
([14](14-tfm-secure-partition-layout.md)):

- **Relay, load-limit, and tamper** are owned by **secure-world** services. The
  non-secure application can *request* an operation but cannot toggle the GPIOs
  directly — the GD32W515's GPIO security attribution (TrustZone SAU/peripheral
  protection) routes these pins to the secure side. This realizes the
  control-path protection of [06 §6](06-security-architecture.md) in hardware.
- **Metrology SPI** is read by a secure App-RoT service that verifies the AFE
  seal before the value is exposed to the non-secure app — so revenue data is
  attested at the trust boundary.
- **QSPI flash** is split: the **firmware-update** region is secure (PSA FWU +
  anti-rollback), while the **log/telemetry-cache** region is non-secure but
  every record is integrity-tagged by the secure Crypto service.
- **SWD debug** is gated by the device lifecycle/secure-boot state — open in
  manufacturing, locked (or authenticated-only) in the field, per IEC 62443 /
  IEEE 1686 requirements.

---

## 4. Clocking, power & integrity notes

- **LSE 32.768 kHz** crystal with **super-cap backup** keeps the RTC alive for
  ToU accuracy and last-gasp timestamps ([02 §6](02-firmware-architecture.md)).
- The **brown-out comparator** arms the high-priority last-gasp task; the
  super-cap holds the rail long enough to publish the final outage message.
- The **RTC tamper inputs** latch tamper events even across resets and feed the
  secure Tamper Monitor, which writes signed records to the audit log.

Continue to [14 — TF-M Secure-Partition Layout](14-tfm-secure-partition-layout.md).
