#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H


#include "Std_Types.h"


typedef struct
{
    float PackVoltage;
    float CellVoltage[16];

    float Temperature;

    boolean Valid;

}BatteryMonitor_DataType;



void BatteryMonitor_Init(void);


void BatteryMonitor_MainFunction(void);


const BatteryMonitor_DataType*
BatteryMonitor_GetData(void);



#endif