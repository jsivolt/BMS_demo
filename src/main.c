/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2020 - 2026 NXP
*
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

#include "Siul2_Port_Ip.h"
#include "Siul2_Port_Ip_Cfg.h"
#include "Bms_Led.h"

static void delay(volatile unsigned int count)
{
    while (count--)
    {
        __asm("nop");
    }
}

int main(void)
{
    Siul2_Port_Ip_Init(
        NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
        g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals
    );

    Bms_Led_Init();

    while (1)
    {
        Bms_Led_On(BMS_LED_GREEN);
        delay(2000000U);
        Bms_Led_Off(BMS_LED_GREEN);

        Bms_Led_On(BMS_LED_YELLOW);
        delay(2000000U);
        Bms_Led_Off(BMS_LED_YELLOW);

        Bms_Led_On(BMS_LED_RED);
        delay(2000000U);
        Bms_Led_Off(BMS_LED_RED);

        Bms_Led_On(BMS_LED_BLUE);
        delay(2000000U);
        Bms_Led_Off(BMS_LED_BLUE);
    }
}

/** @} */
