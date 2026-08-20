#ifndef BMS_CONTACTOR_CFG_H
#define BMS_CONTACTOR_CFG_H

/*
 * State machine execution period
 */
#define BMS_CONTACTOR_TASK_PERIOD_MS          (10U)


/*
 * Delay after negative contactor closes
 */
#define BMS_CONTACTOR_NEG_DELAY_MS            (100U)


/*
 * Precharge timeout
 */
#define BMS_PRECHARGE_TIMEOUT_MS              (2000U)


/*
 * Bus voltage must reach 90% of Pack voltage
 */
#define BMS_PRECHARGE_COMPLETE_RATIO          (0.90F)


/*
 * Delay after positive contactor closes
 * before opening precharge relay
 */
#define BMS_CONTACTOR_POS_DELAY_MS            (100U)


#endif