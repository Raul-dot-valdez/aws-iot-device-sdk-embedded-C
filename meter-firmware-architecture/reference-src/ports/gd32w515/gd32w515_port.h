/*
 * GD32W515 port - factory bindings for the HAL interfaces.
 *
 * Concrete instantiation of the application-core for the GigaDevice GD32W515
 * (Cortex-M33, TrustZone, integrated Wi-Fi). See 12-gigadevice-gd32w515-
 * platform.md. These are DECLARATIONS only: the implementations live in the
 * (non-portable) .c files that call into the GD32W51x SDK - mbedTLS, lwIP, the
 * Wi-Fi driver, FreeRTOS, the GD32W51x Firmware Library, and TF-M PSA services.
 *
 * Moving to/from another Cortex-M33 part replaces only this port - the layers
 * above the HAL are unchanged (09-hardware-platform.md section 3).
 */
#ifndef GD32W515_PORT_H_
#define GD32W515_PORT_H_

#include "../../include/hal/metrology_hal.h"
#include "../../include/hal/crypto_hal.h"
#include "../../include/hal/storage_hal.h"
#include "../../include/hal/actuator_hal.h"

/* Metrology HAL bound to the GD32W51x SPI/UART driver talking to the sealed
 * metrology AFE (e.g. ADI ADE9000 / TI MSP430 metering). */
MetrologyInterface_t * GD32W515_MetrologyPort( void );

/* Crypto HAL bound to TF-M PSA Crypto + the GD32W515 hardware accelerator
 * (AES/HASH/PKCAU/TRNG); device key held non-exportable in Secure Storage/eFuse. */
CryptoInterface_t * GD32W515_CryptoPort( void );

/* Storage HAL bound to the GD32W51x QSPI NOR driver (A/B slots, journaled
 * registers) with the audit-log root anchored in TF-M Secure Storage. */
StorageInterface_t * GD32W515_StoragePort( void );

/* Actuator HAL bound to GD32W51x GPIO/timer (relay, load-limit) and UART
 * (IEC 62056-21 optical port). */
ActuatorInterface_t * GD32W515_ActuatorPort( void );

#endif /* GD32W515_PORT_H_ */
