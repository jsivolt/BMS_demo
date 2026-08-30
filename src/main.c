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

#include "Bms_Scheduler.h"
#include "Bms_App.h"
#include "Battery_Monitor.h"

#include "Bms_Adc.h"
#include "Bms_Ntc.h"
#include "Bms_Soc.h"
#include "battery/vAFE/Bms_Vafe.h"
#include "battery/vPACK/Bms_Vpack.h"
#include "communication/Bms_Can.h"
#include "communication/Bms_Spi.h"
#include "storage/Bms_Nvm.h"

#include "Bms_StateMachine.h"

#include "safety/Fault_Manager.h"
#include "control/Bms_Contactor.h"

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
 * Fault register addresses (Cortex-M7 SCB)
 * ============================================================================================== */

#define SCB_HFSR_ADDR   (0xE000ED2CUL)
#define SCB_CFSR_ADDR   (0xE000ED28UL)
#define SCB_MMFAR_ADDR  (0xE000ED34UL)
#define SCB_BFAR_ADDR   (0xE000ED38UL)
#define SCB_AFSR_ADDR   (0xE000ED3CUL)

#define REG32(addr)     (*(volatile uint32 *)(addr))


/* ================================================================================================
 * Global variables
 * ============================================================================================== */

/*
 * Used only to verify PIT timing.
 *
 * 50 × 10 ms = 500 ms
 */
static uint32 g_LedCounter = 0U;

static boolean g_LedOn = FALSE;

/* Fault registers captured by HardFault_Handler for post-mortem debugging */
volatile uint32 g_HardFault_HFSR  = 0U;
volatile uint32 g_HardFault_CFSR  = 0U;
volatile uint32 g_HardFault_BFAR  = 0U;
volatile uint32 g_HardFault_MMFAR = 0U;
volatile uint32 g_HardFault_AFSR  = 0U;

/* ================================================================================================
 * Fault handlers
 * ============================================================================================== */

void HardFault_Handler(void)
{
    g_HardFault_HFSR  = REG32(SCB_HFSR_ADDR);
    g_HardFault_CFSR  = REG32(SCB_CFSR_ADDR);
    g_HardFault_MMFAR = REG32(SCB_MMFAR_ADDR);
    g_HardFault_BFAR  = REG32(SCB_BFAR_ADDR);
    g_HardFault_AFSR  = REG32(SCB_AFSR_ADDR);

    while (TRUE)
    {
        /* Inspect in debugger */
    }
}

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
    Bms_Scheduler_TickFromIsr();
}


/* ================================================================================================
 * BMS 10 ms task
 * ============================================================================================== */

static void Bms_MainFunction_10ms(void)
{
    Bms_Adc_MainFunction();

    Bms_App_MainFunction();

    /* Millivolts; Bms_Contactor_SetBusVoltage() must use the same unit. */
    if (Bms_Adc_IsPackValid() == TRUE)
    {
        Bms_Contactor_SetPackVoltage(
            BMS_PACK_1,
            (float)Bms_Adc_GetPackV1VoltageMv());

        Bms_Contactor_SetPackVoltage(
            BMS_PACK_2,
            (float)Bms_Adc_GetPackV2VoltageMv());

        Bms_Contactor_SetPackVoltage(
            BMS_PACK_3,
            (float)Bms_Adc_GetPackV3VoltageMv());
    }

    Bms_Contactor_MainFunction();

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
 * BMS 100 ms / 1000 ms tasks (placeholders for future use)
 * ============================================================================================== */

static void Bms_MainFunction_100ms(void)
{
    Bms_Ntc_MainFunction();

    /*
     * CAN RX (Bms_Vpack_ProcessFrame) must run before the vPACK
     * comm-health check, which must run before the battery monitor
     * consumes g_BmsVpackData.
     */
    Bms_Can_MainFunction();

    Bms_Vpack_MainFunction();

    BatteryMonitor_MainFunction();

    Bms_Soc_MainFunction();

    Bms_StateMachine_MainFunction();

    Bms_Can_SendStatus();

    Bms_Can_SendPackStatus();

    Bms_Can_SendContactorStatus();

    Bms_Can_SendFaultStatus1();

    Bms_Can_SendFaultStatus2();

    Bms_Can_SendLastFaultStatus1();

    Bms_Can_SendLastFaultStatus2();

    Bms_Can1_SendTest();

    Bms_Can_SendCellSummary();

    Bms_Can_SendPackCurrent();

    Bms_Can_SendPackPower();

    Bms_Can_SendSocStatus();
}

static void Bms_MainFunction_1000ms(void)
{
    Bms_Soc_1sFunction();
}


/* ================================================================================================
 * Scheduler task table
 *
 * Periods are expressed in base ticks (1 tick = 10 ms, driven by PIT0 Channel 0).
 * ============================================================================================== */

static const Bms_Scheduler_TaskEntryType Bms_Scheduler_TaskTable[] =
{
    { Bms_MainFunction_10ms,    1U },   /* 10 ms   */
    { Bms_MainFunction_100ms,  10U },   /* 100 ms  */
    { Bms_MainFunction_1000ms, 100U }   /* 1000 ms */
};

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
     * 8. Initialize the BMS ADC
     *
     * Must be initialized before the 10 ms task runs Bms_Adc_MainFunction().
     *
     * During bring-up, expose ADC init/calibration failures instead of silently
     * continuing to schedule conversions.
     * ========================================================================================== */

    if (Bms_Adc_Init() != (Std_ReturnType)E_OK)
    {
        /* ADC init or calibration failed: turn RED LED permanently ON and trap. */
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

        /*
    * Initialize 3-channel NTC temperature monitor.
    *
    * NTC1 / NTC2 / NTC3 will use ADC results from Bms_Adc.
    */
    Bms_Ntc_Init();


    /* Initialize CAN (FlexCAN) for BMS communication. */
    if (Bms_Can_Init() != (Std_ReturnType)E_OK)
    {
        /* CAN init failed: turn RED LED permanently ON and trap. */
        Siul2_Dio_Ip_WritePin(
            LED_RED_PORT,
            LED_RED_PIN,
            0U
        );

        while (1)
        {
            /* CAN init failed */
        }
    }

    /* Initialize SPI for BMS communication. */
    if (Bms_Spi_Init() != (Std_ReturnType)E_OK)
    {
        Siul2_Dio_Ip_WritePin(
            LED_RED_PORT,
            LED_RED_PIN,
            0U
        );

        while (1)
        {
            /* SPI init failed */
        }
    }

    /* Initialize application fault manager. */
    FaultManager_Init();

/* Initialize contactor / precharge state machine. */
    Bms_Contactor_Init();

    /* Initialize BMS state machine. */
    Bms_StateMachine_Init();

    Bms_App_Init();

    Bms_Vafe_Init();

    Bms_Vpack_Init();

    BatteryMonitor_Init();

    /* Initialize SOC persistent storage and scan Data Flash. */
    Bms_Nvm_Init();

    Bms_Soc_Init();


    /* ============================================================================================
     * 9. Initialize the BMS scheduler
     *
     * Must be registered before the PIT channel starts ticking.
     * ========================================================================================== */

    Bms_Scheduler_Init(
        Bms_Scheduler_TaskTable,
        (uint32)(sizeof(Bms_Scheduler_TaskTable) / sizeof(Bms_Scheduler_TaskTable[0]))
    );


    /* ============================================================================================
     * 10. Start PIT Channel 0
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
     * 11. Main loop
     * ========================================================================================== */

    while (1)
    {
        Bms_Scheduler_MainFunction();
    }
}
