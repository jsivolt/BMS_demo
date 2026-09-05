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

/* Diagnostics: last/peak elapsed ticks per call, and total ticks skipped by the coalescing logic. */
volatile uint32 g_BmsSchedulerPendingTicks = 0U;
volatile uint32 g_BmsSchedulerPendingTicksMax = 0U;
volatile uint32 g_BmsSchedulerMissedTickCount = 0U;

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

    /*
     * Atomically capture elapsed base ticks.
     */
    OsIf_SuspendAllInterrupts();

    pendingTicks = Bms_Scheduler_PendingTicks;
    Bms_Scheduler_PendingTicks = 0U;

    OsIf_ResumeAllInterrupts();

    g_BmsSchedulerPendingTicks = pendingTicks;

    if (pendingTicks >
        g_BmsSchedulerPendingTicksMax)
    {
        g_BmsSchedulerPendingTicksMax =
            pendingTicks;
    }

    if (pendingTicks > 1U)
    {
        g_BmsSchedulerMissedTickCount +=
            (pendingTicks - 1U);
    }

    if (pendingTicks == 0U)
    {
        return;
    }

    /*
     * Advance scheduler time by all elapsed ticks,
     * but execute each periodic task at most once.
     *
     * This prevents catch-up bursts after the main
     * loop temporarily falls behind.
     */
    for (index = 0U;
         index < Bms_Scheduler_TaskCount;
         index++)
    {
        uint32 period;

        period =
            Bms_Scheduler_TaskTable[index].periodTicks;

        if (period == 0U)
        {
            continue;
        }

        Bms_Scheduler_TaskCounters[index] += pendingTicks;

        if (Bms_Scheduler_TaskCounters[index] >= period)
        {
            /*
             * Preserve phase while skipping missed executions.
             *
             * Example:
             * counter = 3
             * elapsed = 28
             * period  = 10
             *
             * 31 % 10 = 1
             */
            Bms_Scheduler_TaskCounters[index] %= period;

            /*
             * Execute only once, regardless of how many
             * periods elapsed.
             */
            Bms_Scheduler_TaskTable[index].taskFunc();
        }
    }
}
