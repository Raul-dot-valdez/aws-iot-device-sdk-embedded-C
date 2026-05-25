/* PAPR reference secure bootloader (GD32E517RE).
 *
 * Responsibilities:
 *   - verify each application slot's signed manifest (SHA-256 of the image +
 *     vendor signature) before allowing it to run  -> secure boot
 *   - select the slot to run with anti-rollback and a one-try rollback:
 *       * a freshly OTA'd image is PENDING; the bootloader marks it TRYING and
 *         boots it once. The app confirms (-> VALID) only after a healthy
 *         self-test. If it resets without confirming, the next boot sees
 *         TRYING, marks it INVALID, and falls back to the previous VALID slot.
 *   - if no slot verifies, stay in a recovery loop (await SWD reflash / DFU).
 *
 * The bootloader is standalone firmware: its own linker script
 * (linker/bootloader.ld), its own reset path via the GD32 startup, and it
 * reuses only papr_sha256.c (which has no HAL dependency). It never calls into
 * the application HAL. Built by the PAPR_BUILD_BOOTLOADER CMake target.
 *
 * Compile-time policy:
 *   PAPR_BOOT_REQUIRE_SIG (default 1): refuse images that fail signature
 *   verification. Set to 0 only for un-provisioned bring-up boards. */

#ifdef GD32E51X

#include "gd32e51x.h"
#include "boot_shared.h"
#include "papr_sha256.h"

#include <string.h>

#ifndef PAPR_BOOT_REQUIRE_SIG
#define PAPR_BOOT_REQUIRE_SIG  1
#endif

#define SEC_KEY_VENDOR_ADDR  (PAPR_PROVISION_ADDR + 0x140U)

static const uint32_t k_slot_base[2] = { PAPR_SLOT_A_ADDR, PAPR_SLOT_B_ADDR };

/* ---- flash helpers ------------------------------------------------------- */

static void state_read(papr_boot_state_record_t *r)
{
    memcpy(r, (const void *)PAPR_BOOT_STATE_ADDR, sizeof(*r));
    if (r->magic != PAPR_IMG_MAGIC)
    {
        memset(r, 0, sizeof(*r));
        r->magic = PAPR_IMG_MAGIC;
        r->slot_state[0] = PAPR_BOOT_VALID;   /* factory image in slot A */
        r->slot_state[1] = PAPR_BOOT_EMPTY;
    }
}

static void state_write(const papr_boot_state_record_t *r)
{
    fmc_unlock();
    if (fmc_page_erase(PAPR_BOOT_STATE_ADDR) == FMC_READY)
    {
        const uint8_t *src = (const uint8_t *)r;
        for (uint32_t i = 0U; i < sizeof(*r); i += 4U)
        {
            uint32_t w = 0xFFFFFFFFU;
            uint32_t take = ((sizeof(*r) - i) >= 4U) ? 4U : (uint32_t)(sizeof(*r) - i);
            for (uint32_t b = 0U; b < take; ++b) { ((uint8_t *)&w)[b] = src[i + b]; }
            (void)fmc_word_program(PAPR_BOOT_STATE_ADDR + i, w);
        }
    }
    fmc_lock();
}

/* ---- image verification (secure boot) ------------------------------------ */

static bool slot_verify(uint8_t slot)
{
    const papr_img_manifest_t *m =
        (const papr_img_manifest_t *)(k_slot_base[slot] + PAPR_MANIFEST_OFFSET);

    if (m->magic != PAPR_IMG_MAGIC) { return false; }
    if (m->image_size == 0U || m->image_size > PAPR_SLOT_APP_MAX) { return false; }

    /* integrity: SHA-256 over the image bytes must match the manifest. */
    uint8_t digest[PAPR_SHA256_LEN];
    papr_sha256((const uint8_t *)k_slot_base[slot], m->image_size, digest);
    if (!papr_ct_equal(digest, m->sha256, PAPR_SHA256_LEN)) { return false; }

#if PAPR_BOOT_REQUIRE_SIG
    /* authenticity: HMAC-SHA256 over the signed manifest header with the
     * vendor key (reference; swap for ECDSA-P256 in production). */
    uint8_t mh[PAPR_SHA256_LEN];
    papr_sha256((const uint8_t *)m, offsetof(papr_img_manifest_t, signature), mh);
    uint8_t tag[PAPR_SHA256_DIGEST_LEN];
    papr_hmac_sha256((const uint8_t *)SEC_KEY_VENDOR_ADDR, PAPR_SHA256_LEN,
                     mh, PAPR_SHA256_LEN, tag);
    if (!papr_ct_equal(tag, m->signature, PAPR_SHA256_DIGEST_LEN)) { return false; }
#endif
    return true;
}

static uint32_t slot_sec_version(uint8_t slot)
{
    const papr_img_manifest_t *m =
        (const papr_img_manifest_t *)(k_slot_base[slot] + PAPR_MANIFEST_OFFSET);
    return m->sec_version;
}

/* ---- jump to application -------------------------------------------------- */

static void jump_to_app(uint32_t base)
{
    uint32_t msp   = *(const volatile uint32_t *)base;
    uint32_t reset = *(const volatile uint32_t *)(base + 4U);

    __disable_irq();
    SCB->VTOR = base;                 /* relocate the vector table to the slot */
    __DSB();
    __set_MSP(msp);
    __ISB();
    ((void (*)(void))reset)();        /* never returns */
    for (;;) { }
}

/* ---- recovery ------------------------------------------------------------ */

static void recovery_loop(void)
{
    /* No bootable image. Blink a fault LED and wait for the SWD flasher or a
     * serial DFU. The independent watchdog is intentionally NOT started so a
     * stuck unit can be reflashed at the bench. */
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_10);
    for (;;)
    {
        gpio_bit_set(GPIOB, GPIO_PIN_10);
        for (volatile uint32_t i = 0U; i < 800000U; ++i) { (void)i; }
        gpio_bit_reset(GPIOB, GPIO_PIN_10);
        for (volatile uint32_t i = 0U; i < 800000U; ++i) { (void)i; }
    }
}

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    SystemInit();

    /* Refuse to proceed if the crypto self-test fails (corrupt bootloader). */
    if (!papr_sha256_selftest()) { recovery_loop(); }

    papr_boot_state_record_t st;
    state_read(&st);

    /* A slot left in TRYING means a prior trial booted but never confirmed
     * (crash / hang). Roll it back. */
    bool dirty = false;
    for (uint8_t s = 0U; s < 2U; ++s)
    {
        if (st.slot_state[s] == PAPR_BOOT_TRYING)
        {
            st.slot_state[s] = PAPR_BOOT_INVALID;
            dirty = true;
        }
    }

    /* Pick a slot: a verifiable PENDING image gets its one trial; otherwise
     * the verifiable VALID image with the highest security version. */
    int8_t   choice = -1;
    bool     choice_pending = false;
    uint32_t choice_secv = 0U;

    for (uint8_t s = 0U; s < 2U; ++s)
    {
        uint32_t state = st.slot_state[s];
        bool usable = (state == PAPR_BOOT_PENDING) || (state == PAPR_BOOT_VALID);
        if (!usable) { continue; }
        if (!slot_verify(s))
        {
            st.slot_state[s] = PAPR_BOOT_INVALID;   /* tamper / corruption */
            dirty = true;
            continue;
        }
        uint32_t secv = slot_sec_version(s);
        bool pending = (state == PAPR_BOOT_PENDING);
        /* Prefer PENDING (a just-staged update) over VALID; among equals,
         * higher security version wins. */
        if (choice < 0 ||
            (pending && !choice_pending) ||
            (pending == choice_pending && secv > choice_secv))
        {
            choice = (int8_t)s;
            choice_pending = pending;
            choice_secv = secv;
        }
    }

    if (choice < 0)
    {
        if (dirty) { state_write(&st); }
        recovery_loop();
    }

    if (choice_pending)
    {
        st.slot_state[(uint8_t)choice] = PAPR_BOOT_TRYING;   /* one chance */
        st.try_count++;
        dirty = true;
    }
    if (dirty) { state_write(&st); }

    jump_to_app(k_slot_base[(uint8_t)choice]);
    return 0;   /* unreachable */
}

#endif /* GD32E51X */
