#include "papr_ble.h"
#include "papr_alarms.h"
#include "papr_controller.h"
#include "papr_hal.h"
#include "papr_sdp810.h"   /* for papr_sdp810_crc8 */
#include "papr_version.h"

#include <string.h>

/* ---- Receive parser states ----------------------------------------------- */

enum
{
    RX_WAIT_SYNC0 = 0,
    RX_WAIT_SYNC1,
    RX_WAIT_LEN,
    RX_WAIT_CMD,
    RX_COLLECT_PAYLOAD,
    RX_WAIT_CRC
};

/* Firmware version is owned by papr_version.h; these aliases keep the
 * existing GET_VERSION code path readable. */
#define VERSION_MAJOR  PAPR_FW_VERSION_MAJOR
#define VERSION_MINOR  PAPR_FW_VERSION_MINOR
#define VERSION_PATCH  PAPR_FW_VERSION_PATCH

/* ---- Frame helpers ------------------------------------------------------- */

static papr_status_t send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    if (len > PAPR_BLE_MAX_PAYLOAD) { return PAPR_ERR_PARAM; }

    uint8_t hdr[4] = { PAPR_BLE_SYNC0, PAPR_BLE_SYNC1, len, cmd };
    papr_status_t s = papr_hal_uart_write(hdr, sizeof(hdr));
    if (s != PAPR_OK) { return s; }

    if (len > 0U && payload != NULL)
    {
        s = papr_hal_uart_write(payload, len);
        if (s != PAPR_OK) { return s; }
    }

    /* CRC over [LEN, CMD, PAYLOAD]. Build a small contiguous buffer so we
     * can reuse the existing Sensirion CRC-8 implementation. */
    uint8_t scratch[2 + PAPR_BLE_MAX_PAYLOAD];
    scratch[0] = len;
    scratch[1] = cmd;
    if (len > 0U && payload != NULL)
    {
        memcpy(&scratch[2], payload, len);
    }
    uint8_t crc = papr_sdp810_crc8(scratch, (size_t)(2U + len));
    return papr_hal_uart_write(&crc, 1U);
}

static void send_ack(uint8_t cmd)
{
    (void)send_frame(PAPR_BLE_NTF_ACK, &cmd, 1U);
}

static void send_nack(uint8_t cmd, uint8_t reason)
{
    uint8_t p[2] = { cmd, reason };
    (void)send_frame(PAPR_BLE_NTF_NACK, p, sizeof(p));
}

static void send_event(uint8_t event_type, const uint8_t *extra, uint8_t extra_len)
{
    if (extra_len > PAPR_BLE_MAX_PAYLOAD - 1U) { return; }
    uint8_t p[1 + PAPR_BLE_MAX_PAYLOAD];
    p[0] = event_type;
    if (extra_len > 0U && extra != NULL) { memcpy(&p[1], extra, extra_len); }
    (void)send_frame(PAPR_BLE_NTF_EVENT, p, (uint8_t)(1U + extra_len));
}

/* ---- Telemetry packing --------------------------------------------------- */

static void pack_u16_le(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void pack_i16_le(uint8_t *p, int16_t v)  { pack_u16_le(p, (uint16_t)v); }
static void pack_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t unpack_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Builds and sends an OTA_STATUS notification: [state, error, pct, maj,
 * min, pat]. The version reported is the *running* firmware, so the app can
 * tell the user what is installed now and compare against the latest. */
static void send_ota_status(struct papr_controller *ctrl)
{
    uint8_t st = 0U, err = 0U, pct = 0U;
    papr_controller_ota_status(ctrl, &st, &err, &pct);
    uint8_t p[6] = { st, err, pct,
                     PAPR_FW_VERSION_MAJOR,
                     PAPR_FW_VERSION_MINOR,
                     PAPR_FW_VERSION_PATCH };
    (void)send_frame(PAPR_BLE_NTF_OTA_STATUS, p, sizeof(p));
}

papr_status_t papr_ble_send_telemetry(papr_ble_t *b,
                                      const papr_telemetry_t *t,
                                      uint8_t state, uint8_t level,
                                      uint32_t alarm_mask)
{
    if (b == NULL || t == NULL) { return PAPR_ERR_PARAM; }

    /* Layout (32 bytes): base telemetry + rev-6 adaptive-comfort tail.    */
    uint8_t buf[32];
    uint8_t *p = buf;
    *p++ = state;
    *p++ = level;
    pack_u32_le(p, alarm_mask);                p += 4;
    pack_u16_le(p, t->flow_lpm);               p += 2;
    pack_i16_le(p, t->pressure_pa);            p += 2;
    pack_u32_le(p, t->absolute_pressure_pa);   p += 4;
    pack_i16_le(p, t->temperature_c10);        p += 2;
    pack_u16_le(p, t->battery_mv);             p += 2;
    pack_u16_le(p, t->battery_ma);             p += 2;
    *p++ = t->battery_soc_percent;
    pack_u16_le(p, t->motor_rpm);              p += 2;
    pack_u16_le(p, t->duty_permille);          p += 2;
    *p++ = t->breaths_per_min;
    pack_u16_le(p, t->remaining_minutes);      p += 2;
    *p++ = t->auto_mode_active;

    b->last_telem_ms = papr_hal_now_ms();
    return send_frame(PAPR_BLE_NTF_TELEMETRY, buf, (uint8_t)(p - buf));
}

/* ---- Command dispatch ---------------------------------------------------- */

static void dispatch(papr_ble_t *b, struct papr_controller *ctrl,
                     uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    (void)b;
    switch (cmd)
    {
        case PAPR_BLE_CMD_SET_LEVEL:
            if (len != 1U || payload[0] >= (uint8_t)PAPR_LEVEL_COUNT)
            {
                send_nack(cmd, 1U);
                return;
            }
            papr_controller_remote_set_level(ctrl, (papr_flow_level_t)payload[0]);
            send_ack(cmd);
            break;

        case PAPR_BLE_CMD_POWER_ON:
            papr_controller_remote_power(ctrl, true);
            send_ack(cmd);
            break;

        case PAPR_BLE_CMD_POWER_OFF:
            papr_controller_remote_power(ctrl, false);
            send_ack(cmd);
            break;

        case PAPR_BLE_CMD_MUTE_ALARM:
        {
            if (len != 2U) { send_nack(cmd, 1U); return; }
            uint16_t secs = (uint16_t)((uint16_t)payload[1] << 8 | payload[0]);
            papr_controller_remote_mute(ctrl, (uint32_t)secs * 1000U);
            send_ack(cmd);
            break;
        }

        case PAPR_BLE_CMD_RESET_FAULT:
            papr_controller_remote_reset_fault(ctrl);
            send_ack(cmd);
            break;

        case PAPR_BLE_CMD_GET_VERSION:
        {
            uint8_t v[3] = { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
            (void)send_frame(PAPR_BLE_NTF_ACK, v, sizeof(v));
            break;
        }

        case PAPR_BLE_CMD_GET_TELEMETRY:
        {
            papr_telemetry_t t;
            uint8_t state, level;
            uint32_t mask;
            papr_controller_snapshot(ctrl, &t, &state, &level, &mask);
            (void)papr_ble_send_telemetry(b, &t, state, level, mask);
            break;
        }

        case PAPR_BLE_CMD_SET_AUTO_MODE:
            if (len != 1U) { send_nack(cmd, 1U); return; }
            papr_controller_remote_set_auto(ctrl, payload[0] != 0U);
            send_ack(cmd);
            break;

        case PAPR_BLE_CMD_OTA_BEGIN:
        {
            if (len != 11U) { send_nack(cmd, 1U); return; }
            uint32_t size = unpack_u32_le(&payload[0]);
            uint32_t crc  = unpack_u32_le(&payload[4]);
            papr_fw_version_t ver = { payload[8], payload[9], payload[10] };
            (void)papr_controller_ota_begin(ctrl, size, crc, ver);
            send_ota_status(ctrl);   /* reports RECEIVING or the error */
            break;
        }

        case PAPR_BLE_CMD_OTA_DATA:
        {
            if (len < 5U) { send_nack(cmd, 1U); return; }
            uint32_t offset = unpack_u32_le(&payload[0]);
            uint16_t n      = (uint16_t)(len - 4U);
            papr_ota_error_t e =
                papr_controller_ota_write(ctrl, offset, &payload[4], n);
            if (e == PAPR_OTA_ERR_NONE)
            {
                /* Fast path: bare ACK lets the app pipeline the next chunk. */
                send_ack(cmd);
            }
            else
            {
                send_ota_status(ctrl);
            }
            break;
        }

        case PAPR_BLE_CMD_OTA_END:
            (void)papr_controller_ota_finish(ctrl);
            send_ota_status(ctrl);   /* READY on success, error otherwise */
            break;

        case PAPR_BLE_CMD_OTA_APPLY:
        {
            papr_ota_error_t e = papr_controller_ota_apply(ctrl);
            /* If apply succeeded the MCU resets inside the call and never
             * reaches here; if it returns, report why it could not apply. */
            (void)e;
            send_ota_status(ctrl);
            break;
        }

        case PAPR_BLE_CMD_OTA_ABORT:
            papr_controller_ota_abort(ctrl);
            send_ota_status(ctrl);
            break;

        case PAPR_BLE_CMD_OTA_STATUS:
            send_ota_status(ctrl);
            break;

        default:
            send_nack(cmd, 2U);   /* unknown command */
            break;
    }
}

/* ---- Receive state machine ---------------------------------------------- */

static void rx_reset(papr_ble_t *b)
{
    b->rx_state       = RX_WAIT_SYNC0;
    b->rx_len         = 0U;
    b->rx_payload_len = 0U;
    b->rx_cmd         = 0U;
}

static void feed_byte(papr_ble_t *b, struct papr_controller *ctrl, uint8_t byte)
{
    switch (b->rx_state)
    {
        case RX_WAIT_SYNC0:
            if (byte == PAPR_BLE_SYNC0) { b->rx_state = RX_WAIT_SYNC1; }
            break;

        case RX_WAIT_SYNC1:
            b->rx_state = (byte == PAPR_BLE_SYNC1) ? RX_WAIT_LEN : RX_WAIT_SYNC0;
            break;

        case RX_WAIT_LEN:
            if (byte > PAPR_BLE_MAX_PAYLOAD) { rx_reset(b); break; }
            b->rx_payload_len = byte;
            b->rx_buf[0]      = byte;
            b->rx_len         = 1U;
            b->rx_state       = RX_WAIT_CMD;
            break;

        case RX_WAIT_CMD:
            b->rx_cmd      = byte;
            b->rx_buf[1]   = byte;
            b->rx_len      = 2U;
            b->rx_state    = (b->rx_payload_len > 0U)
                              ? RX_COLLECT_PAYLOAD : RX_WAIT_CRC;
            break;

        case RX_COLLECT_PAYLOAD:
            b->rx_buf[b->rx_len++] = byte;
            if ((b->rx_len - 2U) >= b->rx_payload_len)
            {
                b->rx_state = RX_WAIT_CRC;
            }
            break;

        case RX_WAIT_CRC:
        {
            uint8_t expected = papr_sdp810_crc8(b->rx_buf, b->rx_len);
            if (expected == byte)
            {
                dispatch(b, ctrl, b->rx_cmd,
                         (b->rx_payload_len > 0U) ? &b->rx_buf[2] : NULL,
                         b->rx_payload_len);
            }
            else
            {
                send_nack(b->rx_cmd, 3U);   /* CRC error */
            }
            rx_reset(b);
            break;
        }

        default:
            rx_reset(b);
            break;
    }
}

/* ---- Public API --------------------------------------------------------- */

papr_status_t papr_ble_init(papr_ble_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    memset(b, 0, sizeof(*b));
    rx_reset(b);
    b->last_state      = 0xFFU;
    b->last_level      = 0xFFU;
    b->last_alarm_mask = 0xFFFFFFFFU;
    return papr_ble_module_reset(b);
}

papr_status_t papr_ble_module_reset(papr_ble_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    /* Pulse RESET_N low for ~10 ms then release. The module enumerates and
     * begins advertising autonomously. */
    (void)papr_hal_ble_set_reset(true);
    uint32_t start = papr_hal_now_ms();
    while ((uint32_t)(papr_hal_now_ms() - start) < 10U) { /* spin */ }
    (void)papr_hal_ble_set_reset(false);
    b->module_ready = true;
    rx_reset(b);
    return PAPR_OK;
}

papr_status_t papr_ble_poll(papr_ble_t *b, struct papr_controller *ctrl)
{
    if (b == NULL || ctrl == NULL) { return PAPR_ERR_PARAM; }

    /* Drain a bounded number of RX bytes per poll to keep the supervisor
     * tick latency predictable. */
    for (uint16_t i = 0U; i < 64U; ++i)
    {
        uint8_t byte;
        if (!papr_hal_uart_read_byte(&byte)) { break; }
        feed_byte(b, ctrl, byte);
    }

    /* Periodic telemetry. */
    uint32_t now = papr_hal_now_ms();
    if ((uint32_t)(now - b->last_telem_ms) >= PAPR_BLE_TELEM_PERIOD_MS)
    {
        papr_telemetry_t t;
        uint8_t state, level;
        uint32_t mask;
        papr_controller_snapshot(ctrl, &t, &state, &level, &mask);

        if (state != b->last_state)
        {
            uint8_t extra = state;
            send_event(PAPR_BLE_EVT_STATE_CHANGE, &extra, 1U);
            b->last_state = state;
        }
        if (level != b->last_level)
        {
            uint8_t extra = level;
            send_event(PAPR_BLE_EVT_LEVEL_CHANGE, &extra, 1U);
            b->last_level = level;
        }
        if (mask != b->last_alarm_mask)
        {
            uint8_t extra[4];
            pack_u32_le(extra, mask);
            send_event(PAPR_BLE_EVT_ALARM_CHANGE, extra, sizeof(extra));
            b->last_alarm_mask = mask;
        }

        (void)papr_ble_send_telemetry(b, &t, state, level, mask);
    }

    return PAPR_OK;
}
