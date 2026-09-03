#ifndef BMS_LED_H
#define BMS_LED_H

typedef enum
{
    BMS_LED_RED = 0,
    BMS_LED_GREEN,
    BMS_LED_BLUE,
    BMS_LED_YELLOW
} Bms_LedType;

void Bms_Led_Init(void);
void Bms_Led_On(Bms_LedType led);
void Bms_Led_Off(Bms_LedType led);
void Bms_Led_AllOff(void);

#endif /* BMS_LED_H */
