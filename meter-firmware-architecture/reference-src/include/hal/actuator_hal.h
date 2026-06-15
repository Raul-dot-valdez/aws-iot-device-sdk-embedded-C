/*
 * Actuator HAL - service-disconnect relay, load-limit, and the optical port.
 *
 * The relay and load-limit are the meter's grid-control actuators and are
 * gated by local safety interlocks; commands only ever arrive via authenticated
 * Jobs (06-security-architecture.md section 6). The optical port is the local
 * IEC 62056-21 / ANSI C12.18 infrared interface (09-hardware-platform.md
 * section 4) used for field read/commissioning - a local interface, never the
 * remote billing path.
 */
#ifndef ACTUATOR_HAL_H_
#define ACTUATOR_HAL_H_

#include "../meter_types.h"

typedef enum RelayState
{
    RELAY_OPEN = 0,   /* service disconnected */
    RELAY_CLOSED      /* service connected    */
} RelayState_t;

struct ActuatorInterface;

/* Drive the service relay. Implementation MUST honor interlocks (no close into
 * fault, no operation while tamper asserted) and return METER_ERR_STATE if the
 * requested transition is unsafe. */
typedef MeterStatus_t ( * ActuatorSetRelay_t )( struct ActuatorInterface * pCtx,
                                                RelayState_t state,
                                                const char * pReason );

typedef MeterStatus_t ( * ActuatorGetRelay_t )( struct ActuatorInterface * pCtx,
                                                RelayState_t * pState );

/* Enforce an import load-limit (kW). 0 disables. Commercial backstop for DR /
 * EV-charging envelopes (04-ev-grid-complexity.md section 3). */
typedef MeterStatus_t ( * ActuatorSetLoadLimit_t )( struct ActuatorInterface * pCtx,
                                                    double limitKw );

/* Non-blocking poll of the optical/IR port: returns bytes read (>=0) or error.
 * Used by the local-service handler; isolated from the network stack. */
typedef MeterStatus_t ( * ActuatorOpticalPoll_t )( struct ActuatorInterface * pCtx,
                                                   uint8_t * pBuf, size_t bufLen,
                                                   size_t * pBytesRead );

typedef struct ActuatorInterface
{
    void * pImplCtx;
    ActuatorSetRelay_t     setRelay;
    ActuatorGetRelay_t     getRelay;
    ActuatorSetLoadLimit_t setLoadLimit;
    ActuatorOpticalPoll_t  opticalPoll;
} ActuatorInterface_t;

#endif /* ACTUATOR_HAL_H_ */
