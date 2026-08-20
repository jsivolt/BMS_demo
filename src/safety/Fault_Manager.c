#include "Fault_Manager.h"


static volatile FaultMaskType g_ActiveFaults = FAULT_NONE;


/* ---------------------------------------------------------- */

void FaultManager_Init(void)
{
    g_ActiveFaults = FAULT_NONE;
}


/* ---------------------------------------------------------- */

void FaultManager_Set(FaultMaskType fault)
{
    g_ActiveFaults |= fault;
}


/* ---------------------------------------------------------- */

void FaultManager_Clear(FaultMaskType fault)
{
    g_ActiveFaults &= ~fault;
}


/* ---------------------------------------------------------- */

void FaultManager_ClearAll(void)
{
    g_ActiveFaults = FAULT_NONE;
}


/* ---------------------------------------------------------- */

FaultMaskType FaultManager_GetActiveFaults(void)
{
    return g_ActiveFaults;
}


/* ---------------------------------------------------------- */

boolean FaultManager_IsActive(FaultMaskType fault)
{
    return ((g_ActiveFaults & fault) != 0UL)
            ? TRUE
            : FALSE;
}


/* ---------------------------------------------------------- */

boolean FaultManager_HasAnyFault(void)
{
    return (g_ActiveFaults != FAULT_NONE)
            ? TRUE
            : FALSE;
}


/* ---------------------------------------------------------- */

boolean FaultManager_HasCriticalFault(void)
{
    return ((g_ActiveFaults & FAULT_CRITICAL_MASK) != 0UL)
            ? TRUE
            : FALSE;
}