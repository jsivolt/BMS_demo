#include "Bms_Led.h"
#include "Siul2_Dio_Ip.h"

#define LED_ON_LEVEL   0U
#define LED_OFF_LEVEL  1U

static void Bms_Led_Write(Bms_LedType led, uint8 level)
{
    switch (led)
    {
        case BMS_LED_RED:
            Siul2_Dio_Ip_WritePin(PTA_H_HALF, 13U, level);
            break;

        case BMS_LED_GREEN:
            Siul2_Dio_Ip_WritePin(PTA_H_HALF, 14U, level);
            break;

        case BMS_LED_BLUE:
            Siul2_Dio_Ip_WritePin(PTA_H_HALF, 15U, level);
            break;

        case BMS_LED_YELLOW:
            Siul2_Dio_Ip_WritePin(PTB_H_HALF, 2U, level);
            break;

        default:
            break;
    }
}

void Bms_Led_Init(void)
{
    Bms_Led_AllOff();
}

void Bms_Led_On(Bms_LedType led)
{
    Bms_Led_Write(led, LED_ON_LEVEL);
}

void Bms_Led_Off(Bms_LedType led)
{
    Bms_Led_Write(led, LED_OFF_LEVEL);
}

void Bms_Led_AllOff(void)
{
    Bms_Led_Off(BMS_LED_RED);
    Bms_Led_Off(BMS_LED_GREEN);
    Bms_Led_Off(BMS_LED_BLUE);
    Bms_Led_Off(BMS_LED_YELLOW);
}
