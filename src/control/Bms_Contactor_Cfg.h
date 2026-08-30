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


/*
 * Pack1 relay outputs.
 *
 * PGPDO5 (PTC_H_HALF) covers GPIO80..95, so index = GPIO - 80.
 *
 * PTC23 = GPIO87 -> index 7   Negative contactor
 * PTC24 = GPIO88 -> index 8   Precharge relay
 * PTC25 = GPIO89 -> index 9   Positive contactor
 *
 * Pack2 / Pack3 have no relay hardware yet.
 */
#define BMS_CONTACTOR_P1_NEG_PORT             PTC_H_HALF
#define BMS_CONTACTOR_P1_NEG_PIN              (7U)

#define BMS_CONTACTOR_P1_PRE_PORT             PTC_H_HALF
#define BMS_CONTACTOR_P1_PRE_PIN              (8U)

#define BMS_CONTACTOR_P1_POS_PORT             PTC_H_HALF
#define BMS_CONTACTOR_P1_POS_PIN              (9U)


/*
 * Relay drive polarity: 1 = active high
 */
#define BMS_CONTACTOR_OUTPUT_ACTIVE_LEVEL     (1U)


#endif