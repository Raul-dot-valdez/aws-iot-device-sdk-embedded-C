#ifndef PAPR_BOOT_SHARED_H
#define PAPR_BOOT_SHARED_H

#include <stdint.h>

/* Layout and metadata shared by the application's OTA path (papr_ota +
 * the per-MCU HAL flash routines) and the standalone bootloader. Both sides
 * MUST agree on these definitions, so they live in one header.
 *
 * The default values describe the reference GD32E517RE map (512 KB flash).
 * Each MCU port may override any of the address / size macros from CMake or
 * its own pinmap header (so the GD32E503CE 256 KB port can shrink slots, for
 * example). The portable bootloader and the app HAL both pick up whatever
 * the build defines.
 *
 * Default (GD32E517RE) layout, 512 KB:
 *
 *   0x08000000  bootloader code        28 KB
 *   0x08007000  boot-state page         2 KB  (mutable, owned by bootloader)
 *   0x08007800  provisioning page       2 KB  (papr_provision)
 *   0x08008000  slot A app            238 KB  + 2 KB signed manifest trailer
 *   0x08044000  slot B app            238 KB  + 2 KB signed manifest trailer
 *   0x08080000  end
 *
 * The signed manifest trailer occupies the last flash page of each slot and
 * is written once by OTA commit. The mutable per-slot boot state lives in
 * the separate boot-state page so it can change without rewriting signed
 * data.
 */

#ifndef PAPR_FLASH_BASE
#define PAPR_FLASH_BASE          0x08000000U
#endif
#ifndef PAPR_BOOT_STATE_ADDR
#define PAPR_BOOT_STATE_ADDR     0x08007000U
#endif
#ifndef PAPR_PROVISION_ADDR
#define PAPR_PROVISION_ADDR      0x08007800U
#endif
#ifndef PAPR_SLOT_A_ADDR
#define PAPR_SLOT_A_ADDR         0x08008000U
#endif
#ifndef PAPR_SLOT_B_ADDR
#define PAPR_SLOT_B_ADDR         0x08044000U
#endif
#ifndef PAPR_SLOT_SIZE
#define PAPR_SLOT_SIZE           (240U * 1024U)
#endif
#ifndef PAPR_FLASH_PAGE
#define PAPR_FLASH_PAGE          2048U
#endif

#define PAPR_SLOT_APP_MAX        (PAPR_SLOT_SIZE - PAPR_FLASH_PAGE)
#define PAPR_MANIFEST_OFFSET     (PAPR_SLOT_SIZE - PAPR_FLASH_PAGE)

#define PAPR_IMG_MAGIC           0x49524150U   /* 'P''A''R''I' (PAPR Image)  */
#define PAPR_SHA256_LEN          32U
#define PAPR_SIG_MAXLEN          64U           /* fits ECDSA-P256 / Ed25519  */

/* Signed manifest, stored in each slot's trailer page. Everything up to and
 * including 'signature' is what the vendor signs (over its SHA-256). */
typedef struct
{
    uint32_t magic;                       /* PAPR_IMG_MAGIC                 */
    uint32_t image_size;                  /* bytes of app image (excl trailer) */
    uint32_t sec_version;                 /* anti-rollback monotonic counter */
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    uint8_t  sig_len;                     /* signature length in bytes       */
    uint8_t  sha256[PAPR_SHA256_LEN];     /* hash of the app image           */
    uint8_t  signature[PAPR_SIG_MAXLEN];  /* over manifest hdr + sha256      */
} papr_img_manifest_t;

/* Per-slot boot lifecycle. Distinct 32-bit tags so a partially-written or
 * bit-flipped value never aliases another state. Plain uint32_t (not an enum)
 * so the 0xFFFFFFFF erased value stays in range under -Wpedantic. */
typedef uint32_t papr_boot_state_t;
#define PAPR_BOOT_EMPTY    0xFFFFFFFFU  /* erased / no image                 */
#define PAPR_BOOT_PENDING  0x50454E44U  /* staged by OTA, never booted       */
#define PAPR_BOOT_TRYING   0x54525901U  /* bootloader gave it its one chance */
#define PAPR_BOOT_VALID    0x564C4944U  /* confirmed healthy by the app      */
#define PAPR_BOOT_INVALID  0x4241441FU  /* rejected: bad sig/hash/boot        */

/* Mutable boot-state record (boot-state page). The bootloader erases+writes
 * this page on each transition; OTA stage and app confirm go through HAL. */
typedef struct
{
    uint32_t magic;                   /* PAPR_IMG_MAGIC                     */
    uint32_t slot_state[2];           /* papr_boot_state_t for slot A / B   */
    uint32_t slot_sec_version[2];     /* last accepted sec_version per slot */
    uint32_t try_count;               /* attempts on the TRYING slot        */
    uint32_t reserved;
} papr_boot_state_record_t;

#define PAPR_BOOT_MAX_TRIES   1U      /* one boot attempt before rollback   */

#endif /* PAPR_BOOT_SHARED_H */
