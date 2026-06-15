/*
 * Metrology HAL - the contract to the sealed, certified metrology core.
 *
 * The application core treats metrology as an authenticated, read-only input
 * (02-firmware-architecture.md section 1). This interface is deliberately
 * four-quadrant from the start because EV + DER homes need it on every meter
 * (04-ev-grid-complexity.md section 2).
 *
 * Like the SDK's TransportInterface_t, this is a function-pointer struct so the
 * portable layers above never know which silicon is underneath.
 */
#ifndef METROLOGY_HAL_H_
#define METROLOGY_HAL_H_

#include "../meter_types.h"

/* Energy/power for one phase (or aggregate on single-phase meters). */
typedef struct MetrologyPhase
{
    double kWh_import;   /* active energy imported   (Q1/Q2) */
    double kWh_export;   /* active energy exported   (Q3/Q4) */
    double kVARh_q1;     /* reactive, quadrant 1             */
    double kVARh_q2;     /* reactive, quadrant 2             */
    double kVARh_q3;
    double kVARh_q4;
    double kVAh;         /* apparent energy                  */
    double v_rms;        /* RMS voltage                      */
    double i_rms;        /* RMS current                      */
    double pf;           /* power factor (signed)            */
    double thd_pct;      /* total harmonic distortion        */
} MetrologyPhase_t;

/* A complete interval snapshot, sealed and timestamped at the metrology core. */
typedef struct MetrologyReading
{
    MeterTimeMs_t   ts;
    uint32_t        interval_s;
    MeterPhases_t   phases;
    MetrologyPhase_t phase[ 3 ];   /* [0] used on single/split; [0..2] on three */
    double          frequency_hz;
    bool            reverse_power; /* net export this interval                  */
} MetrologyReading_t;

struct MetrologyInterface;

/* Read the latest sealed interval snapshot. */
typedef MeterStatus_t ( * MetrologyGetInterval_t )( struct MetrologyInterface * pCtx,
                                                    MetrologyReading_t * pOut );

/* Read instantaneous demand (sliding/block kW & kVA) for the demand register. */
typedef MeterStatus_t ( * MetrologyGetDemand_t )( struct MetrologyInterface * pCtx,
                                                  double * pPeakKw,
                                                  double * pPeakKva );

/* Verify the metrology core's seal/signature over a reading (revenue trust). */
typedef MeterStatus_t ( * MetrologyVerifySeal_t )( struct MetrologyInterface * pCtx,
                                                   const MetrologyReading_t * pReading );

typedef struct MetrologyInterface
{
    void * pImplCtx;                     /* port-private context */
    MetrologyGetInterval_t getInterval;
    MetrologyGetDemand_t   getDemand;
    MetrologyVerifySeal_t  verifySeal;
} MetrologyInterface_t;

#endif /* METROLOGY_HAL_H_ */
