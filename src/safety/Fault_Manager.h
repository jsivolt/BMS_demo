#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "Std_Types.h"

typedef uint32 FaultMaskType;

#define FAULT_PACK_COUNT    (3U)

typedef enum
{
    FAULT_PACK_1 = 0U,
    FAULT_PACK_2,
    FAULT_PACK_3
} FaultPackIdType;

/* Fault bits */
#define FAULT_NONE                     (0UL)

#define FAULT_PACK_OV                  (1UL << 0)
#define FAULT_PACK_UV                  (1UL << 1)

#define FAULT_CELL_OV                  (1UL << 2)
#define FAULT_CELL_UV                  (1UL << 3)

#define FAULT_OVER_TEMP                (1UL << 4)
#define FAULT_UNDER_TEMP               (1UL << 5)

#define FAULT_AFE_COMM                 (1UL << 6)
#define FAULT_CAN_TIMEOUT              (1UL << 7)
#define FAULT_SPI_TIMEOUT              (1UL << 8)

#define FAULT_PRECHARGE_TIMEOUT        (1UL << 9)
#define FAULT_CONTACTOR_FEEDBACK       (1UL << 10)
#define FAULT_CONTACTOR_WELD           (1UL << 11)

#define FAULT_OVER_CURRENT             (1UL << 12)

#define FAULT_TEMP_SENSOR              (1UL << 13)
#define FAULT_TEMP_DELTA               (1UL << 14)

/*
 * Add future faults here.
 */


/* Critical faults that must open contactors */
#define FAULT_CRITICAL_MASK            ( \
        FAULT_PACK_OV              |   \
        FAULT_PACK_UV              |   \
        FAULT_CELL_OV              |   \
        FAULT_CELL_UV              |   \
        FAULT_OVER_TEMP            |   \
        FAULT_TEMP_SENSOR          |   \
        FAULT_AFE_COMM             |   \
        FAULT_PRECHARGE_TIMEOUT    |   \
        FAULT_CONTACTOR_FEEDBACK   |   \
        FAULT_CONTACTOR_WELD       |   \
        FAULT_OVER_CURRENT )


void FaultManager_Init(void);

void FaultManager_SetPack(
        FaultPackIdType packId,
        FaultMaskType fault);

void FaultManager_ClearPack(
        FaultPackIdType packId,
        FaultMaskType fault);

void FaultManager_ClearPackAll(
        FaultPackIdType packId);

FaultMaskType FaultManager_GetPackFaults(
        FaultPackIdType packId);

boolean FaultManager_IsPackFaultActive(
        FaultPackIdType packId,
        FaultMaskType fault);

boolean FaultManager_PackHasCriticalFault(
        FaultPackIdType packId);

void FaultManager_SetSystem(FaultMaskType fault);

void FaultManager_ClearSystem(FaultMaskType fault);

void FaultManager_ClearSystemAll(void);

FaultMaskType FaultManager_GetSystemFaults(void);

boolean FaultManager_SystemHasCriticalFault(void);

void FaultManager_Set(FaultMaskType fault);
void FaultManager_Clear(FaultMaskType fault);
void FaultManager_ClearAll(void);

FaultMaskType FaultManager_GetActiveFaults(void);

boolean FaultManager_IsActive(FaultMaskType fault);
boolean FaultManager_HasAnyFault(void);
boolean FaultManager_HasCriticalFault(void);

#endif