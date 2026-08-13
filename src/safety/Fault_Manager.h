/**
 *  @file       Fault_Manager.h
 *  @brief      BMS safety layer - fault detection and reporting.
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Identifiers for every fault the BMS is able to detect. */
typedef enum
{
    FAULT_ID_OVER_VOLTAGE = 0,
    FAULT_ID_UNDER_VOLTAGE,
    FAULT_ID_OVER_CURRENT,
    FAULT_ID_OVER_TEMPERATURE,
    FAULT_ID_UNDER_TEMPERATURE,
    FAULT_ID_CAN_COMM_LOSS,
    FAULT_ID_COUNT
} Fault_Manager_FaultIdType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the fault manager, clearing every latched fault.
 */
void Fault_Manager_Init(void);

/**
 * @brief Latches a fault as active.
 * @param[in] faultId The fault being reported.
 */
void Fault_Manager_ReportFault(Fault_Manager_FaultIdType faultId);

/**
 * @brief Clears a previously latched fault.
 * @param[in] faultId The fault to clear.
 */
void Fault_Manager_ClearFault(Fault_Manager_FaultIdType faultId);

/**
 * @brief Checks whether a fault is currently latched.
 * @param[in] faultId The fault to query.
 * @return TRUE if the fault is active, FALSE otherwise.
 */
boolean Fault_Manager_IsFaultActive(Fault_Manager_FaultIdType faultId);

/**
 * @brief Returns TRUE if any fault is currently active.
 */
boolean Fault_Manager_HasActiveFaults(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_MANAGER_H */
