/*
 * eFuse / OTP field map and PSA lifecycle - compile-checked descriptor.
 *
 * Representative layout of the GD32W515 one-time-programmable fields that anchor
 * TF-M secure boot, identity, lifecycle, and anti-rollback. See
 * 15-tfm-efuse-secure-boot-provisioning.md. Offsets/sizes are illustrative -
 * confirm against the GD32W515 reference manual and the TF-M port. This header
 * only declares accessors used by the SPE provisioning/boot code; OTP itself is
 * never readable/writable from the non-secure world.
 */
#ifndef EFUSE_MAP_H_
#define EFUSE_MAP_H_

#include "../meter_types.h"

/* PSA device lifecycle state, advanced monotonically via fused transitions. */
typedef enum EfuseLifecycle
{
    LCS_CHIP_MANUFACTURE = 0, /* HUK fused; debug open                       */
    LCS_ASSEMBLY_TEST,        /* board built; pre PSA-RoT provisioning       */
    LCS_SECURED,              /* field state; secure boot + gated debug      */
    LCS_DECOMMISSIONED        /* retired; storage keys zeroized, debug closed */
} EfuseLifecycle_t;

/* Logical OTP fields (a descriptor, not silicon addresses). */
typedef enum EfuseField
{
    EFUSE_ROTPK_HASH = 0,     /* Root-of-Trust public key hash (verify BL2)   */
    EFUSE_HUK,                /* Hardware Unique Key (derives storage keys)   */
    EFUSE_IAK,                /* Initial Attestation Key / seed               */
    EFUSE_IMPLEMENTATION_ID,  /* immutable SoC/impl identity                  */
    EFUSE_INSTANCE_ID,        /* immutable device identity                    */
    EFUSE_DEBUG_AUTH_KEY_HASH,/* ADAC authenticated-debug key hash            */
    EFUSE_AR_COUNTER_BL2,     /* anti-rollback floor: BL2                      */
    EFUSE_AR_COUNTER_SPE,     /* anti-rollback floor: secure image            */
    EFUSE_AR_COUNTER_NSPE,    /* anti-rollback floor: non-secure image        */
    EFUSE_PROV_FLAGS,         /* per-stage provisioning completion flags      */
    EFUSE_FIELD_COUNT
} EfuseField_t;

/* Read the current lifecycle state (callable in SPE only). */
MeterStatus_t Efuse_GetLifecycle( EfuseLifecycle_t * pState );

/* Attempt a monotonic lifecycle advance. Regression or skipping returns
 * METER_ERR_STATE - the transition is enforced by hardware, mirrored here. */
MeterStatus_t Efuse_AdvanceLifecycle( EfuseLifecycle_t target );

/* Verify a candidate image version against the relevant anti-rollback floor.
 * Returns METER_ERR_VALIDATION if version is below the fused minimum. */
MeterStatus_t Efuse_CheckAntiRollback( EfuseField_t counterField,
                                       uint32_t imageVersion );

#endif /* EFUSE_MAP_H_ */
