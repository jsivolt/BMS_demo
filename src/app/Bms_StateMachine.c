#include "Bms_StateMachine.h"

#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

#include "Fault_Manager.h"


#define BMS_STATE_LED_PORT    PTA_H_HALF
#define BMS_STATE_LED_PIN     (15U)


/*
 * Temporary external CAN requests.
 *
 * Later these can be replaced with getter APIs.
 */
extern volatile boolean g_BmsEnableRequest;
extern volatile boolean g_BmsDisableRequest;
extern volatile boolean g_BmsClearFaultRequest;


/*
 * Current BMS system state.
 */
static volatile Bms_StateType g_BmsState = BMS_STATE_INIT;


/* ================================================================================================
 * Local function prototypes
 * ============================================================================================== */

static void Bms_StateMachine_UpdateLed(void);


/* ================================================================================================
 * Initialization
 * ============================================================================================== */

void Bms_StateMachine_Init(void)
{
    g_BmsState = BMS_STATE_INIT;
}


/* ================================================================================================
 * Getter
 * ============================================================================================== */

Bms_StateType Bms_StateMachine_GetState(void)
{
    return g_BmsState;
}


/* ================================================================================================
 * State LED
 * ============================================================================================== */

static void Bms_StateMachine_UpdateLed(void)
{
    switch (g_BmsState)
    {
        case BMS_STATE_ACTIVE:
        {
            /*
             * LED3 ON
             *
             * Board LED is active low.
             */
            Siul2_Dio_Ip_WritePin(
                BMS_STATE_LED_PORT,
                BMS_STATE_LED_PIN,
                0U
            );

            break;
        }


        case BMS_STATE_INIT:
        case BMS_STATE_STANDBY:
        case BMS_STATE_FAULT:
        default:
        {
            /*
             * LED3 OFF
             */
            Siul2_Dio_Ip_WritePin(
                BMS_STATE_LED_PORT,
                BMS_STATE_LED_PIN,
                1U
            );

            break;
        }
    }
}


/* ================================================================================================
 * Main state machine
 * ============================================================================================== */

void Bms_StateMachine_MainFunction(void)
{
    switch (g_BmsState)
    {
        /*
         * ==========================================================================================
         * INIT
         * ==========================================================================================
         */
        case BMS_STATE_INIT:
        {
            /*
             * Initialization is completed before the scheduler starts.
             *
             * However, before entering STANDBY, check whether a
             * critical fault already exists.
             */
            if (FaultManager_HasCriticalFault() == TRUE)
            {
                g_BmsState = BMS_STATE_FAULT;
            }
            else
            {
                g_BmsState = BMS_STATE_STANDBY;
            }

            break;
        }


        /*
         * ==========================================================================================
         * STANDBY
         * ==========================================================================================
         */
        case BMS_STATE_STANDBY:
        {
            /*
             * Critical fault always has the highest priority.
             */
            if (FaultManager_HasCriticalFault() == TRUE)
            {
                /*
                 * Do not allow an old enable request to remain pending.
                 */
                g_BmsEnableRequest = FALSE;

                g_BmsState = BMS_STATE_FAULT;
            }
            else if (g_BmsEnableRequest == TRUE)
            {
                /*
                 * Enable request accepted.
                 */
                g_BmsEnableRequest = FALSE;

                g_BmsState = BMS_STATE_ACTIVE;
            }
            else
            {
                /*
                 * Stay in STANDBY.
                 */
            }

            break;
        }


        /*
         * ==========================================================================================
         * ACTIVE
         * ==========================================================================================
         */
        case BMS_STATE_ACTIVE:
        {
            /*
             * Critical fault has the highest priority.
             *
             * Examples:
             * - Pack OV / UV
             * - Cell OV / UV
             * - Over-temperature
             * - NTC sensor fault
             * - AFE communication fault
             * - Contactor fault
             * - Over-current
             */
            if (FaultManager_HasCriticalFault() == TRUE)
            {
                /*
                 * Clear pending commands when entering FAULT.
                 */
                g_BmsEnableRequest = FALSE;
                g_BmsDisableRequest = FALSE;

                g_BmsState = BMS_STATE_FAULT;
            }
            else if (g_BmsDisableRequest == TRUE)
            {
                /*
                 * Normal operator/system disable request.
                 */
                g_BmsDisableRequest = FALSE;

                g_BmsState = BMS_STATE_STANDBY;
            }
            else
            {
                /*
                 * Stay ACTIVE.
                 */
            }

            break;
        }


        /*
         * ==========================================================================================
         * FAULT
         * ==========================================================================================
         */
        case BMS_STATE_FAULT:
        {
            /*
             * Do not leave FAULT automatically.
             *
             * Two conditions are required:
             *
             * 1. The underlying critical fault condition is gone.
             * 2. A clear-fault request is received.
             */
            if (g_BmsClearFaultRequest == TRUE)
            {
                /*
                 * Consume the request once.
                 */
                g_BmsClearFaultRequest = FALSE;

                if (FaultManager_HasCriticalFault() == FALSE)
                {
                    /*
                     * Fault condition has disappeared.
                     *
                     * Return to STANDBY, not directly to ACTIVE.
                     */
                    g_BmsState = BMS_STATE_STANDBY;
                }
                else
                {
                    /*
                     * Critical fault still exists.
                     *
                     * Remain in FAULT.
                     */
                }
            }

            break;
        }


        /*
         * ==========================================================================================
         * Invalid state
         * ==========================================================================================
         */
        default:
        {
            /*
             * Fail-safe behavior:
             * any unexpected state goes to FAULT.
             */
            g_BmsState = BMS_STATE_FAULT;

            break;
        }
    }


    /*
     * Update state indication after processing transitions.
     */
    Bms_StateMachine_UpdateLed();
}