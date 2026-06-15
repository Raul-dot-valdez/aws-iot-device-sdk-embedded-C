/*
 * Telemetry Service (middleware service M1).
 *
 * Serializes interval reads, PQ events, last-gasp, and DR compliance into the
 * data-model payloads of 08-data-model-shadow.md, signs each revenue record via
 * the Crypto HAL, and enqueues them to the single MQTT agent task
 * (02-firmware-architecture.md section 4). It does not own the coreMQTT context
 * - it hands serialized payloads to the agent (05-aws-iot-integration.md
 * section 2). On network loss, records are journaled and replayed on reconnect.
 */
#ifndef TELEMETRY_SERVICE_H_
#define TELEMETRY_SERVICE_H_

#include "../meter_types.h"
#include "../hal/metrology_hal.h"
#include "../hal/crypto_hal.h"
#include "../hal/storage_hal.h"
#include "config_variant.h"

/* MQTT QoS as used by the telemetry topics (08 section 1). */
typedef enum TelemetryQos
{
    TELEMETRY_QOS0 = 0,
    TELEMETRY_QOS1
} TelemetryQos_t;

/* Opaque enqueue callback into the MQTT agent (decouples from the SDK). */
typedef MeterStatus_t ( * TelemetryPublishFn_t )( const char * pTopic,
                                                  const uint8_t * pPayload,
                                                  size_t payloadLen,
                                                  TelemetryQos_t qos );

typedef struct TelemetryService
{
    ConfigManager_t *    pConfig;     /* selects schema profile res.v1/comm.v1 */
    CryptoInterface_t *  pCrypto;     /* per-record revenue signing            */
    StorageInterface_t * pStorage;    /* store-and-forward journal             */
    TelemetryPublishFn_t publish;     /* enqueue to MQTT agent                 */
} TelemetryService_t;

/* Serialize + sign + publish (or journal) an interval reading. The payload
 * shape follows the active variant's schema profile. */
MeterStatus_t Telemetry_PublishInterval( TelemetryService_t * pSvc,
                                         const MetrologyReading_t * pReading );

/* Publish a last-gasp outage message; called from the high-priority last-gasp
 * task on brown-out, so it must be fast and non-blocking. */
MeterStatus_t Telemetry_PublishLastGasp( TelemetryService_t * pSvc,
                                         MeterTimeMs_t ts );

/* Replay journaled records (original timestamps) after reconnect. */
MeterStatus_t Telemetry_FlushBacklog( TelemetryService_t * pSvc );

#endif /* TELEMETRY_SERVICE_H_ */
