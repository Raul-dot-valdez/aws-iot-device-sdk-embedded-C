# Reference Interface Skeleton

Compile-checkable C contracts that make the layered design in
[`../02-firmware-architecture.md`](../02-firmware-architecture.md) concrete.

> **This is not the firmware.** It is the set of *interface boundaries* — the
> HAL ports and the middleware device-service prototypes — turned into real,
> buildable headers so the architecture's seams are unambiguous and can be
> code-reviewed and CI-checked. Function bodies in `build-check/` are trivial
> stubs that exist only to prove the contracts link as a consistent set.

## Layout

```
reference-src/
├── include/
│   ├── meter_types.h              # shared status/variant/feature types
│   ├── hal/                       # Hardware Abstraction Layer (function-pointer
│   │   ├── metrology_hal.h        #   contracts, mirroring the SDK's
│   │   ├── crypto_hal.h           #   TransportInterface_t pattern - the port
│   │   ├── storage_hal.h          #   seam / second-source boundary, see 09 §3)
│   │   └── actuator_hal.h
│   └── services/                  # Middleware device services (the firmware's
│       ├── config_variant.h       #   own modules; map 1:1 to 05 §1)
│       ├── dr_v2g_controller.h
│       └── telemetry_service.h
├── build-check/build_check.c      # includes everything + a wiring sanity main()
└── Makefile                       # `make check` to compile + run
```

## What maps to what

| Header | Architecture reference |
|--------|------------------------|
| `hal/metrology_hal.h` | Sealed metrology core, four-quadrant ([02 §1](../02-firmware-architecture.md), [04 §2](../04-ev-grid-complexity.md)) |
| `hal/crypto_hal.h` | Secure element / PKCS#11, revenue signing ([06](../06-security-architecture.md)) |
| `hal/storage_hal.h` | Power-fail-atomic register journaling ([02 §6](../02-firmware-architecture.md)) |
| `hal/actuator_hal.h` | Relay, load-limit, IR/optical port ([06 §6](../06-security-architecture.md), [09 §4](../09-hardware-platform.md)) |
| `services/config_variant.h` | Variant resolver + feature matrix ([03](../03-variant-configuration.md)) |
| `services/dr_v2g_controller.h` | DR/V2G envelope execution ([04 §3–4](../04-ev-grid-complexity.md)) |
| `services/telemetry_service.h` | Serialize/sign/publish telemetry ([05 §2](../05-aws-iot-integration.md), [08](../08-data-model-shadow.md)) |

## Design rules demonstrated

- **HAL = function-pointer structs** (`*Interface_t`) exactly like the SDK's
  `TransportInterface_t`, so portable code never references silicon. Porting to a
  new MCU re-implements only these — the second-source seam of [09 §3](../09-hardware-platform.md).
- **Services take their dependencies by pointer** (HALs + config), so they are
  unit-testable against mocks — the [11 §2](../11-testing-validation-certification.md)
  Unity/CMock approach, no hardware in CI.
- **Validation before apply** is encoded in the prototypes (`Config_Validate`
  before `Config_Apply`, `Dr_Validate` before `Dr_Apply`) — bad config / unsafe
  commands are rejected, never applied ([03 §6](../03-variant-configuration.md),
  [06 §6](../06-security-architecture.md)).

## Build

```sh
make check        # compiles with -Wall -Wextra -Wpedantic and runs the wiring check
```

Expected output:

```
build-check OK: variant=0 evReady=1 v2g=1 compliant@5kW=1
```
