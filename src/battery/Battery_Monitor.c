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

#define BMS_TEMP_HIGH_FAULT_SET_DC        (2000)    /* 200.0°C */
#define BMS_TEMP_HIGH_FAULT_CLEAR_DC      (1950)    /* 195.0°C */

#define BMS_TEMP_LOW_FAULT_SET_DC         (-200)   /* -20.0°C */
#define BMS_TEMP_LOW_FAULT_CLEAR_DC       (-150)   /* -15.0°C */

#define BMS_TEMP_DELTA_FAULT_SET_DC       (500)    /* 50.0°C (dC = 0.1 °C) */
#define BMS_TEMP_DELTA_FAULT_CLEAR_DC     (100)    /* 10.0°C */

#define BMS_CELL_IMBALANCE_SET_MV      (300U)
#define BMS_CELL_IMBALANCE_CLEAR_MV    (200U)

/*
 * Pack current sign convention:
 *
 * Positive current = Charge
 * Negative current = Discharge
 */

#define BMS_PACK_CHARGE_OC_SET_MA          (80000)
#define BMS_PACK_CHARGE_OC_CLEAR_MA        (70000)

#define BMS_PACK_DISCHARGE_OC_SET_MA      (-100000)
#define BMS_PACK_DISCHARGE_OC_CLEAR_MA     (-90000)

static BatteryMonitor_DataType g_BatteryData;



/* ================================================================================================
 * Local function prototypes
 * ============================================================================================== */

static void BatteryMonitor_UpdateCellVoltages(void);
static void BatteryMonitor_UpdatePackMonitor(void);
static void BatteryMonitor_UpdateVpackFaults(void);
static void BatteryMonitor_UpdatePackPower(void);
static void BatteryMonitor_UpdatePackCurrentFaults(void);
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
        g_BatteryData.PackCurrent_mA[i] = 0;
        g_BatteryData.PackCurrentValid[i] = FALSE;

        g_BatteryData.PackPower_W[i] = 0.0f;
        g_BatteryData.PackPowerValid[i] = FALSE;

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
        (g_BmsVpackData.VoltageValid == TRUE))
    {
        /*
         * Pack 1 voltage from vPACK (ADBMS2950, CAN2 0x411).
         */
        g_BatteryData.PackV1 =
            (float)g_BmsVpackData.PackVoltage_mV / 1000.0f;

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

    if (g_BmsVpackData.VoltageValid == FALSE)
    {
        FaultManager_SetPack(
            FAULT_PACK_1,
            FAULT_PACK1_VOLTAGE_TIMEOUT);
    }
    else
    {
        FaultManager_ClearPack(
            FAULT_PACK_1,
            FAULT_PACK1_VOLTAGE_TIMEOUT);
    }


    /*
     * ==========================================================================
     * Pack monitor data from virtual ADBMS2950
     * ==========================================================================
     */

    BatteryMonitor_UpdatePackMonitor();

    BatteryMonitor_UpdateVpackFaults();

    BatteryMonitor_UpdatePackPower();

    BatteryMonitor_UpdatePackCurrentFaults();


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
     * Communication health from virtual ADBMS2950.
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
     * Current channel currently represents Pack 1.
     */
    g_BatteryData.PackCurrentValid[0] =
        g_BmsVpackData.CurrentValid;

    /*
     * Only overwrite measurement data when the overall
     * vPACK dataset is valid.
     */
    if (g_BmsVpackData.Valid == TRUE)
    {
        g_BatteryData.PackCurrent_mA[0] =
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
 * Pack power calculation
 * ============================================================================================== */

static void BatteryMonitor_UpdatePackPower(void)
{
    /*
     * Pack 1 power is valid only when both
     * voltage and current are valid.
     */
    if ((g_BmsVpackData.VoltageValid == TRUE) &&
        (g_BatteryData.PackCurrentValid[0] == TRUE))
    {
        g_BatteryData.PackPower_W[0] =
            g_BatteryData.PackV1 *
            ((float)g_BatteryData.PackCurrent_mA[0] / 1000.0f);

        g_BatteryData.PackPowerValid[0] = TRUE;
    }
    else
    {
        g_BatteryData.PackPower_W[0] = 0.0f;
        g_BatteryData.PackPowerValid[0] = FALSE;
    }

    /*
     * Pack 2 / Pack 3 current inputs are not implemented yet.
     */
    g_BatteryData.PackPowerValid[1] = FALSE;
    g_BatteryData.PackPowerValid[2] = FALSE;
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
 * Pack 1 current fault evaluation
 * ============================================================================================== */

static void BatteryMonitor_UpdatePackCurrentFaults(void)
{
    sint32 current_mA;

    /*
     * Current threshold faults are only meaningful when
     * the Pack 1 current measurement is valid.
     */
    if (g_BatteryData.PackCurrentValid[0] == FALSE)
    {
        FaultManager_ClearPack(
            FAULT_PACK_1,
            FAULT_PACK_DISCHARGE_OC);

        FaultManager_ClearPack(
            FAULT_PACK_1,
            FAULT_PACK_CHARGE_OC);

        return;
    }

    current_mA =
        g_BatteryData.PackCurrent_mA[0];

    /*
     * ================================================================
     * Discharge over-current
     *
     * SET   <= -100 A
     * CLEAR >=  -90 A
     * ================================================================
     */
    if (FaultManager_IsPackFaultActive(
            FAULT_PACK_1,
            FAULT_PACK_DISCHARGE_OC) == TRUE)
    {
        if (current_mA >=
            BMS_PACK_DISCHARGE_OC_CLEAR_MA)
        {
            FaultManager_ClearPack(
                FAULT_PACK_1,
                FAULT_PACK_DISCHARGE_OC);
        }
    }
    else
    {
        if (current_mA <=
            BMS_PACK_DISCHARGE_OC_SET_MA)
        {
            FaultManager_SetPack(
                FAULT_PACK_1,
                FAULT_PACK_DISCHARGE_OC);
        }
    }

    /*
     * ================================================================
     * Charge over-current
     *
     * SET   >= +80 A
     * CLEAR <= +70 A
     * ================================================================
     */
    if (FaultManager_IsPackFaultActive(
            FAULT_PACK_1,
            FAULT_PACK_CHARGE_OC) == TRUE)
    {
        if (current_mA <=
            BMS_PACK_CHARGE_OC_CLEAR_MA)
        {
            FaultManager_ClearPack(
                FAULT_PACK_1,
                FAULT_PACK_CHARGE_OC);
        }
    }
    else
    {
        if (current_mA >=
            BMS_PACK_CHARGE_OC_SET_MA)
        {
            FaultManager_SetPack(
                FAULT_PACK_1,
                FAULT_PACK_CHARGE_OC);
        }
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