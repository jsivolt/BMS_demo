#include "Bms_Vpack.h"


volatile Bms_Vpack_DataType g_BmsVpackData =
{
    .PackCurrent_mA = 0,
    .ShuntVoltage_uV = 0,
    .PackVoltage_mV = 0U,
    .BusVoltage_mV = 0U,
    .AliveCounter = 0U,
    .Status = 0U,
    .CurrentValid = FALSE,
    .VoltageValid = FALSE,
    .AliveValid = FALSE,
    .Valid = FALSE
};

volatile uint32 g_BmsVpackFrameCount = 0U;

static uint16 g_BmsVpackCurrentTimeoutTicks = 0U;
static uint16 g_BmsVpackVoltageTimeoutTicks = 0U;

static uint8 g_BmsVpackLastAliveCounter = 0U;
static boolean g_BmsVpackAliveInitialized = FALSE;


/*
 * Decode little-endian signed 32-bit value.
 */
static sint32 Bms_Vpack_ReadS32LE(const uint8 *data)
{
    uint32 raw;

    raw =
        ((uint32)data[0]) |
        ((uint32)data[1] << 8U) |
        ((uint32)data[2] << 16U) |
        ((uint32)data[3] << 24U);

    return (sint32)raw;
}


/*
 * Decode little-endian signed 16-bit value.
 */
static sint16 Bms_Vpack_ReadS16LE(const uint8 *data)
{
    uint16 raw;

    raw =
        ((uint16)data[0]) |
        ((uint16)data[1] << 8U);

    return (sint16)raw;
}


/*
 * Decode little-endian unsigned 32-bit value.
 */
static uint32 Bms_Vpack_ReadU32LE(const uint8 *data)
{
    return
        ((uint32)data[0]) |
        ((uint32)data[1] << 8U) |
        ((uint32)data[2] << 16U) |
        ((uint32)data[3] << 24U);
}


void Bms_Vpack_Init(void)
{
    g_BmsVpackData.PackCurrent_mA = 0;
    g_BmsVpackData.ShuntVoltage_uV = 0;

    g_BmsVpackData.PackVoltage_mV = 0U;
    g_BmsVpackData.BusVoltage_mV  = 0U;

    g_BmsVpackData.AliveCounter = 0U;
    g_BmsVpackData.Status = 0U;

    g_BmsVpackData.CurrentValid = FALSE;
    g_BmsVpackData.VoltageValid = FALSE;
    g_BmsVpackData.AliveValid = FALSE;
    g_BmsVpackData.Valid = FALSE;

    g_BmsVpackFrameCount = 0U;

    g_BmsVpackCurrentTimeoutTicks = 0U;
    g_BmsVpackVoltageTimeoutTicks = 0U;

    g_BmsVpackLastAliveCounter = 0U;
    g_BmsVpackAliveInitialized = FALSE;
}


void Bms_Vpack_ProcessFrame(
    uint32 canId,
    uint8 dlc,
    const uint8 *data
)
{
    if (data == NULL_PTR)
    {
        return;
    }

    if (canId == BMS_VPACK_CAN_ID_CURRENT)
    {
        if (dlc < 8U)
        {
            return;
        }

        /*
         * Byte 0-3:
         * Pack current, signed, unit = 1 mA.
         */
        g_BmsVpackData.PackCurrent_mA =
            Bms_Vpack_ReadS32LE(&data[0]);

        /*
         * Byte 4-5:
         * Shunt voltage, signed, unit = 1 uV.
         */
        g_BmsVpackData.ShuntVoltage_uV =
            Bms_Vpack_ReadS16LE(&data[4]);

        /*
         * Byte 6:
         * Alive counter.
         */
        g_BmsVpackData.AliveCounter =
            data[6];

        /*
         * Byte 7:
         * Pack-monitor status.
         */
        g_BmsVpackData.Status =
            data[7];

        g_BmsVpackCurrentTimeoutTicks = 0U;
        g_BmsVpackData.CurrentValid = TRUE;

        if (g_BmsVpackAliveInitialized == FALSE)
        {
            g_BmsVpackLastAliveCounter =
                g_BmsVpackData.AliveCounter;

            g_BmsVpackAliveInitialized = TRUE;
            g_BmsVpackData.AliveValid = TRUE;
        }
        else
        {
            uint8 expectedAlive;

            expectedAlive =
                (uint8)((g_BmsVpackLastAliveCounter + 1U)
                & BMS_VPACK_ALIVE_MAX_VALUE);

            g_BmsVpackData.AliveValid =
                (boolean)(g_BmsVpackData.AliveCounter == expectedAlive);

            g_BmsVpackLastAliveCounter =
                g_BmsVpackData.AliveCounter;
        }

        g_BmsVpackFrameCount++;
    }
    else if (canId == BMS_VPACK_CAN_ID_VOLTAGE)
    {
        if (dlc < 8U)
        {
            return;
        }

        /*
         * Byte 0-3:
         * Pack voltage, unsigned, unit = 1 mV.
         */
        g_BmsVpackData.PackVoltage_mV =
            Bms_Vpack_ReadU32LE(&data[0]);

        /*
         * Byte 4-7:
         * Bus voltage, unsigned, unit = 1 mV.
         */
        g_BmsVpackData.BusVoltage_mV =
            Bms_Vpack_ReadU32LE(&data[4]);

        g_BmsVpackVoltageTimeoutTicks = 0U;
        g_BmsVpackData.VoltageValid = TRUE;

        g_BmsVpackFrameCount++;
    }
}


void Bms_Vpack_MainFunction(void)
{
    if (g_BmsVpackCurrentTimeoutTicks < BMS_VPACK_TIMEOUT_TICKS)
    {
        g_BmsVpackCurrentTimeoutTicks++;
    }

    if (g_BmsVpackVoltageTimeoutTicks < BMS_VPACK_TIMEOUT_TICKS)
    {
        g_BmsVpackVoltageTimeoutTicks++;
    }

    if (g_BmsVpackCurrentTimeoutTicks >= BMS_VPACK_TIMEOUT_TICKS)
    {
        g_BmsVpackData.CurrentValid = FALSE;
    }

    if (g_BmsVpackVoltageTimeoutTicks >= BMS_VPACK_TIMEOUT_TICKS)
    {
        g_BmsVpackData.VoltageValid = FALSE;
    }

    g_BmsVpackData.Valid =
        (boolean)(
            (g_BmsVpackData.CurrentValid == TRUE) &&
            (g_BmsVpackData.VoltageValid == TRUE) &&
            (g_BmsVpackData.AliveValid == TRUE)
        );
}