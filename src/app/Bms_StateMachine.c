#include "Bms_StateMachine.h"
#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

#define BMS_STATE_LED_PORT    PTA_H_HALF
#define BMS_STATE_LED_PIN     (15U)

/*
 * Temporary external CAN requests.
 * Later we can replace these with getter APIs.
 */
extern volatile boolean g_BmsEnableRequest;
extern volatile boolean g_BmsDisableRequest;
extern volatile boolean g_BmsClearFaultRequest;

static volatile Bms_StateType g_BmsState = BMS_STATE_INIT;

void Bms_StateMachine_Init(void)
{
    g_BmsState = BMS_STATE_INIT;
}

Bms_StateType Bms_StateMachine_GetState(void)
{
    return g_BmsState;
}

static void Bms_StateMachine_UpdateLed(void)
{
    switch (g_BmsState)
    {
        case BMS_STATE_ACTIVE:
        {
            /* LED3 ON - active low */
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
            /* LED3 OFF */
            Siul2_Dio_Ip_WritePin(
                BMS_STATE_LED_PORT,
                BMS_STATE_LED_PIN,
                1U
            );
            break;
        }
    }
}

void Bms_StateMachine_MainFunction(void)
{
    switch (g_BmsState)
    {
        case BMS_STATE_INIT:
        {
            /*
             * First version:
             * initialization is already completed before scheduler starts,
             * so transition directly to STANDBY.
             */
            g_BmsState = BMS_STATE_STANDBY;
            break;
        }

        case BMS_STATE_STANDBY:
        {
            if (g_BmsEnableRequest == TRUE)
            {
                g_BmsEnableRequest = FALSE;
                g_BmsState = BMS_STATE_ACTIVE;
            }

            break;
        }

        case BMS_STATE_ACTIVE:
        {
            if (g_BmsDisableRequest == TRUE)
            {
                g_BmsDisableRequest = FALSE;
                g_BmsState = BMS_STATE_STANDBY;
            }

            break;
        }

        case BMS_STATE_FAULT:
        {
            if (g_BmsClearFaultRequest == TRUE)
            {
                g_BmsClearFaultRequest = FALSE;

                /*
                 * Temporary:
                 * assume fault condition has disappeared.
                 */
                g_BmsState = BMS_STATE_STANDBY;
            }

            break;
        }

        default:
        {
            g_BmsState = BMS_STATE_FAULT;
            break;
        }
    }

    Bms_StateMachine_UpdateLed();
}
