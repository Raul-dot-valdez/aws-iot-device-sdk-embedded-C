/*
 * Config & Variant Manager (middleware service M7).
 *
 * Resolves the active configuration from factory defaults, provisioning
 * profile, local persisted config, and the Device Shadow desired state - in
 * that precedence (03-variant-configuration.md section 2) - validates it, and
 * exposes the feature-flag matrix the application layer queries. This is the
 * one place the residential/commercial split is decided at runtime.
 */
#ifndef CONFIG_VARIANT_H_
#define CONFIG_VARIANT_H_

#include "../meter_types.h"
#include "../hal/storage_hal.h"

/* Resolved, validated runtime configuration. */
typedef struct MeterConfig
{
    MeterVariant_t variant;
    char           gridProfile[ 32 ];   /* e.g. "us-ca-pge" */
    bool           evReady;
    uint32_t       reportIntervalS;
    MeterPhases_t  phases;
    double         ctRatio;
    bool           features[ METER_FEAT_COUNT ];
} MeterConfig_t;

typedef struct ConfigManager
{
    StorageInterface_t * pStorage;
    MeterConfig_t        active;
    bool                 valid;
} ConfigManager_t;

/* Load the last-good config from storage at boot (offline fallback). */
MeterStatus_t Config_LoadPersisted( ConfigManager_t * pMgr );

/* Validate a candidate config (schema + hardware capability checks). On success
 * the caller may apply it at a safe billing boundary (03 section 6). Invalid
 * candidates are rejected so the application never runs on bad config. */
MeterStatus_t Config_Validate( const ConfigManager_t * pMgr,
                               const MeterConfig_t * pCandidate );

/* Atomically activate a validated candidate and persist it. */
MeterStatus_t Config_Apply( ConfigManager_t * pMgr,
                            const MeterConfig_t * pCandidate );

/* Feature query used by application modules instead of #ifdef (03 section 4). */
bool Config_FeatureEnabled( const ConfigManager_t * pMgr, MeterFeature_t feature );

#endif /* CONFIG_VARIANT_H_ */
