/*
 * Meter secure-service API - the Non-Secure-Callable (NSC) boundary.
 *
 * The single audited entry surface from the non-secure meter app into the
 * GD32W515 secure world (TF-M). See 14-tfm-secure-partition-layout.md. The app
 * may *ask*; the secure world *decides* - each call validates policy/identity
 * inside the SPE before acting.
 *
 * In a TF-M build these map to PSA secure-service calls and return psa_status_t
 * across the veneer; here they use MeterStatus_t so the contract is
 * self-contained and CI-compilable without the TF-M / PSA headers.
 */
#ifndef METER_SECURE_API_H_
#define METER_SECURE_API_H_

#include "../meter_types.h"

/* --- Metering Trust Service (App Root of Trust) --------------------------- */

/* Verify the sealed metrology reading and produce a revenue signature over the
 * canonical payload, using the device key held in the SPE. The private key never
 * crosses into the non-secure world. */
MeterStatus_t meter_secure_sign_revenue( const uint8_t * pCanonicalPayload,
                                         size_t payloadLen,
                                         uint8_t * pSig, size_t * pSigLen );

/* --- Actuator Guard (App Root of Trust) ---------------------------------- */

typedef enum SecureActuator
{
    SECURE_ACT_RELAY_OPEN = 0,
    SECURE_ACT_RELAY_CLOSE,
    SECURE_ACT_LOAD_LIMIT,      /* setpoint in pParam (kW)            */
    SECURE_ACT_DR_V2G_WINDOW    /* envelope proof in pJobProof        */
} SecureActuator_t;

/* Request an actuator operation. The SPE verifies pJobProof (the signature of an
 * authenticated IoT Job), checks interlocks, policy, and rate-limit, and only
 * then drives the secure-attributed GPIO. Returns METER_ERR_VALIDATION /
 * METER_ERR_STATE on rejection - the non-secure app cannot bypass this. */
MeterStatus_t meter_secure_actuate( SecureActuator_t action,
                                    double param,
                                    const uint8_t * pJobProof, size_t proofLen );

/* --- Initial Attestation (PSA Root of Trust) ----------------------------- */

/* Produce a signed device attestation token (EAT) reflecting identity and boot
 * state, for the cloud to verify on connect. */
MeterStatus_t meter_secure_get_attestation( const uint8_t * pChallenge,
                                            size_t challengeLen,
                                            uint8_t * pToken, size_t * pTokenLen );

/* --- Tamper Monitor (App Root of Trust) ---------------------------------- */

/* Read the latched, signed tamper status. NSPE can read but cannot clear it. */
MeterStatus_t meter_secure_get_tamper( uint32_t * pTamperFlags );

#endif /* METER_SECURE_API_H_ */
