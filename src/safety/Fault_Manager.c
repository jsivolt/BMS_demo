/**
 *  @file       Fault_Manager.c
 *  @brief      BMS safety layer - fault detection and reporting.
 */

#include "Fault_Manager.h"

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

/** @brief One flag per fault id, TRUE while the fault is latched active. */
static boolean Fault_Manager_ActiveFlags[FAULT_ID_COUNT];

/*==================================================================================================
*                                       FUNCTION DEFINITIONS
==================================================================================================*/

void Fault_Manager_Init(void)
{
    uint32 index;

    for (index = 0U; index < (uint32)FAULT_ID_COUNT; index++)
    {
        Fault_Manager_ActiveFlags[index] = FALSE;
    }
}

void Fault_Manager_ReportFault(Fault_Manager_FaultIdType faultId)
{
    if (faultId < FAULT_ID_COUNT)
    {
        Fault_Manager_ActiveFlags[faultId] = TRUE;
    }
}

void Fault_Manager_ClearFault(Fault_Manager_FaultIdType faultId)
{
    if (faultId < FAULT_ID_COUNT)
    {
        Fault_Manager_ActiveFlags[faultId] = FALSE;
    }
}

boolean Fault_Manager_IsFaultActive(Fault_Manager_FaultIdType faultId)
{
    boolean isActive = FALSE;

    if (faultId < FAULT_ID_COUNT)
    {
        isActive = Fault_Manager_ActiveFlags[faultId];
    }

    return isActive;
}

boolean Fault_Manager_HasActiveFaults(void)
{
    boolean hasFault = FALSE;
    uint32 index;

    for (index = 0U; index < (uint32)FAULT_ID_COUNT; index++)
    {
        if (Fault_Manager_ActiveFlags[index] == TRUE)
        {
            hasFault = TRUE;
            break;
        }
    }

    return hasFault;
}
