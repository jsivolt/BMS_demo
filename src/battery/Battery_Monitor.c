#include "Battery_Monitor.h"

#include "Bms_Adc.h"
#include "Bms_Ntc.h"
#include "Fault_Manager.h"

#define BMS_TEMP_HIGH_FAULT_SET_DC        (600)    /* 60.0°C */
#define BMS_TEMP_HIGH_FAULT_CLEAR_DC      (550)    /* 55.0°C */

#define BMS_TEMP_LOW_FAULT_SET_DC         (-200)   /* -20.0°C */
#define BMS_TEMP_LOW_FAULT_CLEAR_DC       (-150)   /* -15.0°C */

#define BMS_TEMP_DELTA_FAULT_SET_DC       (150)    /* 15.0°C */
#define BMS_TEMP_DELTA_FAULT_CLEAR_DC     (100)    /* 10.0°C */

static BatteryMonitor_DataType g_BatteryData;



/* ================================================================================================
 * Local function prototypes
 * ============================================================================================== */

static void BatteryMonitor_UpdateTemperatureSummary(void);
static void BatteryMonitor_UpdateTemperatureFaults(void);


/* ================================================================================================
 * Initialization
 * ============================================================================================== */

void BatteryMonitor_Init(void)
{
    uint8 i;

    g_BatteryData.PackV1 = 0.0f;
    g_BatteryData.PackV2 = 0.0f;
    g_BatteryData.PackV3 = 0.0f;

    for (i = 0U; i < 16U; i++)
    {
        g_BatteryData.CellVoltage[i] = 0.0f;
    }

    for (i = 0U; i < BATTERY_MONITOR_PACK_COUNT; i++)
    {
        g_BatteryData.PackTemperature_dC[i] = 0;
        g_BatteryData.PackTemperatureValid[i] = FALSE;
    }

    g_BatteryData.MinPackTemperature_dC = 0;
    g_BatteryData.MaxPackTemperature_dC = 0;
    g_BatteryData.DeltaPackTemperature_dC = 0;

    g_BatteryData.TemperatureSummaryValid = FALSE;

    g_BatteryData.Valid = FALSE;

    g_BatteryData.Status = BAT_MON_STATUS_INVALID;
}


/* ================================================================================================
 * Main periodic function
 * ============================================================================================== */

void BatteryMonitor_MainFunction(void)
{
    /*
     * ==========================================================================
     * Pack voltage
     * ==========================================================================
     */

    if (Bms_Adc_IsPackValid() == TRUE)
    {
        g_BatteryData.PackV1 =
            (float)Bms_Adc_GetPackV1VoltageMv() / 1000.0f;

        g_BatteryData.PackV2 =
            (float)Bms_Adc_GetPackV2VoltageMv() / 1000.0f;

        g_BatteryData.PackV3 =
            (float)Bms_Adc_GetPackV3VoltageMv() / 1000.0f;

        g_BatteryData.Valid = TRUE;
        g_BatteryData.Status = BAT_MON_STATUS_OK;
    }
    else
    {
        g_BatteryData.Valid = FALSE;
        g_BatteryData.Status = BAT_MON_STATUS_INVALID;
    }


    /*
     * ==========================================================================
     * Pack temperature
     *
     * NTC1 -> Pack 1
     * NTC2 -> Pack 2
     * NTC3 -> Pack 3
     * ==========================================================================
     */

    g_BatteryData.PackTemperatureValid[0] =
        Bms_Ntc_IsValid(BMS_NTC_1);

    g_BatteryData.PackTemperatureValid[1] =
        Bms_Ntc_IsValid(BMS_NTC_2);

    g_BatteryData.PackTemperatureValid[2] =
        Bms_Ntc_IsValid(BMS_NTC_3);


    /*
     * Only overwrite a pack temperature when the newest NTC reading is valid.
     *
     * Bms_Ntc itself also preserves the last valid temperature.
     */
    if (g_BatteryData.PackTemperatureValid[0] == TRUE)
    {
        g_BatteryData.PackTemperature_dC[0] =
            Bms_Ntc_GetTemperature_dC(BMS_NTC_1);
    }

    if (g_BatteryData.PackTemperatureValid[1] == TRUE)
    {
        g_BatteryData.PackTemperature_dC[1] =
            Bms_Ntc_GetTemperature_dC(BMS_NTC_2);
    }

    if (g_BatteryData.PackTemperatureValid[2] == TRUE)
    {
        g_BatteryData.PackTemperature_dC[2] =
            Bms_Ntc_GetTemperature_dC(BMS_NTC_3);
    }


    /*
     * Calculate Min / Max / Delta using VALID packs only.
     */
    BatteryMonitor_UpdateTemperatureSummary();

    /*
    * Update temperature-related pack/system faults.
    */
    BatteryMonitor_UpdateTemperatureFaults();
}


/* ================================================================================================
 * Temperature summary
 * ============================================================================================== */

static void BatteryMonitor_UpdateTemperatureSummary(void)
{
    uint8 i;
    boolean firstValidFound;
    sint16 temperature;

    firstValidFound = FALSE;

    g_BatteryData.MinPackTemperature_dC = 0;
    g_BatteryData.MaxPackTemperature_dC = 0;
    g_BatteryData.DeltaPackTemperature_dC = 0;

    for (i = 0U; i < BATTERY_MONITOR_PACK_COUNT; i++)
    {
        if (g_BatteryData.PackTemperatureValid[i] == TRUE)
        {
            temperature =
                g_BatteryData.PackTemperature_dC[i];

            if (firstValidFound == FALSE)
            {
                /*
                 * First valid pack becomes the initial min/max.
                 */
                g_BatteryData.MinPackTemperature_dC =
                    temperature;

                g_BatteryData.MaxPackTemperature_dC =
                    temperature;

                firstValidFound = TRUE;
            }
            else
            {
                if (temperature <
                    g_BatteryData.MinPackTemperature_dC)
                {
                    g_BatteryData.MinPackTemperature_dC =
                        temperature;
                }

                if (temperature >
                    g_BatteryData.MaxPackTemperature_dC)
                {
                    g_BatteryData.MaxPackTemperature_dC =
                        temperature;
                }
            }
        }
    }


    if (firstValidFound == TRUE)
    {
        g_BatteryData.DeltaPackTemperature_dC =
            g_BatteryData.MaxPackTemperature_dC -
            g_BatteryData.MinPackTemperature_dC;

        g_BatteryData.TemperatureSummaryValid = TRUE;
    }
    else
    {
        /*
         * No valid temperature sensors.
         */
        g_BatteryData.TemperatureSummaryValid = FALSE;
    }
}

/* ================================================================================================
 * Temperature fault monitoring
 * ============================================================================================== */

static void BatteryMonitor_UpdateTemperatureFaults(void)
{
    uint8 i;
    FaultPackIdType packId;
    sint16 temperature_dC;

    /*
     * Check each battery pack independently.
     */
    for (i = 0U; i < BATTERY_MONITOR_PACK_COUNT; i++)
    {
        packId = (FaultPackIdType)i;

        /*
         * ============================================================
         * NTC sensor validity
         * ============================================================
         */
        if (g_BatteryData.PackTemperatureValid[i] == FALSE)
        {
            /*
             * Sensor data is invalid.
             */
            FaultManager_SetPack(
                packId,
                FAULT_TEMP_SENSOR);

            /*
             * Temperature value cannot be trusted.
             *
             * Clear threshold-based temperature faults.
             */
            FaultManager_ClearPack(
                packId,
                FAULT_OVER_TEMP);

            FaultManager_ClearPack(
                packId,
                FAULT_UNDER_TEMP);
        }
        else
        {
            /*
             * Sensor is valid.
             */
            FaultManager_ClearPack(
                packId,
                FAULT_TEMP_SENSOR);

            temperature_dC =
                g_BatteryData.PackTemperature_dC[i];


            /*
             * ========================================================
             * Over-temperature hysteresis
             *
             * SET   >= 60.0°C
             * CLEAR <= 55.0°C
             * ========================================================
             */
            if (FaultManager_IsPackFaultActive(
                    packId,
                    FAULT_OVER_TEMP) == TRUE)
            {
                /*
                 * Fault is already active.
                 *
                 * Only clear after temperature drops
                 * below the clear threshold.
                 */
                if (temperature_dC <=
                    BMS_TEMP_HIGH_FAULT_CLEAR_DC)
                {
                    FaultManager_ClearPack(
                        packId,
                        FAULT_OVER_TEMP);
                }
            }
            else
            {
                /*
                 * Fault is currently inactive.
                 *
                 * Set only after temperature reaches
                 * the set threshold.
                 */
                if (temperature_dC >=
                    BMS_TEMP_HIGH_FAULT_SET_DC)
                {
                    FaultManager_SetPack(
                        packId,
                        FAULT_OVER_TEMP);
                }
            }


            /*
             * ========================================================
             * Under-temperature hysteresis
             *
             * SET   <= -20.0°C
             * CLEAR >= -15.0°C
             * ========================================================
             */
            if (FaultManager_IsPackFaultActive(
                    packId,
                    FAULT_UNDER_TEMP) == TRUE)
            {
                /*
                 * Fault is already active.
                 */
                if (temperature_dC >=
                    BMS_TEMP_LOW_FAULT_CLEAR_DC)
                {
                    FaultManager_ClearPack(
                        packId,
                        FAULT_UNDER_TEMP);
                }
            }
            else
            {
                /*
                 * Fault is currently inactive.
                 */
                if (temperature_dC <=
                    BMS_TEMP_LOW_FAULT_SET_DC)
                {
                    FaultManager_SetPack(
                        packId,
                        FAULT_UNDER_TEMP);
                }
            }
        }
    }


    /*
     * ================================================================
     * Pack-to-pack temperature delta hysteresis
     *
     * SET   >= 15.0°C
     * CLEAR <= 10.0°C
     * ================================================================
     *
     * Only evaluate the delta fault when all three NTC sensors
     * are currently valid.
     */
    if ((g_BatteryData.PackTemperatureValid[0] == TRUE) &&
        (g_BatteryData.PackTemperatureValid[1] == TRUE) &&
        (g_BatteryData.PackTemperatureValid[2] == TRUE))
    {
        if (FaultManager_IsSystemFaultActive(
                FAULT_TEMP_DELTA) == TRUE)
        {
            /*
             * Delta fault is already active.
             */
            if (g_BatteryData.DeltaPackTemperature_dC <=
                BMS_TEMP_DELTA_FAULT_CLEAR_DC)
            {
                FaultManager_ClearSystem(
                    FAULT_TEMP_DELTA);
            }
        }
        else
        {
            /*
             * Delta fault is currently inactive.
             */
            if (g_BatteryData.DeltaPackTemperature_dC >=
                BMS_TEMP_DELTA_FAULT_SET_DC)
            {
                FaultManager_SetSystem(
                    FAULT_TEMP_DELTA);
            }
        }
    }
    else
    {
        /*
         * Temperature delta cannot be evaluated reliably
         * unless all three pack temperatures are valid.
         */
        FaultManager_ClearSystem(
            FAULT_TEMP_DELTA);
    }
}


/* ================================================================================================
 * Getter
 * ============================================================================================== */

const BatteryMonitor_DataType *BatteryMonitor_GetData(void)
{
    return &g_BatteryData;
}