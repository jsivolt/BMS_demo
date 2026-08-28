#include "Battery_Monitor.h"

#include "Bms_Adc.h"
#include "Bms_Ntc.h"
#include "../communication/Bms_Can.h"
#include "vAFE/Bms_Vafe.h"
#include "vPACK/Bms_Vpack.h"
#include "Fault_Manager.h"

/* Simulation-stage cell voltage fault thresholds, tune per cell chemistry later */
#define BMS_CELL_OV_FAULT_SET_MV       (4250U)
#define BMS_CELL_OV_FAULT_CLEAR_MV     (4150U)

#define BMS_CELL_UV_FAULT_SET_MV       (2500U)
#define BMS_CELL_UV_FAULT_CLEAR_MV     (2700U)

#define BMS_TEMP_HIGH_FAULT_SET_DC        (600)    /* 60.0°C */
#define BMS_TEMP_HIGH_FAULT_CLEAR_DC      (550)    /* 55.0°C */

#define BMS_TEMP_LOW_FAULT_SET_DC         (-200)   /* -20.0°C */
#define BMS_TEMP_LOW_FAULT_CLEAR_DC       (-150)   /* -15.0°C */

#define BMS_TEMP_DELTA_FAULT_SET_DC       (150)    /* 15.0°C */
#define BMS_TEMP_DELTA_FAULT_CLEAR_DC     (100)    /* 10.0°C */

#define BMS_CELL_IMBALANCE_SET_MV      (300U)
#define BMS_CELL_IMBALANCE_CLEAR_MV    (200U)

static BatteryMonitor_DataType g_BatteryData;



/* ================================================================================================
 * Local function prototypes
 * ============================================================================================== */

static void BatteryMonitor_UpdateCellVoltages(void);
static void BatteryMonitor_UpdatePackMonitor(void);
static void BatteryMonitor_UpdateVpackFaults(void);
static void BatteryMonitor_UpdateCellVoltageFaults(void);
static void BatteryMonitor_UpdateCellImbalanceFault(void);
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

    g_BatteryData.PackCurrent_mA = 0;
    g_BatteryData.ShuntVoltage_uV = 0;

    g_BatteryData.VpackPackVoltage_mV = 0U;
    g_BatteryData.VpackBusVoltage_mV = 0U;

    g_BatteryData.VpackAliveCounter = 0U;
    g_BatteryData.VpackStatus = 0U;

    g_BatteryData.VpackCurrentValid = FALSE;
    g_BatteryData.VpackVoltageValid = FALSE;
    g_BatteryData.VpackAliveValid = FALSE;
    g_BatteryData.VpackValid = FALSE;

    for (i = 0U; i < 16U; i++)
    {
        g_BatteryData.CellVoltage[i] = 0.0f;
    }

    g_BatteryData.MinCellVoltage = 0.0f;
    g_BatteryData.MaxCellVoltage = 0.0f;
    g_BatteryData.DeltaCellVoltage = 0.0f;

    g_BatteryData.MinCellIndex = 0U;
    g_BatteryData.MaxCellIndex = 0U;

    g_BatteryData.CellVoltageValid = FALSE;

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

    if ((Bms_Adc_IsPackValid() == TRUE) &&
        (g_CanPack1VoltageValid == TRUE))
    {
        /*
         * Pack 1 voltage from CAN2 simulation.
         */
        g_BatteryData.PackV1 =
            g_CanPack1Voltage_V;

        /*
         * Pack 2 / Pack 3 remain ADC inputs.
         */
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
     * Pack monitor data from virtual ADBMS2950
     * ==========================================================================
     */

    BatteryMonitor_UpdatePackMonitor();

    BatteryMonitor_UpdateVpackFaults();


    /*
     * ==========================================================================
     * Cell voltages from vAFE
     * ==========================================================================
     */

    BatteryMonitor_UpdateCellVoltages();

    BatteryMonitor_UpdateCellVoltageFaults();

    BatteryMonitor_UpdateCellImbalanceFault();


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
 * Pack monitor update (virtual ADBMS2950)
 * ============================================================================================== */

static void BatteryMonitor_UpdatePackMonitor(void)
{
    /*
     * Always copy communication health flags.
     */
    g_BatteryData.VpackCurrentValid =
        g_BmsVpackData.CurrentValid;

    g_BatteryData.VpackVoltageValid =
        g_BmsVpackData.VoltageValid;

    g_BatteryData.VpackAliveValid =
        g_BmsVpackData.AliveValid;

    g_BatteryData.VpackValid =
        g_BmsVpackData.Valid;

    /*
     * Only overwrite measurement data when the overall
     * vPACK dataset is valid.
     */
    if (g_BmsVpackData.Valid == TRUE)
    {
        g_BatteryData.PackCurrent_mA =
            g_BmsVpackData.PackCurrent_mA;

        g_BatteryData.ShuntVoltage_uV =
            g_BmsVpackData.ShuntVoltage_uV;

        g_BatteryData.VpackPackVoltage_mV =
            g_BmsVpackData.PackVoltage_mV;

        g_BatteryData.VpackBusVoltage_mV =
            g_BmsVpackData.BusVoltage_mV;

        g_BatteryData.VpackAliveCounter =
            g_BmsVpackData.AliveCounter;

        g_BatteryData.VpackStatus =
            g_BmsVpackData.Status;
    }
}


/* ================================================================================================
 * Vpack (ADBMS2950) fault evaluation
 * ============================================================================================== */

static void BatteryMonitor_UpdateVpackFaults(void)
{
    /*
     * ADBMS2950 communication timeout:
     * either required frame is missing.
     */
    if ((g_BatteryData.VpackCurrentValid == FALSE) ||
        (g_BatteryData.VpackVoltageValid == FALSE))
    {
        FaultManager_SetSystem(
            FAULT_VPACK_COMM_TIMEOUT);
    }
    else
    {
        FaultManager_ClearSystem(
            FAULT_VPACK_COMM_TIMEOUT);
    }

    /*
     * Alive counter error.
     */
    if (g_BatteryData.VpackAliveValid == FALSE)
    {
        FaultManager_SetSystem(
            FAULT_VPACK_ALIVE_ERROR);
    }
    else
    {
        FaultManager_ClearSystem(
            FAULT_VPACK_ALIVE_ERROR);
    }

    /*
     * ADBMS2950-reported device status.
     */
    if (g_BatteryData.VpackStatus != 0U)
    {
        FaultManager_SetSystem(
            FAULT_VPACK_DEVICE_FAULT);
    }
    else
    {
        FaultManager_ClearSystem(
            FAULT_VPACK_DEVICE_FAULT);
    }
}


/* ================================================================================================
 * Cell voltage update (vAFE)
 * ============================================================================================== */

static void BatteryMonitor_UpdateCellVoltages(void)
{
    uint8 i;

    if (g_BmsVafeData.DataValid == TRUE)
    {
        for (i = 0U; i < 16U; i++)
        {
            g_BatteryData.CellVoltage[i] =
                (float)g_BmsVafeData.CellVoltage_mV[i] / 1000.0f;
        }

        g_BatteryData.MinCellVoltage =
            (float)g_BmsVafeData.MinCellVoltage_mV / 1000.0f;

        g_BatteryData.MaxCellVoltage =
            (float)g_BmsVafeData.MaxCellVoltage_mV / 1000.0f;

        g_BatteryData.DeltaCellVoltage =
            (float)g_BmsVafeData.DeltaCellVoltage_mV / 1000.0f;

        g_BatteryData.MinCellIndex =
            g_BmsVafeData.MinCellIndex;

        g_BatteryData.MaxCellIndex =
            g_BmsVafeData.MaxCellIndex;

        g_BatteryData.CellVoltageValid = TRUE;
    }
    else
    {
        g_BatteryData.CellVoltageValid = FALSE;
    }
}


/* ================================================================================================
 * Cell voltage fault monitoring
 * ============================================================================================== */

static void BatteryMonitor_UpdateCellVoltageFaults(void)
{
    if (g_BatteryData.CellVoltageValid == FALSE)
    {
        /*
         * vAFE data is not trustworthy, raise AFE comm fault.
         *
         * Clear threshold-based cell voltage faults.
         */
        FaultManager_SetSystem(FAULT_AFE_COMM);

        FaultManager_ClearSystem(FAULT_CELL_OV);
        FaultManager_ClearSystem(FAULT_CELL_UV);

        return;
    }

    FaultManager_ClearSystem(FAULT_AFE_COMM);

    /*
     * ================================================================
     * Cell over-voltage hysteresis
     *
     * SET   >= 4.250 V
     * CLEAR <= 4.150 V
     * ================================================================
     */
    if (FaultManager_IsSystemFaultActive(FAULT_CELL_OV) == TRUE)
    {
        if (g_BmsVafeData.MaxCellVoltage_mV <=
            BMS_CELL_OV_FAULT_CLEAR_MV)
        {
            FaultManager_ClearSystem(FAULT_CELL_OV);
        }
    }
    else
    {
        if (g_BmsVafeData.MaxCellVoltage_mV >=
            BMS_CELL_OV_FAULT_SET_MV)
        {
            FaultManager_SetSystem(FAULT_CELL_OV);
        }
    }

    /*
     * ================================================================
     * Cell under-voltage hysteresis
     *
     * SET   <= 2.500 V
     * CLEAR >= 2.700 V
     * ================================================================
     */
    if (FaultManager_IsSystemFaultActive(FAULT_CELL_UV) == TRUE)
    {
        if (g_BmsVafeData.MinCellVoltage_mV >=
            BMS_CELL_UV_FAULT_CLEAR_MV)
        {
            FaultManager_ClearSystem(FAULT_CELL_UV);
        }
    }
    else
    {
        if (g_BmsVafeData.MinCellVoltage_mV <=
            BMS_CELL_UV_FAULT_SET_MV)
        {
            FaultManager_SetSystem(FAULT_CELL_UV);
        }
    }
}


static void BatteryMonitor_UpdateCellImbalanceFault(void)
{
    if (g_BatteryData.CellVoltageValid == FALSE)
    {
        FaultManager_ClearSystem(FAULT_CELL_IMBALANCE);
        return;
    }

    if (FaultManager_IsSystemFaultActive(
            FAULT_CELL_IMBALANCE) == TRUE)
    {
        if (g_BmsVafeData.DeltaCellVoltage_mV <=
            BMS_CELL_IMBALANCE_CLEAR_MV)
        {
            FaultManager_ClearSystem(
                FAULT_CELL_IMBALANCE);
        }
    }
    else
    {
        if (g_BmsVafeData.DeltaCellVoltage_mV >=
            BMS_CELL_IMBALANCE_SET_MV)
        {
            FaultManager_SetSystem(
                FAULT_CELL_IMBALANCE);
        }
    }
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