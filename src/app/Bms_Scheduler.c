/**
 *  @file       Bms_Scheduler.c
 *  @brief      Generic table-driven cooperative scheduler for periodic BMS tasks.
 */

#include "Bms_Scheduler.h"

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static const Bms_Scheduler_TaskEntryType *Bms_Scheduler_TaskTable = NULL_PTR;
static uint32 Bms_Scheduler_TaskCount = 0U;
static uint32 Bms_Scheduler_TaskCounters[BMS_SCHEDULER_MAX_TASKS];

/* Set by the ISR, consumed by the main loop. */
static volatile boolean Bms_Scheduler_TickPending = FALSE;

/*==================================================================================================
*                                       FUNCTION DEFINITIONS
==================================================================================================*/

void Bms_Scheduler_Init(const Bms_Scheduler_TaskEntryType * const taskTable, uint32 taskCount)
{
    uint32 index;

    Bms_Scheduler_TaskTable = taskTable;
    Bms_Scheduler_TaskCount = (taskCount > BMS_SCHEDULER_MAX_TASKS) ? BMS_SCHEDULER_MAX_TASKS : taskCount;

    for (index = 0U; index < BMS_SCHEDULER_MAX_TASKS; index++)
    {
        Bms_Scheduler_TaskCounters[index] = 0U;
    }

    Bms_Scheduler_TickPending = FALSE;
}

void Bms_Scheduler_TickFromIsr(void)
{
    Bms_Scheduler_TickPending = TRUE;
}

void Bms_Scheduler_MainFunction(void)
{
    uint32 index;

    if (Bms_Scheduler_TickPending == TRUE)
    {
        Bms_Scheduler_TickPending = FALSE;

        for (index = 0U; index < Bms_Scheduler_TaskCount; index++)
        {
            Bms_Scheduler_TaskCounters[index]++;

            if (Bms_Scheduler_TaskCounters[index] >= Bms_Scheduler_TaskTable[index].periodTicks)
            {
                Bms_Scheduler_TaskCounters[index] = 0U;
                Bms_Scheduler_TaskTable[index].taskFunc();
            }
        }
    }
}
