#ifndef PAPR_BLE_H
#define PAPR_BLE_H

#include "papr_config.h"
#include "papr_types.h"

/* Wireless link to the paired mobile application via the
 * GD32VW553-UNIFI-EMH7 module. Communication with the module is a simple
 * length-prefixed binary protocol over UART; the module itself handles BLE
 * pairing, GATT, and the over-the-air encoding.
 *
 * Frame layout (little-endian where applicable):
 *
 *   +------+------+------+------+------------------+------+
 *   | SYN0 | SYN1 | LEN  | CMD  | PAYLOAD (LEN B)  | CRC8 |
 *   +------+------+------+------+------------------+------+
 *      AA     55     N      C       C bytes         crc
 *
 *   - LEN is the payload length (0 .. PAPR_BLE_MAX_PAYLOAD).
 *   - CRC8 is computed over [LEN, CMD, PAYLOAD]. Polynomial 0x31, init 0xFF
 *     (the same Sensirion CRC-8 we already use for the SDP810).
 *
 * Mobile → device commands:
 *   0x01 SET_LEVEL       payload: [u8 level]              (0=lo, 1=med, 2=hi)
 *   0x02 POWER_ON        payload: empty
 *   0x03 POWER_OFF       payload: empty
 *   0x04 MUTE_ALARM      payload: [u16 LE seconds]
 *   0x05 RESET_FAULT     payload: empty
 *   0x06 GET_VERSION     payload: empty
 *   0x07 GET_TELEMETRY   payload: empty   (force one immediate telemetry)
 *
 * Device → mobile notifications:
 *   0x80 TELEMETRY       payload: papr_ble_telemetry_t (packed)
 *   0x81 ACK             payload: [u8 cmd]
 *   0x82 NACK            payload: [u8 cmd, u8 reason]
 *   0x83 EVENT           payload: [u8 event_type, …]
 */

typedef enum
{
    PAPR_BLE_CMD_SET_LEVEL      = 0x01U,
    PAPR_BLE_CMD_POWER_ON       = 0x02U,
    PAPR_BLE_CMD_POWER_OFF      = 0x03U,
    PAPR_BLE_CMD_MUTE_ALARM     = 0x04U,
    PAPR_BLE_CMD_RESET_FAULT    = 0x05U,
    PAPR_BLE_CMD_GET_VERSION    = 0x06U,
    PAPR_BLE_CMD_GET_TELEMETRY  = 0x07U,

    PAPR_BLE_NTF_TELEMETRY      = 0x80U,
    PAPR_BLE_NTF_ACK            = 0x81U,
    PAPR_BLE_NTF_NACK           = 0x82U,
    PAPR_BLE_NTF_EVENT          = 0x83U
} papr_ble_msg_t;

typedef enum
{
    PAPR_BLE_EVT_STATE_CHANGE   = 0x01U,
    PAPR_BLE_EVT_ALARM_CHANGE   = 0x02U,
    PAPR_BLE_EVT_LEVEL_CHANGE   = 0x03U,
    PAPR_BLE_EVT_FAULT          = 0x04U
} papr_ble_event_t;

/* Forward declaration to avoid pulling in the full controller header. */
struct papr_controller;

typedef struct
{
    /* Receive parser state. */
    uint8_t  rx_buf[PAPR_BLE_MAX_PAYLOAD + 8U];
    uint16_t rx_len;
    uint8_t  rx_state;
    uint8_t  rx_payload_len;
    uint8_t  rx_cmd;

    /* Transmit pacing. */
    uint32_t last_telem_ms;

    /* Cached state used to detect transitions for EVENT notifications. */
    uint8_t  last_state;
    uint8_t  last_level;
    uint32_t last_alarm_mask;

    bool     paired;
    bool     module_ready;
} papr_ble_t;

papr_status_t papr_ble_init(papr_ble_t *b);

/* Brings the GD32VW553 module out of reset and waits for the boot ack. */
papr_status_t papr_ble_module_reset(papr_ble_t *b);

/* Drains the UART RX buffer, parses any complete frames, and invokes
 * controller actions for valid commands. Sends queued telemetry / event
 * notifications. Call once per supervisor tick. */
papr_status_t papr_ble_poll(papr_ble_t *b, struct papr_controller *ctrl);

/* Forces an immediate telemetry frame. */
papr_status_t papr_ble_send_telemetry(papr_ble_t *b,
                                      const papr_telemetry_t *t,
                                      uint8_t state, uint8_t level,
                                      uint32_t alarm_mask);

#endif /* PAPR_BLE_H */
