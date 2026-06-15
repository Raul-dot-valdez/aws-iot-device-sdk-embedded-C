/*
 * Storage HAL - persistence with power-fail-atomic register journaling.
 *
 * Billing registers and the event log are append-only and integrity-protected
 * (02-firmware-architecture.md section 6). This interface separates a simple
 * key/value store (config, shadow cache) from a journaled record store (revenue
 * registers, audit log) whose writes must be atomic across power loss.
 */
#ifndef STORAGE_HAL_H_
#define STORAGE_HAL_H_

#include "../meter_types.h"

struct StorageInterface;

/* Key/value: variant, grid profile, calibration, shadow cache. */
typedef MeterStatus_t ( * StorageKvGet_t )( struct StorageInterface * pCtx,
                                            const char * pKey,
                                            uint8_t * pBuf, size_t bufLen,
                                            size_t * pOutLen );

typedef MeterStatus_t ( * StorageKvSet_t )( struct StorageInterface * pCtx,
                                            const char * pKey,
                                            const uint8_t * pData, size_t dataLen );

/* Journaled append: write is atomic (all-or-nothing) across brown-out. The
 * record is expected to carry its own integrity tag from the Crypto HAL. */
typedef MeterStatus_t ( * StorageJournalAppend_t )( struct StorageInterface * pCtx,
                                                    const char * pStream,
                                                    const uint8_t * pRecord,
                                                    size_t recordLen );

/* Flush any staged journal entries (e.g. before sleep / after reconnect). */
typedef MeterStatus_t ( * StorageJournalFlush_t )( struct StorageInterface * pCtx,
                                                   const char * pStream );

typedef struct StorageInterface
{
    void * pImplCtx;
    StorageKvGet_t         kvGet;
    StorageKvSet_t         kvSet;
    StorageJournalAppend_t journalAppend;
    StorageJournalFlush_t  journalFlush;
} StorageInterface_t;

#endif /* STORAGE_HAL_H_ */
