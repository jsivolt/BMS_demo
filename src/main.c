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

/*==================================================================================================
*   S32K344 BMS Demo
*   PIT 10 ms scheduler test
*
*   S32 Design Studio for S32 Platform 3.6.10
*   S32K3 RTD 7.0.1
==================================================================================================*/

#include "Clock_Ip.h"
#include "Clock_Ip_Cfg.h"

#include "Siul2_Port_Ip.h"
#include "Siul2_Port_Ip_Cfg.h"

#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

#include "IntCtrl_Ip.h"
#include "IntCtrl_Ip_Cfg.h"

#include "Pit_Ip.h"
#include "Pit_Ip_Cfg.h"

/* ================================================================================================
 * PIT configuration
 * ============================================================================================== */

#define PIT_INSTANCE            (0U)
#define PIT_CHANNEL             (0U)

/*
 * PIT0 clock = AIPS_SLOW_CLK = 40 MHz
 *
 * 40,000,000 × 0.010 = 400,000 ticks
 *
 * Pit_Ip_StartChannel() internally writes:
 *
 * LDVAL = countValue - 1
 *
 * Therefore pass 400000, not 399999.
 */
#define PIT_10MS_TICKS          (400000U)


/* ================================================================================================
 * LED configuration
 *
 * LED1_RED = PTA29
 *
 * PTA_H_HALF contains PTA16 ... PTA31.
 * PTA29 -> local pin = 29 - 16 = 13
 *
 * LED is active-low:
 * 0 = ON
 * 1 = OFF
 * ============================================================================================== */

#define LED_RED_PORT            PTA_H_HALF
#define LED_RED_PIN             (13U)


/* ================================================================================================
 * Global variables
 * ============================================================================================== */

/*
 * Set by PIT interrupt.
 * Processed in main().
 */
static volatile boolean g_Bms10msFlag = FALSE;

/*
 * Used only to verify PIT timing.
 *
 * 50 × 10 ms = 500 ms
 */
static uint32 g_LedCounter = 0U;

static boolean g_LedOn = FALSE;


/* ================================================================================================
 * PIT callback
 *
 * Generated PIT configuration expects:
 *
 * extern void Bms_Pit10msCallback(uint8 channel);
 * ============================================================================================== */

void Bms_Pit10msCallback(uint8 channel)
{
    (void)channel;

    /*
     * Keep ISR callback very short.
     *
     * Do NOT execute BMS algorithms directly here.
     */
    g_Bms10msFlag = TRUE;
}


/* ================================================================================================
 * BMS 10 ms task
 * ============================================================================================== */

static void Bms_MainFunction_10ms(void)
{
    g_LedCounter++;

    /*
     * Toggle LED every 500 ms.
     *
     * Full ON/OFF cycle = 1 second.
     */
    if (g_LedCounter >= 50U)
    {
        g_LedCounter = 0U;

        if (g_LedOn == FALSE)
        {
            /*
             * Active low:
             * LOW = LED ON
             */
            Siul2_Dio_Ip_WritePin(
                LED_RED_PORT,
                LED_RED_PIN,
                0U
            );

            g_LedOn = TRUE;
        }
        else
        {
            /*
             * HIGH = LED OFF
             */
            Siul2_Dio_Ip_WritePin(
                LED_RED_PORT,
                LED_RED_PIN,
                1U
            );

            g_LedOn = FALSE;
        }
    }
}


/* ================================================================================================
 * main
 * ============================================================================================== */

int main(void)
{
    Pit_Ip_StatusType pitStatus;

    /* ============================================================================================
     * 1. Initialize clocks
     * ========================================================================================== */

    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);


    /* ============================================================================================
     * 2. Initialize pins
     *
     * These are the generated Port settings.
     * ========================================================================================== */

    Siul2_Port_Ip_Init(
        NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
        g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals
    );


    /* ============================================================================================
     * 3. Initial LED state = OFF
     *
     * LED is active-low.
     * ========================================================================================== */

    Siul2_Dio_Ip_WritePin(
        LED_RED_PORT,
        LED_RED_PIN,
        1U
    );


    /* ============================================================================================
     * 4. Initialize interrupt controller
     *
     * Generated configuration:
     *
     * PIT0_IRQn
     * Enable = TRUE
     * Priority = 10
     * Handler = PIT_0_ISR
     * ========================================================================================== */

    IntCtrl_Ip_Init(&IntCtrlConfig_0);


    /* ============================================================================================
     * 5. Initialize PIT0 module
     * ========================================================================================== */

    Pit_Ip_Init(
        PIT_INSTANCE,
        &PIT_0_InitConfig_PB
    );


    /* ============================================================================================
     * 6. Initialize PIT0 Channel 0
     *
     * PIT_0_CH_0 is generated as:
     *
     * #define PIT_0_CH_0 (&PIT_0_ChannelConfig_PB[0U])
     * ========================================================================================== */

    Pit_Ip_InitChannel(
        PIT_INSTANCE,
        PIT_0_CH_0
    );


    /* ============================================================================================
     * 7. Enable PIT Channel 0 interrupt
     *
     * Important:
     * Pit_Ip_InitChannel() disables the channel IRQ while initializing it.
     * So enable it again here.
     * ========================================================================================== */

    Pit_Ip_EnableChannelInterrupt(
        PIT_INSTANCE,
        PIT_CHANNEL
    );


    /* ============================================================================================
     * 8. Start PIT Channel 0
     *
     * 400000 PIT clocks = 10 ms
     * ========================================================================================== */

    pitStatus = Pit_Ip_StartChannel(
        PIT_INSTANCE,
        PIT_CHANNEL,
        PIT_10MS_TICKS
    );


    /* ============================================================================================
     * Optional startup failure indication
     * ========================================================================================== */

    if (pitStatus != PIT_IP_SUCCESS)
    {
        /*
         * PIT failed to start.
         *
         * Turn RED LED permanently ON.
         */
        Siul2_Dio_Ip_WritePin(
            LED_RED_PORT,
            LED_RED_PIN,
            0U
        );

        while (1)
        {
            /* Error trap */
        }
    }


    /* ============================================================================================
     * 9. Main loop
     * ========================================================================================== */

    while (1)
    {
        if (g_Bms10msFlag == TRUE)
        {
            /*
             * Clear first.
             */
            g_Bms10msFlag = FALSE;

            /*
             * Execute 10 ms BMS task.
             */
            Bms_MainFunction_10ms();
        }
    }
}
