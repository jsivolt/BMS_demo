/*==================================================================================================
* Project : S32K344 BMS Demo
* Module  : Bms_PowerManager
*
* Owns everything the MCU needs in order to leave RUN and enter STANDBY, plus the
* classification of the boot reason after a STANDBY wake-up.
*
* STANDBY wake-up is treated as a *reset-style* wake path:
*
*   Power_Ip_SetMode(STANDBY)
*     -> MCU powers down
*     -> PTB26 / WKPU[41] (RTD HW channel 45) wake-up event
*     -> MCU restarts
*     -> main()
*     -> Power_Ip_GetResetReason() == MCU_WAKEUP_REASON (28)
*
* Execution therefore never continues after Power_Ip_SetMode(), and no application
* code may rely on doing so.
==================================================================================================*/

#include "Bms_PowerManager.h"

#include "Clock_Ip.h"
#include "Clock_Ip_Cfg.h"

#include "Pit_Ip.h"
#include "Pit_Ip_Cfg.h"

#include "Wkpu_Ip.h"
#include "Wkpu_Ip_Cfg.h"

#include "Power_Ip.h"
#include "Power_Ip_Cfg.h"

#define PIT_INSTANCE             (0U)
#define PIT_CHANNEL              (0U)

#define MCU_WAKEUP_REASON        ((Power_Ip_ResetType)28U)

static boolean g_SleepRequested = FALSE;

static Bms_PowerBootReasonType g_BootReason =
    BMS_POWER_BOOT_COLD;

void Bms_PowerManager_Init(Power_Ip_ResetType resetReason)
{
    g_SleepRequested = FALSE;

    if (resetReason == MCU_WAKEUP_REASON)
    {
        g_BootReason = BMS_POWER_BOOT_STANDBY_WAKE;
    }
    else
    {
        g_BootReason = BMS_POWER_BOOT_COLD;
    }
}

void Bms_PowerManager_RequestSleep(void)
{
    g_SleepRequested = TRUE;
}

boolean Bms_PowerManager_IsSleepRequested(void)
{
    return g_SleepRequested;
}

Bms_PowerBootReasonType Bms_PowerManager_GetBootReason(void)
{
    return g_BootReason;
}

void Bms_PowerManager_MainFunction(void)
{
    if (g_SleepRequested == FALSE)
    {
        return;
    }

    /*
     * Prevent repeated entry if something unexpected happens.
     */
    g_SleepRequested = FALSE;

    /*
     * Stop scheduler tick before changing clock / entering Standby.
     */
    Pit_Ip_StopChannel(
        PIT_INSTANCE,
        PIT_CHANNEL
    );

    /*
     * Standby clock:
     *
     * CORE      48 MHz
     * AIPS      48 MHz
     * AIPS_SLOW 24 MHz
     * PLL       OFF
     */
    (void)Clock_Ip_Init(
        &Clock_Ip_aClockConfig[1U]
    );

    /*
     * PTB26 -> WKPU[41] -> RTD HW channel 45.
     */
    (void)Wkpu_Ip_Init(
        0U,
        &Wkpu_Ip_Config_PB
    );

    Wkpu_Ip_EnableInterrupt(
        0U,
        Wkpu_Ip_ChannelConfig_PB[0].hwChannel
    );

    /*
     * Enter normal Standby.
     *
     * Successful wake-up restarts the MCU.
     */
    Power_Ip_SetMode(
        &Power_Ip_aModeConfigPB[1U]
    );

    /*
     * Normal application should never rely on execution
     * continuing below here.
     */
    while (1)
    {
    }
}
