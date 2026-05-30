#include "papr_factory.h"
#include "papr_config.h"
#include "papr_gdy1124.h"
#include "papr_hal.h"
#include "papr_provision.h"
#include "papr_sdp810.h"
#include "papr_version.h"

#include <string.h>

/* ---- factory command set (over the BLE UART pads) ------------------------ */
enum
{
    FCT_PING        = 0xF0U,  /* -> FCT_INFO                                 */
    FCT_RUN_BIST    = 0xF1U,  /* -> FCT_BIST report                          */
    FCT_READ_PROV   = 0xF2U,  /* -> provisioning blob                        */
    FCT_WRITE_PROV  = 0xF3U,  /* [papr_provision_t] -> status                */
    FCT_SET_OUTPUT  = 0xF4U,  /* [u8 target][u16 value] -> status            */
    FCT_READ_SENSE  = 0xF5U,  /* -> raw sensor snapshot                      */
    FCT_REBOOT      = 0xF6U,  /* -> status, then reset into app              */

    FCT_RSP_INFO    = 0xE0U,
    FCT_RSP_BIST    = 0xE1U,
    FCT_RSP_PROV    = 0xE2U,
    FCT_RSP_SENSE   = 0xE3U,
    FCT_RSP_STATUS  = 0xEFU
};

/* FCT_SET_OUTPUT targets. */
enum { OUT_LED0 = 0U, OUT_LED1, OUT_LED2, OUT_BUZZER, OUT_BLOWER_VREF };

#define SYNC0 0xAAU
#define SYNC1 0x55U
#define FCT_MAX_PAYLOAD 96U

/* ---- timing -------------------------------------------------------------- */
static void busy_wait_ms(uint32_t ms)
{
    uint32_t start = papr_hal_now_ms();
    while ((uint32_t)(papr_hal_now_ms() - start) < ms)
    {
        papr_hal_wdt_kick();
    }
}

/* ---- framed UART I/O (shares the BLE wire format + CRC-8) ----------------- */
static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t hdr[4] = { SYNC0, SYNC1, len, cmd };
    (void)papr_hal_uart_write(hdr, sizeof(hdr));
    if (len > 0U && payload != NULL) { (void)papr_hal_uart_write(payload, len); }

    uint8_t scratch[2U + FCT_MAX_PAYLOAD];
    scratch[0] = len;
    scratch[1] = cmd;
    if (len > 0U && payload != NULL) { memcpy(&scratch[2], payload, len); }
    uint8_t crc = papr_sdp810_crc8(scratch, (size_t)(2U + len));
    (void)papr_hal_uart_write(&crc, 1U);
}

static void send_status(uint8_t code)
{
    send_frame(FCT_RSP_STATUS, &code, 1U);
}

/* ---- BIST helpers -------------------------------------------------------- */
static void mark(papr_bist_report_t *r, papr_bist_bit_t bit, bool pass)
{
    r->executed_mask |= (uint16_t)bit;
    if (pass) { r->pass_mask |= (uint16_t)bit; }
}

static bool keypad_no_stuck(void)
{
    bool any_down = false;
    for (uint8_t row = 0U; row < PAPR_KEYPAD_ROWS; ++row)
    {
        (void)papr_hal_keypad_drive_row(row, true);
        busy_wait_ms(1U);
        for (uint8_t col = 0U; col < PAPR_KEYPAD_COLS; ++col)
        {
            if (papr_hal_keypad_read_col(col)) { any_down = true; }
        }
        (void)papr_hal_keypad_drive_row(row, false);
    }
    return !any_down;   /* pass = nothing pressed at EOL */
}

bool papr_factory_run_bist(papr_bist_report_t *report)
{
    if (report == NULL) { return false; }
    memset(report, 0, sizeof(*report));

    /* 1. provisioning page present? (warn-only: a fresh board has none yet) */
    papr_provision_t prov;
    mark(report, PAPR_BIST_PROVISION, papr_provision_load(&prov) == PAPR_OK);

    /* 2. battery / bench supply in window */
    uint16_t mv = 0U;
    bool ok = (papr_hal_read_battery_mv(&mv) == PAPR_OK);
    report->battery_mv = mv;
    mark(report, PAPR_BIST_BATTERY,
         ok && mv >= PAPR_FACTORY_BATT_MIN_MV && mv <= PAPR_FACTORY_BATT_MAX_MV);

    /* 3. temperature sane */
    int16_t c10 = 0;
    ok = (papr_hal_read_temperature_c10(&c10) == PAPR_OK);
    report->temperature_c10 = c10;
    mark(report, PAPR_BIST_TEMP, ok && c10 > -300 && c10 < 850);

    /* 4. SDP810 differential pressure responds with valid CRC frames */
    papr_sdp810_t sdp;
    bool sdp_ok = (papr_sdp810_init(&sdp) == PAPR_OK);
    busy_wait_ms(25U);
    sdp_ok = sdp_ok && (papr_sdp810_read(&sdp) == PAPR_OK) && papr_sdp810_valid(&sdp);
    report->sdp_pressure_pa = papr_sdp810_pressure_pa(&sdp);
    mark(report, PAPR_BIST_SDP810, sdp_ok);

    /* 5. GDY1124 absolute pressure chip-ID + read */
    papr_gdy1124_t baro;
    bool baro_ok = (papr_gdy1124_init(&baro) == PAPR_OK);
    baro_ok = baro_ok && (papr_gdy1124_read(&baro) == PAPR_OK) && papr_gdy1124_valid(&baro);
    report->baro_pressure_pa = papr_gdy1124_pressure_pa(&baro);
    mark(report, PAPR_BIST_GDY1124, baro_ok);

    /* 6/7/8. spin the blower and check TACHO, current, and that flow rises */
    uint16_t flow_off = 0U;
    (void)papr_hal_read_flow_lpm(&flow_off);

    (void)papr_hal_l6235_set_forward(true);
    (void)papr_hal_l6235_set_brake(false);
    (void)papr_hal_l6235_set_enable(true);
    (void)papr_hal_l6235_set_vref(PAPR_FACTORY_BLOWER_TEST_VREF);
    busy_wait_ms(PAPR_FACTORY_BLOWER_SPINUP_MS);

    uint16_t rpm = 0U, cur = 0U, flow_on = 0U;
    (void)papr_hal_l6235_read_tacho_rpm(&rpm);
    (void)papr_hal_read_battery_ma(&cur);
    (void)papr_hal_read_flow_lpm(&flow_on);
    bool diag = papr_hal_l6235_diag_active();

    report->blower_rpm        = rpm;
    report->blower_current_ma = cur;
    report->flow_lpm          = flow_on;

    mark(report, PAPR_BIST_TACHO,  rpm >= PAPR_FACTORY_BLOWER_MIN_RPM);
    mark(report, PAPR_BIST_BLOWER, !diag && rpm >= PAPR_FACTORY_BLOWER_MIN_RPM);
    mark(report, PAPR_BIST_FLOW,
         flow_on >= PAPR_FACTORY_FLOW_MIN_LPM && flow_on > flow_off);

    (void)papr_hal_l6235_set_vref(0U);
    (void)papr_hal_l6235_set_enable(false);
    (void)papr_hal_l6235_set_brake(true);

    /* 9. keypad: nothing stuck */
    mark(report, PAPR_BIST_KEYPAD, keypad_no_stuck());

    /* 10. LEDs: cycle each (fixture's optical sensors confirm). */
    for (uint8_t led = 0U; led < 3U; ++led)
    {
        (void)papr_hal_led_set(led, true);
        busy_wait_ms(120U);
        (void)papr_hal_led_set(led, false);
    }
    mark(report, PAPR_BIST_LED, true);

    /* 11. buzzer: short chirp (fixture's mic confirms). */
    (void)papr_hal_buzzer_set(true, 2700U);
    busy_wait_ms(150U);
    (void)papr_hal_buzzer_set(false, 0U);
    mark(report, PAPR_BIST_BUZZER, true);

    /* 12. BLE module: pulse reset (fixture confirms it advertises). */
    (void)papr_hal_ble_set_reset(true);
    busy_wait_ms(10U);
    (void)papr_hal_ble_set_reset(false);
    mark(report, PAPR_BIST_BLE, true);

    return report->executed_mask == report->pass_mask;
}

/* ---- responses ----------------------------------------------------------- */
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;        p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void respond_info(void)
{
    papr_provision_t prov;
    bool provisioned = (papr_provision_load(&prov) == PAPR_OK);
    char serial[PAPR_PROV_SERIAL_LEN + 1U];
    papr_provision_serial(provisioned ? &prov : NULL, serial);

    uint8_t p[4 + PAPR_PROV_SERIAL_LEN];
    p[0] = PAPR_FW_VERSION_MAJOR;
    p[1] = PAPR_FW_VERSION_MINOR;
    p[2] = PAPR_FW_VERSION_PATCH;
    p[3] = provisioned ? 1U : 0U;
    memcpy(&p[4], serial, PAPR_PROV_SERIAL_LEN);
    send_frame(FCT_RSP_INFO, p, sizeof(p));
}

static void respond_bist(void)
{
    papr_bist_report_t r;
    (void)papr_factory_run_bist(&r);

    uint8_t p[20];
    put_u16(&p[0],  r.executed_mask);
    put_u16(&p[2],  r.pass_mask);
    put_u16(&p[4],  r.blower_rpm);
    put_u16(&p[6],  r.blower_current_ma);
    put_u16(&p[8],  r.flow_lpm);
    put_u16(&p[10], (uint16_t)r.sdp_pressure_pa);
    put_u32(&p[12], r.baro_pressure_pa);
    put_u16(&p[16], r.battery_mv);
    put_u16(&p[18], (uint16_t)r.temperature_c10);
    send_frame(FCT_RSP_BIST, p, sizeof(p));
}

static void respond_prov(void)
{
    papr_provision_t prov;
    if (papr_provision_load(&prov) != PAPR_OK)
    {
        memset(&prov, 0, sizeof(prov));   /* blank record signals unprovisioned */
    }
    send_frame(FCT_RSP_PROV, (const uint8_t *)&prov, (uint8_t)sizeof(prov));
}

static void respond_sense(void)
{
    uint16_t flow = 0U, mv = 0U, ma = 0U, rpm = 0U;
    int16_t  c10 = 0;
    (void)papr_hal_read_flow_lpm(&flow);
    (void)papr_hal_read_battery_mv(&mv);
    (void)papr_hal_read_battery_ma(&ma);
    (void)papr_hal_l6235_read_tacho_rpm(&rpm);
    (void)papr_hal_read_temperature_c10(&c10);

    uint8_t p[10];
    put_u16(&p[0], flow);
    put_u16(&p[2], mv);
    put_u16(&p[4], ma);
    put_u16(&p[6], rpm);
    put_u16(&p[8], (uint16_t)c10);
    send_frame(FCT_RSP_SENSE, p, sizeof(p));
}

static void handle_set_output(const uint8_t *payload, uint8_t len)
{
    if (len < 3U) { send_status(1U); return; }
    uint8_t  target = payload[0];
    uint16_t value  = (uint16_t)((uint16_t)payload[1] | ((uint16_t)payload[2] << 8));
    switch (target)
    {
        case OUT_LED0: case OUT_LED1: case OUT_LED2:
            (void)papr_hal_led_set(target, value != 0U);
            break;
        case OUT_BUZZER:
            (void)papr_hal_buzzer_set(value != 0U, value ? value : 0U);
            break;
        case OUT_BLOWER_VREF:
            (void)papr_hal_l6235_set_forward(true);
            (void)papr_hal_l6235_set_brake(value != 0U ? false : true);
            (void)papr_hal_l6235_set_enable(value != 0U);
            (void)papr_hal_l6235_set_vref(value);
            break;
        default:
            send_status(1U);
            return;
    }
    send_status(0U);
}

static void dispatch(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    switch (cmd)
    {
        case FCT_PING:       respond_info();  break;
        case FCT_RUN_BIST:   respond_bist();  break;
        case FCT_READ_PROV:  respond_prov();  break;
        case FCT_READ_SENSE: respond_sense(); break;
        case FCT_SET_OUTPUT: handle_set_output(payload, len); break;

        case FCT_WRITE_PROV:
            if (len != (uint8_t)sizeof(papr_provision_t)) { send_status(1U); break; }
            else
            {
                papr_provision_t prov;
                memcpy(&prov, payload, sizeof(prov));
                /* Station supplies the fields; we (re)compute magic+crc. */
                send_status((papr_provision_save(&prov) == PAPR_OK) ? 0U : 2U);
            }
            break;

        case FCT_REBOOT:
            send_status(0U);
            busy_wait_ms(20U);          /* let the status frame flush */
            papr_hal_ota_reboot();      /* no return on real hardware */
            break;

        default:
            send_status(0xFFU);         /* unknown command */
            break;
    }
}

/* ---- RX parser (same framing as the BLE link) ---------------------------- */
void papr_factory_main(void)
{
    enum { S_SYNC0, S_SYNC1, S_LEN, S_CMD, S_PAYLOAD, S_CRC } st = S_SYNC0;
    uint8_t  buf[2U + FCT_MAX_PAYLOAD];
    uint8_t  plen = 0U, cmd = 0U;
    uint16_t idx = 0U;

    /* Announce readiness so the station knows the unit booted into test. */
    respond_info();

    for (;;)
    {
        papr_hal_wdt_kick();

        uint8_t byte;
        if (!papr_hal_uart_read_byte(&byte)) { continue; }

        switch (st)
        {
            case S_SYNC0: st = (byte == SYNC0) ? S_SYNC1 : S_SYNC0; break;
            case S_SYNC1: st = (byte == SYNC1) ? S_LEN   : S_SYNC0; break;
            case S_LEN:
                if (byte > FCT_MAX_PAYLOAD) { st = S_SYNC0; break; }
                plen = byte; buf[0] = byte; idx = 1U; st = S_CMD;
                break;
            case S_CMD:
                cmd = byte; buf[1] = byte; idx = 2U;
                st = (plen > 0U) ? S_PAYLOAD : S_CRC;
                break;
            case S_PAYLOAD:
                buf[idx++] = byte;
                if ((idx - 2U) >= plen) { st = S_CRC; }
                break;
            case S_CRC:
                if (papr_sdp810_crc8(buf, idx) == byte)
                {
                    dispatch(cmd, (plen > 0U) ? &buf[2] : NULL, plen);
                }
                else
                {
                    send_status(3U);   /* CRC error */
                }
                st = S_SYNC0;
                break;
            default: st = S_SYNC0; break;
        }
    }
}
