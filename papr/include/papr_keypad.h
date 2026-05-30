#ifndef PAPR_KEYPAD_H
#define PAPR_KEYPAD_H

#include "papr_config.h"
#include "papr_types.h"

/* 3x3 switch matrix:
 *
 *     col 0           col 1           col 2
 *   +---------------+---------------+---------------+
 *   | POWER         | LEVEL_UP      | LEVEL_DOWN    |   row 0
 *   +---------------+---------------+---------------+
 *   | MUTE          | MODE (auto)   | PAIR          |   row 1
 *   +---------------+---------------+---------------+
 *   | BRIGHT        | INFO          | RESET_FAULT   |   row 2
 *   +---------------+---------------+---------------+
 *
 * The scan is portable; only papr_hal_keypad_drive_row() and
 * papr_hal_keypad_read_col() are platform-specific. Long-press of any key
 * (default 1.2 s) produces a PAPR_KEY_EVT_LONG_PRESS in addition to the
 * earlier PRESS event, which lets the supervisor implement chorded /
 * gated actions (e.g. POWER long-press → real shutdown). */

typedef enum
{
    PAPR_KEY_POWER       = 0U,
    PAPR_KEY_LEVEL_UP    = 1U,
    PAPR_KEY_LEVEL_DOWN  = 2U,
    PAPR_KEY_MUTE        = 3U,
    PAPR_KEY_MODE        = 4U,
    PAPR_KEY_PAIR        = 5U,
    PAPR_KEY_BRIGHT      = 6U,
    PAPR_KEY_INFO        = 7U,
    PAPR_KEY_RESET       = 8U,
    PAPR_KEY_COUNT       = (PAPR_KEYPAD_ROWS * PAPR_KEYPAD_COLS)
} papr_key_t;

typedef enum
{
    PAPR_KEY_EVT_PRESS = 0U,
    PAPR_KEY_EVT_RELEASE,
    PAPR_KEY_EVT_LONG_PRESS
} papr_key_event_kind_t;

typedef struct
{
    papr_key_t            key;
    papr_key_event_kind_t kind;
    uint32_t              timestamp_ms;
} papr_key_event_t;

typedef struct
{
    /* Per-key state. */
    uint8_t  stable_state[PAPR_KEY_COUNT];     /* 0 = up, 1 = down */
    uint8_t  candidate_state[PAPR_KEY_COUNT];  /* incoming sample  */
    uint8_t  candidate_count[PAPR_KEY_COUNT];  /* debounce counter */
    uint32_t pressed_ms[PAPR_KEY_COUNT];       /* edge timestamp   */
    uint8_t  long_press_emitted[PAPR_KEY_COUNT];

    /* Event FIFO (drained by the supervisor each tick). */
    papr_key_event_t queue[PAPR_KEYPAD_QUEUE_LEN];
    uint8_t          head;
    uint8_t          tail;

    uint32_t last_scan_ms;
} papr_keypad_t;

papr_status_t papr_keypad_init(papr_keypad_t *k);

/* One scan of the matrix. Pace using PAPR_KEYPAD_SCAN_PERIOD_MS; the routine
 * is cheap (≈ rows × (drive + settle + col reads)) and can be called every
 * supervisor tick — it self-paces internally. */
papr_status_t papr_keypad_scan(papr_keypad_t *k, uint32_t now_ms);

/* Pops the next event from the FIFO. Returns false if the queue is empty. */
bool papr_keypad_poll(papr_keypad_t *k, papr_key_event_t *out);

bool papr_keypad_is_down(const papr_keypad_t *k, papr_key_t key);

#endif /* PAPR_KEYPAD_H */
