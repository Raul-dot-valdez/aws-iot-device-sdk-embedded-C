/*
 * build_check.c - compile-time coherence check for the reference interfaces.
 *
 * This is NOT the firmware. It includes every header and exercises each type and
 * prototype so CI can prove the contracts compile and link as a consistent set.
 * The function bodies are trivial stubs standing in for real ports/services.
 *
 * Build:  cc -std=c99 -Wall -Wextra -I../include build_check.c -o build_check
 */
#include "hal/metrology_hal.h"
#include "hal/crypto_hal.h"
#include "hal/storage_hal.h"
#include "hal/actuator_hal.h"
#include "services/config_variant.h"
#include "services/dr_v2g_controller.h"
#include "services/telemetry_service.h"

/* Concrete GD32W515 port declarations - proves the port header stays coherent
 * with the HAL contracts (12-gigadevice-gd32w515-platform.md). Declaration-only,
 * so no GD32W51x SDK is needed in CI. */
#include "../ports/gd32w515/gd32w515_port.h"

/* Secure-world NSC boundary (14-tfm-secure-partition-layout.md). */
#include "secure/meter_secure_api.h"

/* eFuse/OTP descriptor + PSA lifecycle (15-tfm-efuse-secure-boot-provisioning.md). */
#include "secure/efuse_map.h"

#include <string.h>
#include <stdio.h>

/* --- trivial HAL port stubs (one per interface function) ------------------ */

static MeterStatus_t stub_getInterval( MetrologyInterface_t * c, MetrologyReading_t * o )
{
    ( void ) c;
    memset( o, 0, sizeof( *o ) );
    o->phases = METER_PHASE_SPLIT;
    o->interval_s = 900U;
    return METER_OK;
}

static MeterStatus_t stub_setRelay( ActuatorInterface_t * c, RelayState_t s, const char * r )
{
    ( void ) c; ( void ) s; ( void ) r;
    return METER_OK; /* a real port enforces interlocks here */
}

/* --- minimal service stubs so prototypes are linked ----------------------- */

MeterStatus_t Config_LoadPersisted( ConfigManager_t * m ) { ( void ) m; return METER_OK; }

MeterStatus_t Config_Validate( const ConfigManager_t * m, const MeterConfig_t * c )
{
    ( void ) m;
    /* EV-ready meters must run four-quadrant accounting (03 / schema rule). */
    if( c->evReady && !c->features[ METER_FEAT_FOUR_QUADRANT ] )
    {
        return METER_ERR_VALIDATION;
    }
    return METER_OK;
}

MeterStatus_t Config_Apply( ConfigManager_t * m, const MeterConfig_t * c )
{
    m->active = *c;
    m->valid = true;
    return METER_OK;
}

bool Config_FeatureEnabled( const ConfigManager_t * m, MeterFeature_t f )
{
    return m->valid && f < METER_FEAT_COUNT && m->active.features[ f ];
}

MeterStatus_t Dr_Validate( const DrController_t * ctrl, const DrCommand_t * cmd )
{
    if( cmd->action == DR_ACTION_AUTHORIZE_V2G &&
        !Config_FeatureEnabled( ctrl->pConfig, METER_FEAT_V2G_AUTHORIZATION ) )
    {
        return METER_ERR_UNSUPPORTED;
    }
    return METER_OK;
}

MeterStatus_t Dr_Apply( DrController_t * ctrl, const DrCommand_t * cmd )
{
    ctrl->active = *cmd;
    ctrl->hasActive = true;
    return ctrl->pActuator->setRelay ? METER_OK : METER_ERR_STATE;
}

bool Dr_IsCompliant( const DrController_t * ctrl, double actualKw )
{
    return !ctrl->hasActive || actualKw <= ctrl->active.setpointKw;
}

MeterStatus_t Telemetry_PublishInterval( TelemetryService_t * s, const MetrologyReading_t * r )
{
    ( void ) r;
    return s->publish ? METER_OK : METER_ERR_STATE;
}

MeterStatus_t Telemetry_PublishLastGasp( TelemetryService_t * s, MeterTimeMs_t ts )
{
    ( void ) ts;
    return s->publish ? METER_OK : METER_ERR_STATE;
}

MeterStatus_t Telemetry_FlushBacklog( TelemetryService_t * s ) { ( void ) s; return METER_OK; }

/* --- secure-world NSC stubs (SPE side; real impl lives in TF-M partitions) - */

MeterStatus_t meter_secure_sign_revenue( const uint8_t * p, size_t n,
                                         uint8_t * pSig, size_t * pSigLen )
{
    ( void ) p; ( void ) n; ( void ) pSig;
    if( pSigLen ) { *pSigLen = 0U; }
    return METER_OK;
}

MeterStatus_t meter_secure_actuate( SecureActuator_t action, double param,
                                    const uint8_t * pProof, size_t proofLen )
{
    ( void ) param;
    /* SPE rejects an unproven command - the app cannot bypass policy. */
    if( ( pProof == NULL ) || ( proofLen == 0U ) )
    {
        return METER_ERR_VALIDATION;
    }
    return ( action <= SECURE_ACT_DR_V2G_WINDOW ) ? METER_OK : METER_ERR_PARAM;
}

MeterStatus_t meter_secure_get_attestation( const uint8_t * pC, size_t cn,
                                            uint8_t * pTok, size_t * pTokLen )
{
    ( void ) pC; ( void ) cn; ( void ) pTok;
    if( pTokLen ) { *pTokLen = 0U; }
    return METER_OK;
}

MeterStatus_t meter_secure_get_tamper( uint32_t * pFlags )
{
    if( pFlags ) { *pFlags = 0U; }
    return METER_OK;
}

/* --- eFuse stubs: enforce monotonic lifecycle + anti-rollback floor ------- */

static EfuseLifecycle_t g_lcs = LCS_SECURED;     /* a deployed field device   */
static uint32_t g_arFloorNspe = 5U;              /* fused min NSPE version     */

MeterStatus_t Efuse_GetLifecycle( EfuseLifecycle_t * pState )
{
    if( !pState ) { return METER_ERR_PARAM; }
    *pState = g_lcs;
    return METER_OK;
}

MeterStatus_t Efuse_AdvanceLifecycle( EfuseLifecycle_t target )
{
    if( target <= g_lcs ) { return METER_ERR_STATE; } /* never regress/skip-back */
    g_lcs = target;
    return METER_OK;
}

MeterStatus_t Efuse_CheckAntiRollback( EfuseField_t counterField, uint32_t version )
{
    ( void ) counterField;
    return ( version >= g_arFloorNspe ) ? METER_OK : METER_ERR_VALIDATION;
}

/* --- wiring sanity: build a config, validate, query a feature, run DR ----- */

int main( void )
{
    ConfigManager_t cfg = { 0 };
    MeterConfig_t res = { 0 };

    res.variant = METER_VARIANT_RESIDENTIAL;
    strcpy( res.gridProfile, "us-ca-pge" );
    res.evReady = true;
    res.reportIntervalS = 900U;
    res.phases = METER_PHASE_SPLIT;
    res.ctRatio = 1.0;
    res.features[ METER_FEAT_FOUR_QUADRANT ] = true;     /* required by evReady */
    res.features[ METER_FEAT_V2G_AUTHORIZATION ] = true;

    if( Config_Validate( &cfg, &res ) != METER_OK ) { return 1; }
    Config_Apply( &cfg, &res );

    MetrologyInterface_t metrology = { 0 };
    metrology.getInterval = stub_getInterval;
    MetrologyReading_t reading;
    metrology.getInterval( &metrology, &reading );

    ActuatorInterface_t actuator = { 0 };
    actuator.setRelay = stub_setRelay;

    DrController_t dr = { 0 };
    dr.pConfig = &cfg;
    dr.pActuator = &actuator;

    DrCommand_t cmd = { 0 };
    cmd.action = DR_ACTION_AUTHORIZE_V2G;
    cmd.setpointKw = 6.0;
    if( Dr_Validate( &dr, &cmd ) == METER_OK )
    {
        Dr_Apply( &dr, &cmd );
    }

    /* Exercise the secure NSC boundary: an unproven actuate is rejected, a
     * proven one is accepted (14-tfm-secure-partition-layout.md). */
    const uint8_t proof[ 8 ] = { 0 };
    int actDenied = ( meter_secure_actuate( SECURE_ACT_RELAY_OPEN, 0.0, NULL, 0 )
                      == METER_ERR_VALIDATION );
    int actOk = ( meter_secure_actuate( SECURE_ACT_RELAY_OPEN, 0.0,
                                        proof, sizeof( proof ) ) == METER_OK );
    uint32_t tamper = 0xFFFFFFFFu;
    meter_secure_get_tamper( &tamper );

    /* eFuse rules: lifecycle cannot regress; downgrade below the floor fails. */
    EfuseLifecycle_t lcs = LCS_CHIP_MANUFACTURE;
    Efuse_GetLifecycle( &lcs );
    int noRegress = ( Efuse_AdvanceLifecycle( LCS_ASSEMBLY_TEST ) == METER_ERR_STATE );
    int rollbackBlocked = ( Efuse_CheckAntiRollback( EFUSE_AR_COUNTER_NSPE, 4U )
                            == METER_ERR_VALIDATION );
    int upgradeOk = ( Efuse_CheckAntiRollback( EFUSE_AR_COUNTER_NSPE, 6U ) == METER_OK );

    printf( "build-check OK: variant=%d evReady=%d v2g=%d compliant@5kW=%d "
            "secure(denyUnproven=%d allowProven=%d tamper=%u) "
            "efuse(lcs=%d noRegress=%d rollbackBlocked=%d upgradeOk=%d)\n",
            ( int ) cfg.active.variant,
            ( int ) cfg.active.evReady,
            ( int ) Config_FeatureEnabled( &cfg, METER_FEAT_V2G_AUTHORIZATION ),
            ( int ) Dr_IsCompliant( &dr, 5.0 ),
            actDenied, actOk, ( unsigned ) tamper,
            ( int ) lcs, noRegress, rollbackBlocked, upgradeOk );
    return 0;
}
