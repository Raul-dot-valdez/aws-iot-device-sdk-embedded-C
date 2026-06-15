# 08 — Data Model & Device Shadow

The concrete data contracts: MQTT topic structure, telemetry payloads, the Device
Shadow document (the variant/config contract), and how the two variants differ on
the wire. JSON is shown for clarity; CBOR is used on bandwidth-constrained links.

---

## 1. Naming & topic structure

Thing name convention: `mtr-<region>-<serial>` (e.g. `mtr-uscapge-000A1B2C`).

```
dt/meter/<thingName>/interval        # interval energy reads          (QoS1)
dt/meter/<thingName>/demand          # demand register (commercial)   (QoS1)
dt/meter/<thingName>/pq              # power-quality events/stream     (QoS0/1)
dt/meter/<thingName>/event           # tamper, reverse-energy, alarms  (QoS1)
dt/meter/<thingName>/outage          # last-gasp / restoration         (QoS1)
dt/meter/<thingName>/dr              # DR/V2G compliance telemetry      (QoS1)

$aws/things/<thingName>/shadow/#     # configuration contract (Shadow)
$aws/things/<thingName>/jobs/#       # commands, OTA, DR, disconnect
$aws/things/<thingName>/defender/#   # security/health metrics
```

`dt/` = data telemetry; head-end rules route by the third segment.

---

## 2. Telemetry payloads

### Interval read (both variants — fields scale with variant)

```json
{
  "v": 1,
  "schema": "res.v1",
  "ts": "2026-06-15T03:15:00Z",
  "interval_s": 900,
  "energy": {
    "kWh_import": 1.842,
    "kWh_export": 0.530,
    "net_kWh": 1.312,
    "kVARh_q1": 0.21, "kVARh_q2": 0.04
  },
  "ev": { "evse_kWh_import": 1.20, "evse_kWh_export_v2g": 0.00 },
  "der": { "pv_kWh_export": 0.530, "batt_kWh_net": -0.10 },
  "tariff": { "rate": "TOU_PEAK", "rtp_price": 0.34, "currency": "USD" },
  "quality": "actual",
  "sig": "base64(secure-element signature over canonical payload)"
}
```

### Commercial interval read adds per-phase + demand (`comm.v1`)

```json
{
  "v": 1,
  "schema": "comm.v1",
  "ts": "2026-06-15T03:05:00Z",
  "interval_s": 300,
  "phases": {
    "A": { "kWh_import": 4.10, "kWh_export": 0.0, "kVARh": 1.2, "pf": 0.94, "v_rms": 277.1 },
    "B": { "kWh_import": 3.98, "kWh_export": 0.0, "kVARh": 1.1, "pf": 0.95, "v_rms": 276.8 },
    "C": { "kWh_import": 4.22, "kWh_export": 0.0, "kVARh": 1.3, "pf": 0.93, "v_rms": 277.4 }
  },
  "demand": { "peak_kW": 41.6, "peak_kVA": 44.3, "window": "15min_sliding", "ratchet_kW": 52.0 },
  "tariff": { "rate": "TOU_PEAK", "demand_charge": true },
  "sig": "..."
}
```

### Last-gasp (outage)

```json
{ "v": 1, "ts": "2026-06-15T03:20:11Z", "type": "last_gasp",
  "reason": "loss_of_mains", "phase": "ALL", "v_rms_last": 12.4, "sig": "..." }
```

### Power-quality event

```json
{ "v": 1, "ts": "2026-06-15T03:18:30Z", "type": "voltage_sag",
  "phase": "B", "v_rms": 198.0, "duration_ms": 420, "thd_pct": 6.1, "sig": "..." }
```

### DR/V2G compliance

```json
{ "v": 1, "ts": "2026-06-15T18:00:00Z", "program": "feeder-peak-2026-06-15",
  "setpoint_kW": 6.0, "actual_kW": 5.7, "compliant": true,
  "v2g_export_kWh": 0.0, "job_id": "dr-curtail-8842", "sig": "..." }
```

---

## 3. Device Shadow — the configuration contract

The shadow is where the **variant lives** ([03](03-variant-configuration.md)).

```json
{
  "state": {
    "desired": {
      "variant": "residential",
      "gridProfile": "us-ca-pge",
      "evReady": true,
      "reporting": { "interval_s": 900, "pq_stream": false },
      "tariff": { "plan": "EV2A", "schedule_ref": "tou-2026h2" },
      "dr": { "enrolled": true, "programs": ["feeder-peak", "v2g-pilot"] },
      "features": {
        "demand_register": false,
        "four_quadrant": true,
        "load_limit": false,
        "v2g_authorization": true
      },
      "metrology": { "phases": "split", "ct_ratio": 1 }
    },
    "reported": {
      "variant": "residential",
      "gridProfile": "us-ca-pge",
      "evReady": true,
      "fw_version": "2.4.1",
      "appliedAt": "2026-06-15T02:00:00Z",
      "health": { "rssi": -71, "time_synced": true, "relay": "closed" },
      "configError": null
    }
  }
}
```

**Commercial differences in `desired`:**

```json
{
  "variant": "commercial",
  "reporting": { "interval_s": 300, "pq_stream": true },
  "tariff": { "plan": "B-19", "demand_charge": true },
  "features": {
    "demand_register": true,
    "four_quadrant": true,
    "load_limit": true,
    "v2g_authorization": true
  },
  "metrology": { "phases": "three", "ct_ratio": 200 }
}
```

The Shadow Manager reconciles `desired` → applies at a safe boundary → writes
`reported` ([03 §6](03-variant-configuration.md)). `configError` is non-null when
a desired state fails schema validation, giving operators a closed feedback loop.

---

## 4. Schema & examples

- JSON Schema: [`config-examples/meter-config.schema.json`](config-examples/meter-config.schema.json)
- Residential + EV: [`config-examples/residential-ev.json`](config-examples/residential-ev.json)
- Commercial: [`config-examples/commercial.json`](config-examples/commercial.json)

---

## 5. Schema versioning

- Every payload carries `v` (envelope version) and a `schema` profile id
  (`res.v1` / `comm.v1`). New fields are additive; consumers ignore unknown
  fields. Breaking changes bump the profile id and are rolled out via OTA +
  head-end coordination so MDMS can parse both during transition.

Back to [README](README.md).
