/*
 * Smart Meter Firmware - common types.
 *
 * Reference interface skeleton accompanying the architecture in
 * meter-firmware-architecture/. These headers are compile-checkable contracts,
 * not a full implementation: they make the layered design of 02-firmware-
 * architecture.md concrete. No silicon, no SDK calls - just the boundaries.
 */
#ifndef METER_TYPES_H_
#define METER_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Uniform status code returned across HAL ports and device services. */
typedef enum MeterStatus
{
    METER_OK = 0,
    METER_ERR_PARAM,       /* invalid argument                         */
    METER_ERR_STATE,       /* operation not valid in current state     */
    METER_ERR_IO,          /* underlying hardware / transport failure  */
    METER_ERR_CRYPTO,      /* signature / verification failure         */
    METER_ERR_NOT_FOUND,   /* key / record absent                      */
    METER_ERR_VALIDATION,  /* schema / envelope validation failed      */
    METER_ERR_UNSUPPORTED, /* feature not enabled for this variant     */
    METER_ERR_BUSY,        /* try again later                          */
    METER_ERR_TIMEOUT
} MeterStatus_t;

/* The firmware-configurable variant (see 03-variant-configuration.md). */
typedef enum MeterVariant
{
    METER_VARIANT_RESIDENTIAL = 0,
    METER_VARIANT_COMMERCIAL
} MeterVariant_t;

/* Physical metrology wiring; calibration-locked to the hardware. */
typedef enum MeterPhases
{
    METER_PHASE_SINGLE = 0,
    METER_PHASE_SPLIT,
    METER_PHASE_THREE
} MeterPhases_t;

/* Feature flags resolved from variant + evReady + gridProfile. Application
 * code queries these instead of using #ifdef (see 03 section 4). */
typedef enum MeterFeature
{
    METER_FEAT_DEMAND_REGISTER = 0,
    METER_FEAT_FOUR_QUADRANT,
    METER_FEAT_LOAD_LIMIT,
    METER_FEAT_V2G_AUTHORIZATION,
    METER_FEAT_PQ_STREAM,
    METER_FEAT_COUNT
} MeterFeature_t;

/* UTC milliseconds since epoch; 0 means time-unsynced. */
typedef uint64_t MeterTimeMs_t;

#endif /* METER_TYPES_H_ */
