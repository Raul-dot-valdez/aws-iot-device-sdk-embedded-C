#ifndef PAPR_CONFIG_H
#define PAPR_CONFIG_H

/* Compile-time configuration for the PAPR controller.
 * Values target a typical 32-bit ARM Cortex-M MCU running at 64-180 MHz.
 * Override any define from the build system (-D...) to retune for hardware. */

/* Control loop period in milliseconds. The main supervisor runs at this rate. */
#ifndef PAPR_CONTROL_PERIOD_MS
#define PAPR_CONTROL_PERIOD_MS          10U
#endif

/* Sensor sampling period in milliseconds. */
#ifndef PAPR_SENSOR_PERIOD_MS
#define PAPR_SENSOR_PERIOD_MS           20U
#endif

/* Airflow setpoints in litres-per-minute for the three user-selectable levels.
 * EN 12941 minimum design flow for TH3 class is 160 L/min. */
#ifndef PAPR_FLOW_LOW_LPM
#define PAPR_FLOW_LOW_LPM               170U
#endif
#ifndef PAPR_FLOW_MED_LPM
#define PAPR_FLOW_MED_LPM               195U
#endif
#ifndef PAPR_FLOW_HIGH_LPM
#define PAPR_FLOW_HIGH_LPM              220U
#endif

/* Minimum acceptable instantaneous flow before the low-flow alarm fires. */
#ifndef PAPR_FLOW_ALARM_LPM
#define PAPR_FLOW_ALARM_LPM             150U
#endif

/* Time the flow must stay below the threshold before the alarm latches. */
#ifndef PAPR_FLOW_ALARM_DEBOUNCE_MS
#define PAPR_FLOW_ALARM_DEBOUNCE_MS     2000U
#endif

/* Battery thresholds in millivolts for a 4-cell Li-ion pack (14.4 V nominal). */
#ifndef PAPR_BATT_FULL_MV
#define PAPR_BATT_FULL_MV               16800
#endif
#ifndef PAPR_BATT_LOW_MV
#define PAPR_BATT_LOW_MV                13200
#endif
#ifndef PAPR_BATT_CRITICAL_MV
#define PAPR_BATT_CRITICAL_MV           12600
#endif
#ifndef PAPR_BATT_CUTOFF_MV
#define PAPR_BATT_CUTOFF_MV             12000
#endif

/* Filter loading is inferred from the controller demand required to hit the
 * setpoint. If the demand saturates above this fraction (in tenths of percent)
 * for the debounce interval, the filter-clog alarm fires. */
#ifndef PAPR_FILTER_CLOG_DUTY_PERMILLE
#define PAPR_FILTER_CLOG_DUTY_PERMILLE  950U
#endif
#ifndef PAPR_FILTER_CLOG_DEBOUNCE_MS
#define PAPR_FILTER_CLOG_DEBOUNCE_MS    3000U
#endif

/* PID gains for the airflow control loop. Output units are motor current in
 * milliamps (see PAPR_L6235_IMAX_MA below). Q16.16 fixed-point. */
#ifndef PAPR_PID_KP_Q16
#define PAPR_PID_KP_Q16                 (12 << 16)
#endif
#ifndef PAPR_PID_KI_Q16
#define PAPR_PID_KI_Q16                 (3 << 16)
#endif
#ifndef PAPR_PID_KD_Q16
#define PAPR_PID_KD_Q16                 (1 << 16)
#endif

/* ---- L6235 brushless DC driver --------------------------------------------
 * The L6235 is a fully integrated three-phase DMOS driver with internal Hall
 * decoding and constant-tOFF PWM current regulation. The MCU sets the peak
 * phase current via VREF (analog input) and toggles enable / direction / brake
 * GPIOs. Speed feedback comes from the TACHO open-drain output. Faults
 * (overcurrent, thermal shutdown) are signalled on DIAG (open-drain, low). */

/* Sense resistor on the SENSE pin, milliohms. Datasheet recommends 0.3 Ω
 * giving ≈ 2.8 A peak at VREF = 0.84 V (peak limited by package thermals). */
#ifndef PAPR_L6235_RSENSE_MOHM
#define PAPR_L6235_RSENSE_MOHM          300U
#endif

/* Maximum VREF voltage the MCU is allowed to drive into the L6235 VREF pin,
 * in millivolts. Below the L6235 absolute max of 7 V; sized for the analog
 * front-end the MCU can produce (typ. 3.0 V from a DAC or filtered PWM). */
#ifndef PAPR_L6235_VREF_MAX_MV
#define PAPR_L6235_VREF_MAX_MV          2500U
#endif

/* Peak motor phase current that corresponds to PAPR_L6235_VREF_MAX_MV.
 * I_peak[A] = VREF[V] / RSENSE[Ω].  2500 mV / 0.3 Ω ≈ 8.3 A — capped here
 * to 2500 mA to stay within the L6235 continuous rating with adequate
 * heatsinking and to protect the blower windings. */
#ifndef PAPR_L6235_IMAX_MA
#define PAPR_L6235_IMAX_MA              2500U
#endif

/* Minimum current to keep the rotor commutating cleanly during light load. */
#ifndef PAPR_L6235_IMIN_MA
#define PAPR_L6235_IMIN_MA              150U
#endif

/* TACHO output: number of pulses per mechanical revolution. The L6235 emits
 * one TACHO edge per Hall transition, so 6 × pole pairs per revolution. */
#ifndef PAPR_L6235_TACHO_PPR
#define PAPR_L6235_TACHO_PPR            12U   /* 2 pole-pair motor */
#endif

/* DIAG debounce. The pin can twitch during commutation; ignore pulses shorter
 * than this before declaring a motor fault. */
#ifndef PAPR_L6235_DIAG_DEBOUNCE_MS
#define PAPR_L6235_DIAG_DEBOUNCE_MS     5U
#endif

/* Resolution of the VREF DAC channel driving the L6235. 12-bit is plenty for
 * smooth current control at the chip's tens-of-kHz internal PWM. */
#ifndef PAPR_L6235_VREF_DAC_MAX
#define PAPR_L6235_VREF_DAC_MAX         4095U
#endif

/* ---- Sensirion SDP810-500Pa differential pressure sensor ------------------
 * 4-pin tube-connection variant. I²C interface, 7-bit address 0x25 by default,
 * ±500 Pa range, scale factor 60 LSB/Pa, on-chip temperature compensation. */

#ifndef PAPR_SDP810_I2C_ADDR
#define PAPR_SDP810_I2C_ADDR            0x25U
#endif

/* Scale factor for the −500..+500 Pa variant (datasheet table 6). */
#ifndef PAPR_SDP810_SCALE
#define PAPR_SDP810_SCALE               60
#endif

/* Time the chip needs after a "start continuous measurement" command before
 * the first 9-byte read can succeed. The datasheet quotes 8 ms typ; round up. */
#ifndef PAPR_SDP810_STARTUP_DELAY_MS
#define PAPR_SDP810_STARTUP_DELAY_MS    20U
#endif

/* Hard limit on how often we issue the start command. Once continuous mode
 * is running the chip refreshes its internal registers every 0.5 ms, so the
 * sensors module just reads the latest 9-byte snapshot. */
#ifndef PAPR_SDP810_RESTART_PERIOD_MS
#define PAPR_SDP810_RESTART_PERIOD_MS   60000U
#endif

/* ---- GD32VW553-UNIFI-EMH7 BLE module --------------------------------------
 * UART-attached pre-certified module hosting GigaDevice's GD32VW553 wireless
 * SoC (RISC-V, BLE 5.2). The host MCU exchanges framed binary packets with
 * the module, which proxies them as GATT notifications / writes to the
 * paired mobile app. */

/* UART baud rate (module default 115200 8N1). */
#ifndef PAPR_BLE_UART_BAUD
#define PAPR_BLE_UART_BAUD              115200U
#endif

/* Telemetry transmission cadence over BLE. */
#ifndef PAPR_BLE_TELEM_PERIOD_MS
#define PAPR_BLE_TELEM_PERIOD_MS        500U
#endif

/* Largest payload a single frame may carry. Sized to fit an OTA data chunk
 * (4-byte offset + image bytes) inside one BLE 5.x DLE PDU. */
#ifndef PAPR_BLE_MAX_PAYLOAD
#define PAPR_BLE_MAX_PAYLOAD            128U
#endif

/* Frame-sync bytes. Two-byte preamble keeps the module's UART from
 * mistakenly accepting log noise as a command. */
#define PAPR_BLE_SYNC0                  0xAAU
#define PAPR_BLE_SYNC1                  0x55U

/* ---- Switch matrix (3x3) --------------------------------------------------
 * Six GPIOs scan a 9-key matrix. Rows are driven low one at a time; an
 * active column reads low through the depressed key. */

#ifndef PAPR_KEYPAD_ROWS
#define PAPR_KEYPAD_ROWS                3U
#endif

#ifndef PAPR_KEYPAD_COLS
#define PAPR_KEYPAD_COLS                3U
#endif

/* Period between full scans of the matrix. */
#ifndef PAPR_KEYPAD_SCAN_PERIOD_MS
#define PAPR_KEYPAD_SCAN_PERIOD_MS      8U
#endif

/* Number of consecutive scans the same state must be observed before a
 * press / release event is emitted. */
#ifndef PAPR_KEYPAD_DEBOUNCE_SCANS
#define PAPR_KEYPAD_DEBOUNCE_SCANS      3U
#endif

/* Key-down duration that promotes a PRESS into a LONG_PRESS event. */
#ifndef PAPR_KEYPAD_LONG_PRESS_MS
#define PAPR_KEYPAD_LONG_PRESS_MS       1200U
#endif

/* Capacity of the FIFO that the keypad uses to hand events off to the
 * supervisor. */
#ifndef PAPR_KEYPAD_QUEUE_LEN
#define PAPR_KEYPAD_QUEUE_LEN           8U
#endif

/* ---- Energy-saving / adaptive comfort algorithm ---------------------------
 * Watches the SDP810 differential-pressure stream to estimate breathing rate
 * and intensity, then nudges the flow setpoint within the user-allowed range
 * to extend battery life without dropping below the safety floor. */

/* Period at which the energy module samples telemetry to feed its history
 * buffer. Choose around the breathing-rate Nyquist (10 Hz is plenty for
 * 12-30 breaths/min). */
#ifndef PAPR_ENERGY_SAMPLE_PERIOD_MS
#define PAPR_ENERGY_SAMPLE_PERIOD_MS    100U
#endif

/* Window over which the breathing detector averages dP samples. */
#ifndef PAPR_ENERGY_WINDOW_SAMPLES
#define PAPR_ENERGY_WINDOW_SAMPLES      32U
#endif

/* Minimum amplitude (Pa) of a dP excursion before it is counted as a
 * breath. Smaller values are treated as quiet/static signal. */
#ifndef PAPR_ENERGY_BREATH_THRESHOLD_PA
#define PAPR_ENERGY_BREATH_THRESHOLD_PA 8
#endif

/* Hold-off between auto-level transitions so the user does not feel the
 * blower hunt. */
#ifndef PAPR_ENERGY_LEVEL_HOLD_MS
#define PAPR_ENERGY_LEVEL_HOLD_MS       30000U
#endif

/* Breaths-per-minute thresholds for the auto level decision. */
#ifndef PAPR_ENERGY_BPM_LIGHT
#define PAPR_ENERGY_BPM_LIGHT           12U
#endif
#ifndef PAPR_ENERGY_BPM_HEAVY
#define PAPR_ENERGY_BPM_HEAVY           24U
#endif

/* Nominal battery pack capacity in mAh. Used to convert SoC into a runtime
 * estimate. Override to match the actual pack. */
#ifndef PAPR_ENERGY_PACK_MAH
#define PAPR_ENERGY_PACK_MAH            6000U
#endif

/* Smoothing factor (out of 256) used by the EWMA that tracks the user's
 * typical session length across boots. Higher = more reactive. */
#ifndef PAPR_ENERGY_SESSION_EWMA_NUM
#define PAPR_ENERGY_SESSION_EWMA_NUM    32U
#endif

/* Watchdog kick interval. Must be shorter than the hardware watchdog window. */
#ifndef PAPR_WDT_KICK_MS
#define PAPR_WDT_KICK_MS                100U
#endif

/* ---- OTA firmware update --------------------------------------------------
 * Images are streamed over BLE into the inactive application flash slot,
 * verified by CRC-32, then a boot flag is set and the MCU resets so a
 * (separate) bootloader runs the new image. Updates are only accepted while
 * the device is idle (STANDBY) — never while the worker is breathing through
 * the unit. See ARCHITECTURE.md "OTA flash map" for the slot layout. */

/* Usable capacity of the staging slot (inactive app bank). The reference
 * GD32E517RE map reserves 32 KB bootloader + 2 x 240 KB app slots. */
#ifndef PAPR_OTA_SLOT_SIZE
#define PAPR_OTA_SLOT_SIZE              (240U * 1024U)
#endif

/* Maximum image payload bytes carried by one OTA_DATA frame (the rest of the
 * BLE payload is the 4-byte image offset). */
#ifndef PAPR_OTA_CHUNK_MAX
#define PAPR_OTA_CHUNK_MAX              (PAPR_BLE_MAX_PAYLOAD - 4U)
#endif

/* Flash programming granularity in bytes. OTA_DATA offsets / lengths must be
 * a multiple of this (4 = word programming on the GD32 FMC). */
#ifndef PAPR_OTA_WRITE_ALIGN
#define PAPR_OTA_WRITE_ALIGN           4U
#endif

/* If set, OTA_BEGIN rejects an image whose version is older than the running
 * firmware (anti-downgrade). Equal versions are allowed so a re-flash / repair
 * is still possible. */
#ifndef PAPR_OTA_REJECT_DOWNGRADE
#define PAPR_OTA_REJECT_DOWNGRADE      1
#endif

/* ---- Production test (Design-for-Test) ------------------------------------
 * The end-of-line flashing/test station flashes the bootloader + golden app
 * over SWD, then asserts the TEST_MODE pad and resets. The firmware then runs
 * the factory command loop (papr_factory) instead of the normal supervisor:
 * built-in self-test (BIST), per-unit provisioning, and manual actuation for
 * the fixture. Thresholds below define BIST pass/fail bands. */

/* VREF code used to spin the blower during the BIST blower stage. */
#ifndef PAPR_FACTORY_BLOWER_TEST_VREF
#define PAPR_FACTORY_BLOWER_TEST_VREF   (PAPR_L6235_VREF_DAC_MAX / 2U)
#endif

/* Blower spin-up settling time before reading RPM / flow / current. */
#ifndef PAPR_FACTORY_BLOWER_SPINUP_MS
#define PAPR_FACTORY_BLOWER_SPINUP_MS   800U
#endif

/* Minimum motor RPM (TACHO) for a healthy blower at the test VREF. */
#ifndef PAPR_FACTORY_BLOWER_MIN_RPM
#define PAPR_FACTORY_BLOWER_MIN_RPM     4000U
#endif

/* Minimum airflow expected once the blower is spun up. */
#ifndef PAPR_FACTORY_FLOW_MIN_LPM
#define PAPR_FACTORY_FLOW_MIN_LPM       50U
#endif

/* Acceptable battery / bench-supply window at the station. */
#ifndef PAPR_FACTORY_BATT_MIN_MV
#define PAPR_FACTORY_BATT_MIN_MV        10000U
#endif
#ifndef PAPR_FACTORY_BATT_MAX_MV
#define PAPR_FACTORY_BATT_MAX_MV        17500U
#endif

#endif /* PAPR_CONFIG_H */
