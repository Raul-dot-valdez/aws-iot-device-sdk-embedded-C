/*
 * Demand Response / V2G Controller (application module A4).
 *
 * Executes curtailment and V2G authorization windows that arrive as
 * authenticated IoT Jobs, validates them against local safety/tariff envelopes,
 * signals the EVSE/HEMS setpoint, and enforces a load-limit backstop if a
 * non-compliant charger ignores the signal (04-ev-grid-complexity.md sections
 * 3-4, 06-security-architecture.md section 6).
 */
#ifndef DR_V2G_CONTROLLER_H_
#define DR_V2G_CONTROLLER_H_

#include "../meter_types.h"
#include "../hal/actuator_hal.h"
#include "config_variant.h"

typedef enum DrAction
{
    DR_ACTION_LIMIT_IMPORT = 0, /* cap import to setpoint (curtailment)     */
    DR_ACTION_AUTHORIZE_V2G,    /* permit export up to setpoint in window   */
    DR_ACTION_RELEASE           /* end the window, return to normal         */
} DrAction_t;

/* A parsed, authenticated DR/V2G command (from a Job document). */
typedef struct DrCommand
{
    char         jobId[ 48 ];
    char         programId[ 48 ];
    DrAction_t   action;
    double       setpointKw;
    MeterTimeMs_t windowStart;
    MeterTimeMs_t windowEnd;
    double       pricePerKwh;   /* RTP/V2G price context for tariff tagging */
} DrCommand_t;

typedef struct DrController
{
    ConfigManager_t *    pConfig;
    ActuatorInterface_t * pActuator;
    DrCommand_t          active;
    bool                 hasActive;
} DrController_t;

/* Validate a command against safety envelope, feature flags (V2G must be
 * authorized), and tariff. Returns METER_ERR_VALIDATION / METER_ERR_UNSUPPORTED
 * on rejection - the command is never applied unchecked. */
MeterStatus_t Dr_Validate( const DrController_t * pCtrl, const DrCommand_t * pCmd );

/* Apply a validated command: signal the setpoint to the HEMS/EVSE and, where
 * load-limit is available, arm the enforcement backstop. */
MeterStatus_t Dr_Apply( DrController_t * pCtrl, const DrCommand_t * pCmd );

/* Compute compliance for telemetry: was the actual import/export within the
 * authorized envelope this interval? (08-data-model-shadow.md DR payload.) */
bool Dr_IsCompliant( const DrController_t * pCtrl, double actualKw );

#endif /* DR_V2G_CONTROLLER_H_ */
