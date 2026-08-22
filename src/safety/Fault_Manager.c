#include "Fault_Manager.h"

static volatile FaultMaskType
    g_PackFaults[FAULT_PACK_COUNT];

static volatile FaultMaskType
    g_SystemFaults = FAULT_NONE;

/* ---------------------------------------------------------- */

void FaultManager_Init(void)
{
    uint32 i;

    for (i = 0U; i < FAULT_PACK_COUNT; i++)
    {
        g_PackFaults[i] = FAULT_NONE;
    }

    g_SystemFaults = FAULT_NONE;
}

/* ---------------------------------------------------------- */
/* Pack-specific faults                                      */
/* ---------------------------------------------------------- */

void FaultManager_SetPack(
        FaultPackIdType packId,
        FaultMaskType fault)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return;
    }

    g_PackFaults[packId] |= fault;
}

void FaultManager_ClearPack(
        FaultPackIdType packId,
        FaultMaskType fault)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return;
    }

    g_PackFaults[packId] &= ~fault;
}

void FaultManager_ClearPackAll(
        FaultPackIdType packId)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return;
    }

    g_PackFaults[packId] = FAULT_NONE;
}

FaultMaskType FaultManager_GetPackFaults(
        FaultPackIdType packId)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return FAULT_NONE;
    }

    return g_PackFaults[packId];
}

boolean FaultManager_IsPackFaultActive(
        FaultPackIdType packId,
        FaultMaskType fault)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return FALSE;
    }

    return ((g_PackFaults[packId] & fault) != 0UL)
            ? TRUE
            : FALSE;
}

boolean FaultManager_PackHasCriticalFault(
        FaultPackIdType packId)
{
    if ((uint32)packId >= FAULT_PACK_COUNT)
    {
        return TRUE;
    }

    return ((g_PackFaults[packId] &
             FAULT_CRITICAL_MASK) != 0UL)
            ? TRUE
            : FALSE;
}

/* ---------------------------------------------------------- */
/* System faults                                             */
/* ---------------------------------------------------------- */

void FaultManager_SetSystem(FaultMaskType fault)
{
    g_SystemFaults |= fault;
}

void FaultManager_ClearSystem(FaultMaskType fault)
{
    g_SystemFaults &= ~fault;
}

void FaultManager_ClearSystemAll(void)
{
    g_SystemFaults = FAULT_NONE;
}

boolean FaultManager_IsSystemFaultActive(FaultMaskType fault)
{
    return ((g_SystemFaults & fault) != 0UL)
            ? TRUE
            : FALSE;
}

FaultMaskType FaultManager_GetSystemFaults(void)
{
    return g_SystemFaults;
}

boolean FaultManager_SystemHasCriticalFault(void)
{
    return ((g_SystemFaults &
             FAULT_CRITICAL_MASK) != 0UL)
            ? TRUE
            : FALSE;
}

/* ---------------------------------------------------------- */
/* Legacy API - temporarily mapped to system faults           */
/* ---------------------------------------------------------- */

void FaultManager_Set(FaultMaskType fault)
{
    FaultManager_SetSystem(fault);
}

void FaultManager_Clear(FaultMaskType fault)
{
    FaultManager_ClearSystem(fault);
}

void FaultManager_ClearAll(void)
{
    uint32 i;

    for (i = 0U; i < FAULT_PACK_COUNT; i++)
    {
        g_PackFaults[i] = FAULT_NONE;
    }

    g_SystemFaults = FAULT_NONE;
}

FaultMaskType FaultManager_GetActiveFaults(void)
{
    return g_SystemFaults;
}

boolean FaultManager_IsActive(FaultMaskType fault)
{
    return ((g_SystemFaults & fault) != 0UL)
            ? TRUE
            : FALSE;
}

boolean FaultManager_HasAnyFault(void)
{
    uint32 i;

    if (g_SystemFaults != FAULT_NONE)
    {
        return TRUE;
    }

    for (i = 0U; i < FAULT_PACK_COUNT; i++)
    {
        if (g_PackFaults[i] != FAULT_NONE)
        {
            return TRUE;
        }
    }

    return FALSE;
}

boolean FaultManager_HasCriticalFault(void)
{
    uint32 i;

    if (FaultManager_SystemHasCriticalFault() == TRUE)
    {
        return TRUE;
    }

    for (i = 0U; i < FAULT_PACK_COUNT; i++)
    {
        if (FaultManager_PackHasCriticalFault(
                (FaultPackIdType)i) == TRUE)
        {
            return TRUE;
        }
    }

    return FALSE;
}