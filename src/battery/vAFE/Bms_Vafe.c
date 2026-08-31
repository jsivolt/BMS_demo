#include "Bms_Vafe.h"

Bms_Vafe_DataType g_BmsVafeData;

static boolean g_BmsVafeFrameReceived[BMS_VAFE_FRAME_COUNT];

static uint16 g_BmsVafePendingCellVoltage_mV[BMS_VAFE_CELL_COUNT];

static uint8 g_BmsVafeActiveCounter = 0U;
static boolean g_BmsVafeCycleActive = FALSE;

static void Bms_Vafe_Decode4Cells(
    uint8 startIndex,
    const uint8 *data
)
{
    g_BmsVafePendingCellVoltage_mV[startIndex + 0U] =
        ((uint16)data[1] << 8U) | (uint16)data[0];

    g_BmsVafePendingCellVoltage_mV[startIndex + 1U] =
        ((uint16)data[3] << 8U) | (uint16)data[2];

    g_BmsVafePendingCellVoltage_mV[startIndex + 2U] =
        ((uint16)data[5] << 8U) | (uint16)data[4];

    g_BmsVafePendingCellVoltage_mV[startIndex + 3U] =
        ((uint16)data[7] << 8U) | (uint16)data[6];
}

static void Bms_Vafe_UpdateStatistics(void)
{
    uint8 i;
    uint16 minVoltage;
    uint16 maxVoltage;
    uint8 minIndex;
    uint8 maxIndex;

    minVoltage = g_BmsVafeData.CellVoltage_mV[0];
    maxVoltage = g_BmsVafeData.CellVoltage_mV[0];

    minIndex = 0U;
    maxIndex = 0U;

    for (i = 1U; i < BMS_VAFE_CELL_COUNT; i++)
    {
        if (g_BmsVafeData.CellVoltage_mV[i] < minVoltage)
        {
            minVoltage = g_BmsVafeData.CellVoltage_mV[i];
            minIndex = i;
        }

        if (g_BmsVafeData.CellVoltage_mV[i] > maxVoltage)
        {
            maxVoltage = g_BmsVafeData.CellVoltage_mV[i];
            maxIndex = i;
        }
    }

    g_BmsVafeData.MinCellVoltage_mV = minVoltage;
    g_BmsVafeData.MaxCellVoltage_mV = maxVoltage;
    g_BmsVafeData.DeltaCellVoltage_mV =
        (uint16)(maxVoltage - minVoltage);

    g_BmsVafeData.MinCellIndex = minIndex;
    g_BmsVafeData.MaxCellIndex = maxIndex;
}

static void Bms_Vafe_PublishMeasurement(void)
{
    uint8 i;

    for (i = 0U; i < BMS_VAFE_CELL_COUNT; i++)
    {
        g_BmsVafeData.CellVoltage_mV[i] =
            g_BmsVafePendingCellVoltage_mV[i];
    }

    Bms_Vafe_UpdateStatistics();

    g_BmsVafeData.DataValid = TRUE;
}

void Bms_Vafe_Init(void)
{
    uint8 i;

    for (i = 0U; i < BMS_VAFE_CELL_COUNT; i++)
    {
        g_BmsVafeData.CellVoltage_mV[i] = 0U;
        g_BmsVafePendingCellVoltage_mV[i] = 0U;
    }

    for (i = 0U; i < BMS_VAFE_FRAME_COUNT; i++)
    {
        g_BmsVafeFrameReceived[i] = FALSE;
    }

    g_BmsVafeData.MinCellVoltage_mV = 0U;
    g_BmsVafeData.MaxCellVoltage_mV = 0U;
    g_BmsVafeData.DeltaCellVoltage_mV = 0U;

    g_BmsVafeData.MinCellIndex = 0U;
    g_BmsVafeData.MaxCellIndex = 0U;

    g_BmsVafeData.MeasurementCounter = 0U;
    g_BmsVafeData.HeaderValid = FALSE;

    g_BmsVafeActiveCounter = 0U;
    g_BmsVafeCycleActive = FALSE;

    g_BmsVafeData.DataValid = FALSE;
}

void Bms_Vafe_ProcessFrame(uint32 canId, const uint8 *data, uint8 dlc)
{
    if (data == NULL_PTR)
    {
        return;
    }

    switch (canId)
    {
        case 0x401U:
            if ((dlc < 8U) || (g_BmsVafeCycleActive == FALSE))
            {
                return;
            }

            Bms_Vafe_Decode4Cells(0U, data);
            g_BmsVafeFrameReceived[0] = TRUE;
            break;

        case 0x402U:
            if ((dlc < 8U) || (g_BmsVafeCycleActive == FALSE))
            {
                return;
            }

            Bms_Vafe_Decode4Cells(4U, data);
            g_BmsVafeFrameReceived[1] = TRUE;
            break;

        case 0x403U:
            if ((dlc < 8U) || (g_BmsVafeCycleActive == FALSE))
            {
                return;
            }

            Bms_Vafe_Decode4Cells(8U, data);
            g_BmsVafeFrameReceived[2] = TRUE;
            break;

        case 0x404U:
            if ((dlc < 8U) || (g_BmsVafeCycleActive == FALSE))
            {
                return;
            }

            Bms_Vafe_Decode4Cells(12U, data);
            g_BmsVafeFrameReceived[3] = TRUE;
            break;

        case 0x405U:
            if (dlc < 1U)
            {
                return;
            }

            /*
             * Start a new AFE measurement cycle.
             * Any incomplete previous cycle is discarded.
             */
            g_BmsVafeFrameReceived[0] = FALSE;
            g_BmsVafeFrameReceived[1] = FALSE;
            g_BmsVafeFrameReceived[2] = FALSE;
            g_BmsVafeFrameReceived[3] = FALSE;

            g_BmsVafeActiveCounter = data[0];
            g_BmsVafeCycleActive = TRUE;

            g_BmsVafeData.HeaderValid = TRUE;
            break;

        default:
            return;
    }

    /*
     * Only publish a new AFE measurement after all
     * four cell-voltage frames have been received.
     */
    if ((g_BmsVafeCycleActive == TRUE) &&
        (g_BmsVafeFrameReceived[0] == TRUE) &&
        (g_BmsVafeFrameReceived[1] == TRUE) &&
        (g_BmsVafeFrameReceived[2] == TRUE) &&
        (g_BmsVafeFrameReceived[3] == TRUE))
    {
        Bms_Vafe_PublishMeasurement();

        /*
         * Publish the counter only when the complete
         * measurement has been accepted.
         */
        g_BmsVafeData.MeasurementCounter = g_BmsVafeActiveCounter;

        /*
         * Start collecting a completely new AFE measurement.
         *
         * This prevents a new frame from being mixed with
         * frames belonging to the previous measurement.
         */
        g_BmsVafeFrameReceived[0] = FALSE;
        g_BmsVafeFrameReceived[1] = FALSE;
        g_BmsVafeFrameReceived[2] = FALSE;
        g_BmsVafeFrameReceived[3] = FALSE;

        g_BmsVafeCycleActive = FALSE;
    }
}