#include "Bms_App.h"
#include "Battery_Monitor.h"


void Bms_App_Init(void)
{
    BatteryMonitor_Init();
}



void Bms_App_MainFunction(void)
{
    BatteryMonitor_MainFunction();
}