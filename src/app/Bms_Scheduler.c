/**
 *  @file       Bms_Scheduler.c
 *  @brief      Generic table-driven cooperative scheduler for periodic BMS tasks.
 */

#include "Bms_Scheduler.h"
#include "OsIf.h"

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static const Bms_Scheduler_TaskEntryType *Bms_Scheduler_TaskTable = NULL_PTR;
static uint32 Bms_Scheduler_TaskCount = 0U;
static uint32 Bms_Scheduler_TaskCounters[BMS_SCHEDULER_MAX_TASKS];

/* Incremented by the ISR for every base tick, drained by the main loop. */
static volatile uint32 Bms_Scheduler_PendingTicks = 0U;

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

    Bms_Scheduler_PendingTicks = 0U;
}

void Bms_Scheduler_TickFromIsr(void)
{
    Bms_Scheduler_PendingTicks++;
}

void Bms_Scheduler_MainFunction(void)
{
    uint32 index;
    uint32 pendingTicks;

    /* Atomically capture and clear the accumulated ticks so none are lost. */
    OsIf_SuspendAllInterrupts();
    pendingTicks = Bms_Scheduler_PendingTicks;
    Bms_Scheduler_PendingTicks = 0U;
    OsIf_ResumeAllInterrupts();

    /* Process every elapsed tick, even if the main loop fell behind. */
    while (pendingTicks > 0U)
    {
        pendingTicks--;

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
