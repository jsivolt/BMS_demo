/**
 *  @file       Bms_SleepTime.c
 *  @brief      Elapsed power-off time provider - placeholder implementation.
 *
 *  No timekeeping source survives power-off on this hardware yet, so this
 *  reports "ready immediately, zero elapsed": the value is a compile-time
 *  constant, which is genuinely available from the first call, and zero keeps
 *  the SOC startup OCV reset disabled. See SOC_DESIGN.md 5.2.
 */

#include "Bms_SleepTime.h"


boolean Bms_SleepTime_IsReady(void)
{
    /*
     * TODO: replace together with Bms_SleepTime_GetElapsed_s().
     *
     * A constant is readable at once, so this reports TRUE from the first call.
     * A real source (RTC, external timekeeper, wake-reason register) MUST
     * return FALSE until its value has actually been acquired - Bms_Soc uses
     * this as the first term of its OCV eligibility test and as the stage 1
     * gate of the deferred startup wait, so reporting TRUE early would let the
     * OCV decision be made against an unpopulated elapsed time.
     */
    return TRUE;
}


uint32 Bms_SleepTime_GetElapsed_s(void)
{
    /*
     * TODO: no timekeeping source survives power-off on this hardware yet -
     * see SOC_DESIGN.md 5.2. Hardcoded to 0 so the OCV-reset branch in
     * Bms_Soc_InitPack() is never taken and every boot restores from NVM.
     */
    return 0UL;
}
