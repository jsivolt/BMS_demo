/**
 *  @file       Bms_Scheduler.h
 *  @brief      Generic table-driven cooperative scheduler for periodic BMS tasks.
 */

#ifndef BMS_SCHEDULER_H
#define BMS_SCHEDULER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       LOCAL DEFINES
==================================================================================================*/

/** @brief Maximum number of task entries the scheduler can hold. */
#define BMS_SCHEDULER_MAX_TASKS  (8U)

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Signature of a periodic task function. */
typedef void (*Bms_Scheduler_TaskFuncType)(void);

/** @brief One task table entry: function to run and its period, in base tick units. */
typedef struct
{
    Bms_Scheduler_TaskFuncType taskFunc;
    uint32                     periodTicks;
} Bms_Scheduler_TaskEntryType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Registers the application task table and resets all internal counters.
 * @param[in] taskTable Application-owned array of task entries (must outlive the scheduler).
 * @param[in] taskCount Number of entries in taskTable, truncated to BMS_SCHEDULER_MAX_TASKS.
 */
void Bms_Scheduler_Init(const Bms_Scheduler_TaskEntryType * const taskTable, uint32 taskCount);

/**
 * @brief Signals that one base tick has elapsed.
 * @note  Call this from the timer ISR only; keep it as short as possible.
 */
void Bms_Scheduler_TickFromIsr(void);

/**
 * @brief Dispatches every due task; call this from the main loop.
 */
void Bms_Scheduler_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SCHEDULER_H */
