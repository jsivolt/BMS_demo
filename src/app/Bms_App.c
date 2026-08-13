#include <stdint.h>

#include "Bms_App.h"
#include "Bms_Led.h"

static Bms_StateType g_BmsState = BMS_STATE_INIT;
static uint32_t g_MainCounter = 0U;

void Bms_Init(void)
{
    Bms_Led_Init();
    g_BmsState = BMS_STATE_INIT;
    g_MainCounter = 0U;
}

void Bms_MainFunction(void)
{
    g_MainCounter++;

    /* 测试状态切换 */
    if (g_MainCounter < 100U)
    {
        g_BmsState = BMS_STATE_INIT;
    }
    else if (g_MainCounter < 300U)
    {
        g_BmsState = BMS_STATE_NORMAL;
    }
    else if (g_MainCounter < 500U)
    {
        g_BmsState = BMS_STATE_WARNING;
    }
    else if (g_MainCounter < 700U)
    {
        g_BmsState = BMS_STATE_FAULT;
    }
    else
    {
        g_MainCounter = 0U;
    }

    Bms_Led_AllOff();

    switch (g_BmsState)
    {
        case BMS_STATE_INIT:
            Bms_Led_On(BMS_LED_BLUE);
            break;

        case BMS_STATE_NORMAL:
            Bms_Led_On(BMS_LED_GREEN);
            break;

        case BMS_STATE_WARNING:
            Bms_Led_On(BMS_LED_YELLOW);
            break;

        case BMS_STATE_FAULT:
        default:
            Bms_Led_On(BMS_LED_RED);
            break;
    }
}
