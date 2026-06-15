# GD32W515 Port (reference)

Concrete instantiation of the HAL for the **GigaDevice GD32W515** (Cortex-M33,
TrustZone, integrated Wi-Fi). See
[`../../12-gigadevice-gd32w515-platform.md`](../../12-gigadevice-gd32w515-platform.md).

[`gd32w515_port.h`](gd32w515_port.h) declares the factory functions that return
the portable `*Interface_t` structs from [`../../include/hal/`](../../include/hal).
It is **declaration-only** and self-contained, so it compiles in CI without the
vendor SDK; the implementations (`.c`) belong in the firmware tree and call into
the GD32W51x SDK.

## Port bindings

| HAL factory | GD32W51x SDK backing |
|-------------|----------------------|
| `GD32W515_MetrologyPort()` | SPI/UART driver → sealed metrology AFE |
| `GD32W515_CryptoPort()` | TF-M PSA Crypto + HW AES/HASH/PKCAU/TRNG; key in Secure Storage/eFuse |
| `GD32W515_StoragePort()` | QSPI NOR driver (A/B slots, journaled registers) + TF-M Secure Storage |
| `GD32W515_ActuatorPort()` | GPIO/timer (relay, load-limit) + UART (IEC 62056-21 optical port) |

Transport for `coreMQTT`/`coreHTTP` is mbedTLS over lwIP on the integrated Wi-Fi
driver — the same shape as `../../../../platform/posix/transport/`, with POSIX
sockets replaced by lwIP/Wi-Fi and software PKCS#11 replaced by TF-M PSA.

## Second-source seam

Swapping to another Cortex-M33 part (per [09 §2](../../09-hardware-platform.md))
replaces only this `ports/<mcu>/` directory; nothing above the HAL changes.
