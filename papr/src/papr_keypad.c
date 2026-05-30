#include "papr_keypad.h"
#include "papr_hal.h"

#include <string.h>

static void enqueue(papr_keypad_t *k, papr_key_t key,
                    papr_key_event_kind_t kind, uint32_t now)
{
    uint8_t next = (uint8_t)((k->head + 1U) % PAPR_KEYPAD_QUEUE_LEN);
    if (next == k->tail)
    {
        /* Queue full — drop the oldest event to keep the most recent ones,
         * because stale releases are less useful than fresh presses. */
        k->tail = (uint8_t)((k->tail + 1U) % PAPR_KEYPAD_QUEUE_LEN);
    }
    k->queue[k->head].key          = key;
    k->queue[k->head].kind         = kind;
    k->queue[k->head].timestamp_ms = now;
    k->head = next;
}

papr_status_t papr_keypad_init(papr_keypad_t *k)
{
    if (k == NULL) { return PAPR_ERR_PARAM; }
    memset(k, 0, sizeof(*k));

    /* Park all rows inactive so a stray column read doesn't ghost a key. */
    for (uint8_t r = 0U; r < PAPR_KEYPAD_ROWS; ++r)
    {
        (void)papr_hal_keypad_drive_row(r, false);
    }
    return PAPR_OK;
}

papr_status_t papr_keypad_scan(papr_keypad_t *k, uint32_t now_ms)
{
    if (k == NULL) { return PAPR_ERR_PARAM; }

    if ((uint32_t)(now_ms - k->last_scan_ms) < PAPR_KEYPAD_SCAN_PERIOD_MS &&
        k->last_scan_ms != 0U)
    {
        return PAPR_OK;
    }
    k->last_scan_ms = now_ms;

    /* Read the matrix into a local snapshot first, then update state, so the
     * column-settle delays don't bias the debounce window. */
    uint8_t snapshot[PAPR_KEY_COUNT];
    for (uint8_t r = 0U; r < PAPR_KEYPAD_ROWS; ++r)
    {
        (void)papr_hal_keypad_drive_row(r, true);
        /* Brief settling: a handful of NOPs is sufficient at ~100 MHz; on
         * slower MCUs the HAL itself can add a delay before returning. */
        for (volatile uint32_t i = 0U; i < 50U; ++i) { (void)i; }

        for (uint8_t c = 0U; c < PAPR_KEYPAD_COLS; ++c)
        {
            uint8_t idx = (uint8_t)(r * PAPR_KEYPAD_COLS + c);
            snapshot[idx] = papr_hal_keypad_read_col(c) ? 1U : 0U;
        }
        (void)papr_hal_keypad_drive_row(r, false);
    }

    /* Debounce + edge / long-press detection. */
    for (uint8_t i = 0U; i < PAPR_KEY_COUNT; ++i)
    {
        if (snapshot[i] != k->candidate_state[i])
        {
            k->candidate_state[i] = snapshot[i];
            k->candidate_count[i] = 1U;
            continue;
        }
        if (k->candidate_count[i] < 0xFFU) { ++k->candidate_count[i]; }

        if (k->candidate_count[i] >= PAPR_KEYPAD_DEBOUNCE_SCANS &&
            k->candidate_state[i] != k->stable_state[i])
        {
            k->stable_state[i] = k->candidate_state[i];
            if (k->stable_state[i] == 1U)
            {
                k->pressed_ms[i]         = now_ms;
                k->long_press_emitted[i] = 0U;
                enqueue(k, (papr_key_t)i, PAPR_KEY_EVT_PRESS, now_ms);
            }
            else
            {
                enqueue(k, (papr_key_t)i, PAPR_KEY_EVT_RELEASE, now_ms);
            }
        }

        if (k->stable_state[i] == 1U && !k->long_press_emitted[i] &&
            (uint32_t)(now_ms - k->pressed_ms[i]) >= PAPR_KEYPAD_LONG_PRESS_MS)
        {
            k->long_press_emitted[i] = 1U;
            enqueue(k, (papr_key_t)i, PAPR_KEY_EVT_LONG_PRESS, now_ms);
        }
    }
    return PAPR_OK;
}

bool papr_keypad_poll(papr_keypad_t *k, papr_key_event_t *out)
{
    if (k == NULL || out == NULL || k->head == k->tail) { return false; }
    *out    = k->queue[k->tail];
    k->tail = (uint8_t)((k->tail + 1U) % PAPR_KEYPAD_QUEUE_LEN);
    return true;
}

bool papr_keypad_is_down(const papr_keypad_t *k, papr_key_t key)
{
    if (k == NULL || (unsigned)key >= (unsigned)PAPR_KEY_COUNT) { return false; }
    return k->stable_state[key] == 1U;
}
