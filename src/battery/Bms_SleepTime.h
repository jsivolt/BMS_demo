/**
 *  @file       Bms_SleepTime.h
 *  @brief      Elapsed power-off time provider.
 *
 *  Supplies how long the system was powered down before this boot, which is
 *  what decides whether the cells have relaxed enough for their voltages to be
 *  read as open-circuit voltage (see Bms_Soc / SOC_DESIGN.md 3.8).
 *
 *  This is a hardware-backed input, not application logic: a real
 *  implementation reads an RTC on a supply that survives power-off, an external
 *  timekeeper, or a wake-reason register. It is kept in its own module for the
 *  same reason Bms_Adc and Bms_Ntc are - so the acquisition can be replaced
 *  wholesale (by real hardware, or by a test double) without touching the
 *  estimator that consumes it.
 *
 *  The value is not necessarily available at startup. Bms_SleepTime_IsReady()
 *  reports whether it has been acquired yet, and callers must check it before
 *  trusting Bms_SleepTime_GetElapsed_s().
 */

#ifndef BMS_SLEEPTIME_H
#define BMS_SLEEPTIME_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/**
 * @brief Whether the elapsed power-off time has been acquired and may be read.
 *
 * @return TRUE once Bms_SleepTime_GetElapsed_s() carries a meaningful value.
 *
 * An implementation whose source needs time to settle (RTC start-up, an
 * external read) must return FALSE until acquisition completes. Returning TRUE
 * early lets a consumer evaluate an unpopulated elapsed time.
 */
boolean Bms_SleepTime_IsReady(void);

/**
 * @brief Elapsed time the system was powered off before this boot. Unit: s.
 *
 * Only meaningful once Bms_SleepTime_IsReady() reports TRUE.
 */
uint32 Bms_SleepTime_GetElapsed_s(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SLEEPTIME_H */
