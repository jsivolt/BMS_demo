#include "Bms_Can.h"
#include "Bms_Can_Cfg.h"

#include "FlexCAN_Ip.h"
#include "FlexCAN_Ip_Sa_PBcfg.h"
#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

#include "../battery/Battery_Monitor.h"
#include "../app/Bms_StateMachine.h"
#include "../safety/Fault_Manager.h"

#include "../control/Bms_Contactor.h"

#include "../battery/vAFE/Bms_Vafe.h"
#include "../battery/vPACK/Bms_Vpack.h"


#define BMS_CAN_TX_TIMEOUT_MS   (100U)
#define BMS_CAN_LED_PORT         PTA_H_HALF
#define BMS_CAN_LED_PIN          (14U)

#define PACK1_VOLTAGE_SCALE         0.1F

#define PACK1_VOLTAGE_TIMEOUT_MS    (500U)
#define BMS_CAN_MAIN_PERIOD_MS      (100U)

/* ================================================================================================
 * CAN TX configuration
 * ============================================================================================== */

static Flexcan_Ip_DataInfoType g_BmsCanTxInfo =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = 8U,

#if (FLEXCAN_IP_FEATURE_HAS_FD == STD_ON)
    .fd_enable   = FALSE,
    .fd_padding  = 0U,
    .enable_brs  = FALSE,
#endif

    .is_remote   = FALSE,
    .is_polling  = TRUE
};


/* ================================================================================================
 * CAN RX configuration
 * ============================================================================================== */

static Flexcan_Ip_DataInfoType g_BmsCanRxInfo =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = 8U,

#if (FLEXCAN_IP_FEATURE_HAS_FD == STD_ON)
    .fd_enable   = FALSE,
    .fd_padding  = 0U,
    .enable_brs  = FALSE,
#endif

    .is_remote   = FALSE,
    .is_polling  = TRUE
};


/* ================================================================================================
 * Debug variables
 *
 * Keep these non-static so they can easily be watched in S32DS Expressions.
 * ============================================================================================== */

volatile Flexcan_Ip_StatusType g_BmsCanInitStatus;
volatile Flexcan_Ip_StatusType g_BmsCanTxStatus;
volatile Flexcan_Ip_StatusType g_BmsCanRxDebugStatus;
volatile Flexcan_Ip_StatusType g_BmsCanRxControlStatus;

volatile Flexcan_Ip_StatusType g_BmsCan1InitStatus;
volatile Flexcan_Ip_StatusType g_BmsCan1TxStatus;
volatile Flexcan_Ip_StatusType g_BmsCan1RxStatus[BMS_CAN1_CFG_RX_MB_COUNT];

volatile uint32 g_BmsCan1RxCount[BMS_CAN1_CFG_RX_MB_COUNT] = { 0U, 0U, 0U, 0U };
volatile uint32 g_BmsCan1RxId[BMS_CAN1_CFG_RX_MB_COUNT] = { 0U, 0U, 0U, 0U };
volatile uint8  g_BmsCan1RxDlc[BMS_CAN1_CFG_RX_MB_COUNT] = { 0U, 0U, 0U, 0U };

volatile uint8 g_BmsCan1RxData[BMS_CAN1_CFG_RX_MB_COUNT][8] =
{
    { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U }
};

volatile Flexcan_Ip_StatusType g_BmsCan2InitStatus;
volatile Flexcan_Ip_StatusType g_BmsCan2RxStatus;

volatile uint32 g_BmsCan2RxCount = 0U;
volatile uint32 g_BmsCan2RxId = 0U;
volatile uint8  g_BmsCan2RxDlc = 0U;

volatile float32 g_CanPack1Voltage_V = 0.0F;
volatile boolean g_CanPack1VoltageValid = FALSE;
volatile uint32 g_CanPack1VoltageRxCount = 0U;
volatile uint32 g_CanPack1VoltageAgeMs = 0U;

volatile uint8 g_BmsCan2RxData[8] =
{
    0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U
};

volatile uint32 g_BmsCanRxCount = 0U;
volatile uint32 g_BmsCanRxInvalidCount = 0U;
volatile uint32 g_BmsCanRxId = 0U;
volatile uint8 g_BmsCanRxDlc = 0U;
volatile boolean g_BmsEnableRequest = FALSE;
volatile boolean g_BmsDisableRequest = FALSE;
volatile boolean g_BmsClearFaultRequest = FALSE;

volatile uint32 g_DebugEnableRequestAddr = 0U;
volatile uint32 g_DebugDisableRequestAddr = 0U;

static uint8 g_BmsCanPackStatusAliveCounter = 0U;
static uint8 g_BmsCanContactorAliveCounter = 0U;
static uint8 g_BmsCanPackCurrentAliveCounter = 0U;
static uint8 g_BmsCanPackPowerAliveCounter = 0U;

volatile uint8 g_BmsCanRxData[8] =
{
    0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U
};


/*
 * RTD receives the complete CAN frame into this structure.
 */
static Flexcan_Ip_MsgBuffType g_BmsCanRxDebugMessage;
static Flexcan_Ip_MsgBuffType g_BmsCanRxControlMessage;
static Flexcan_Ip_MsgBuffType g_BmsCan1RxMessage[BMS_CAN1_CFG_RX_MB_COUNT];
static Flexcan_Ip_MsgBuffType g_BmsCan2RxCurrentMessage;
static Flexcan_Ip_MsgBuffType g_BmsCan2RxVoltageMessage;

/* CAN1 RX mailbox index/ID lookup tables, indexed by mailbox slot 0..3. */
static const uint8 g_BmsCan1RxMbIndex[BMS_CAN1_CFG_RX_MB_COUNT] =
{
    BMS_CAN1_CFG_RX_MB0_INDEX,
    BMS_CAN1_CFG_RX_MB1_INDEX,
    BMS_CAN1_CFG_RX_MB2_INDEX,
    BMS_CAN1_CFG_RX_MB3_INDEX
};

static const uint32 g_BmsCan1RxMbId[BMS_CAN1_CFG_RX_MB_COUNT] =
{
    BMS_CAN1_CFG_RX_MB0_ID,
    BMS_CAN1_CFG_RX_MB1_ID,
    BMS_CAN1_CFG_RX_MB2_ID,
    BMS_CAN1_CFG_RX_MB3_ID
};


/* Converts a voltage (V) to a saturated 0.1V-resolution raw value for CAN payloads. */
static uint16 Bms_Can_VoltageToRaw(float voltage)
{
    float scaled;

    if (voltage <= 0.0F)
    {
        return 0U;
    }

    scaled = voltage * 10.0F;

    if (scaled > 65535.0F)
    {
        return 65535U;
    }

    return (uint16)(scaled + 0.5F);
}


/* Packs a uint32 into 4 bytes, little endian. */
static void Bms_Can_PackUint32LE(
        uint8 *data,
        uint32 value)
{
    data[0] = (uint8)(value & 0xFFUL);
    data[1] = (uint8)((value >> 8U) & 0xFFUL);
    data[2] = (uint8)((value >> 16U) & 0xFFUL);
    data[3] = (uint8)((value >> 24U) & 0xFFUL);
}


/*
 * Convert current from mA to signed 0.1 A/bit CAN raw value.
 *
 * 100 mA = 0.1 A = 1 raw count
 *
 * Positive = discharge
 * Negative = charge
 */
static sint16 Bms_Can_CurrentToRaw(sint32 current_mA)
{
    sint32 raw;

    raw = current_mA / 100;

    if (raw > 32767)
    {
        raw = 32767;
    }
    else if (raw < -32768)
    {
        raw = -32768;
    }
    else
    {
        /* No saturation required. */
    }

    return (sint16)raw;
}


/* Converts power (W) to a saturated signed 32-bit, 1 W/bit raw value for CAN payloads. */
static sint32 Bms_Can_PowerToRaw(float power_W)
{
    if (power_W > 2147483647.0f)
    {
        return (sint32)2147483647;
    }

    if (power_W < -2147483648.0f)
    {
        return (sint32)(-2147483647 - 1);
    }

    return (sint32)power_W;
}


static void Bms_Can_ProcessPack1Voltage(const uint8 *data)
{
    uint16 rawVoltage;

    rawVoltage = ((uint16)data[0]) |
                 ((uint16)data[1] << 8U);

    g_CanPack1Voltage_V =
        ((float32)rawVoltage * PACK1_VOLTAGE_SCALE);

    g_CanPack1VoltageValid = TRUE;
    g_CanPack1VoltageRxCount++;

    /* Reset timeout age whenever a valid 0x411 frame is received. */
    g_CanPack1VoltageAgeMs = 0U;
}


static void Bms_Can_ProcessRxMessage(void)
{
    if (g_BmsCanRxId != BMS_CAN_CFG_RX_DEBUG_ID)
    {
        g_BmsCanRxInvalidCount++;
        return;
    }

    if (g_BmsCanRxDlc < 1U)
    {
        g_BmsCanRxInvalidCount++;
        return;
    }

    switch (g_BmsCanRxData[0])
    {
        case 0x00U:
            /* NOP */
            break;

        case 0x01U:
            /* LED2 GREEN ON - active low */
            Siul2_Dio_Ip_WritePin(
                BMS_CAN_LED_PORT,
                BMS_CAN_LED_PIN,
                0U
            );
            break;

        case 0x02U:
            /* LED2 GREEN OFF - active low */
            Siul2_Dio_Ip_WritePin(
                BMS_CAN_LED_PORT,
                BMS_CAN_LED_PIN,
                1U
            );
            break;

        case 0x03U:
            g_BmsCanRxCount = 0U;
            break;

        default:
            g_BmsCanRxInvalidCount++;
            break;
    }
}


static void Bms_Can_ProcessControlCommand(const uint8 *data, uint8 dlc)
{
    if (dlc < 1U)
    {
        return;
    }

    switch (data[0])
    {
        case 0x00U:
            break;

        case 0x01U:
            g_BmsEnableRequest = TRUE;
            g_BmsDisableRequest = FALSE;
            break;

        case 0x02U:
            g_BmsDisableRequest = TRUE;
            g_BmsEnableRequest = FALSE;
            break;

        case 0x03U:
            g_BmsClearFaultRequest = TRUE;
            break;

        default:
            break;
    }
}


/* ================================================================================================
 * CAN initialization
 * ============================================================================================== */

Std_ReturnType Bms_Can_Init(void)
{
    uint8 i;

    g_DebugEnableRequestAddr = (uint32)&g_BmsEnableRequest;

    g_DebugDisableRequestAddr = (uint32)&g_BmsDisableRequest;

    g_BmsCanInitStatus = FlexCAN_Ip_Init(
        BMS_CAN_CFG_INSTANCE,
        &FlexCAN_State0,
        &FlexCAN_Config0
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    /*
     * Configure RX mailbox.
     *
     * MB1:
     * Standard CAN ID
     * Receive ID = 0x200
     */
    g_BmsCanInitStatus = FlexCAN_Ip_ConfigRxMb(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_DEBUG_MB_INDEX,
        &g_BmsCanRxInfo,
        BMS_CAN_CFG_RX_DEBUG_ID
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    g_BmsCanInitStatus = FlexCAN_Ip_ConfigRxMb(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_CONTROL_MB_INDEX,
        &g_BmsCanRxInfo,
        BMS_CAN_CFG_RX_CONTROL_ID
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    /*
     * Start FlexCAN.
     */
    g_BmsCanInitStatus = FlexCAN_Ip_SetStartMode(
        BMS_CAN_CFG_INSTANCE
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    /*
     * Arm the first RX operation.
     */
    g_BmsCanRxDebugStatus = FlexCAN_Ip_Receive(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_DEBUG_MB_INDEX,
        &g_BmsCanRxDebugMessage,
        TRUE
    );

    if (g_BmsCanRxDebugStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    g_BmsCanRxControlStatus = FlexCAN_Ip_Receive(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_CONTROL_MB_INDEX,
        &g_BmsCanRxControlMessage,
        TRUE
    );

    if (g_BmsCanRxControlStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    /*
     * Initialize CAN1.
     */
    g_BmsCan1InitStatus = FlexCAN_Ip_Init(
        BMS_CAN1_CFG_INSTANCE,
        &FlexCAN_State1,
        &FlexCAN_Config1
    );

    if (g_BmsCan1InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    for (i = 0U; i < BMS_CAN1_CFG_RX_MB_COUNT; i++)
    {
        g_BmsCan1InitStatus = FlexCAN_Ip_ConfigRxMb(
            BMS_CAN1_CFG_INSTANCE,
            g_BmsCan1RxMbIndex[i],
            &g_BmsCanRxInfo,
            g_BmsCan1RxMbId[i]
        );

        if (g_BmsCan1InitStatus != FLEXCAN_STATUS_SUCCESS)
        {
            return (Std_ReturnType)E_NOT_OK;
        }
    }

    g_BmsCan1InitStatus = FlexCAN_Ip_SetStartMode(
        BMS_CAN1_CFG_INSTANCE
    );

    if (g_BmsCan1InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    for (i = 0U; i < BMS_CAN1_CFG_RX_MB_COUNT; i++)
    {
        g_BmsCan1RxStatus[i] = FlexCAN_Ip_Receive(
            BMS_CAN1_CFG_INSTANCE,
            g_BmsCan1RxMbIndex[i],
            &g_BmsCan1RxMessage[i],
            TRUE
        );

        if (g_BmsCan1RxStatus[i] != FLEXCAN_STATUS_SUCCESS)
        {
            return (Std_ReturnType)E_NOT_OK;
        }
    }

    /*
     * Initialize CAN2.
     * CAN2 is reserved for ADBMS2950 simulation.
     */
    g_BmsCan2InitStatus = FlexCAN_Ip_Init(
        BMS_CAN2_CFG_INSTANCE,
        &FlexCAN_State2,
        &FlexCAN_Config2
    );

    if (g_BmsCan2InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Configure CAN2 RX MBs for the ADBMS2950 current and voltage frames.
     */
    g_BmsCan2InitStatus = FlexCAN_Ip_ConfigRxMb(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_CURRENT_MB_INDEX,
        &g_BmsCanRxInfo,
        BMS_CAN2_CFG_RX_CURRENT_ID
    );

    if (g_BmsCan2InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsCan2InitStatus = FlexCAN_Ip_ConfigRxMb(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX,
        &g_BmsCanRxInfo,
        BMS_CAN2_CFG_RX_VOLTAGE_ID
    );

    if (g_BmsCan2InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Start CAN2.
     */
    g_BmsCan2InitStatus = FlexCAN_Ip_SetStartMode(
        BMS_CAN2_CFG_INSTANCE
    );

    if (g_BmsCan2InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Arm CAN2 RX MBs.
     */
    g_BmsCan2RxStatus = FlexCAN_Ip_Receive(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_CURRENT_MB_INDEX,
        &g_BmsCan2RxCurrentMessage,
        TRUE
    );

    if (g_BmsCan2RxStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsCan2RxStatus = FlexCAN_Ip_Receive(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX,
        &g_BmsCan2RxVoltageMessage,
        TRUE
    );

    if (g_BmsCan2RxStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}


/* ================================================================================================
 * CAN TX status frame
 * ============================================================================================== */

void Bms_Can_SendStatus(void)
{
    uint8 txData[8];

    const BatteryMonitor_DataType *batteryData;

    sint16 temp1;
    sint16 temp2;
    sint16 temp3;

    uint8 flags = 0U;

    batteryData = BatteryMonitor_GetData();

    if (batteryData == NULL_PTR)
    {
        return;
    }

    temp1 = batteryData->PackTemperature_dC[0];
    temp2 = batteryData->PackTemperature_dC[1];
    temp3 = batteryData->PackTemperature_dC[2];

    /*
     * Byte 0:
     * BMS state
     *
     * 0 = INIT
     * 1 = STANDBY
     * 2 = ACTIVE
     * 3 = FAULT
     */
    txData[0] =
        (uint8)Bms_StateMachine_GetState();

    /*
     * Bytes 1-2:
     * Pack1 temperature, little endian
     */
    txData[1] = (uint8)((uint16)temp1 & 0xFFU);
    txData[2] = (uint8)(((uint16)temp1 >> 8U) & 0xFFU);

    /*
     * Bytes 3-4:
     * Pack2 temperature
     */
    txData[3] = (uint8)((uint16)temp2 & 0xFFU);
    txData[4] = (uint8)(((uint16)temp2 >> 8U) & 0xFFU);

    /*
     * Bytes 5-6:
     * Pack3 temperature
     */
    txData[5] = (uint8)((uint16)temp3 & 0xFFU);
    txData[6] = (uint8)(((uint16)temp3 >> 8U) & 0xFFU);

    /*
     * Byte 7:
     *
     * bit0 = critical fault
     * bit1 = any fault
     * bit2 = Pack1 temperature valid
     * bit3 = Pack2 temperature valid
     * bit4 = Pack3 temperature valid
     */
    if (FaultManager_HasCriticalFault() == TRUE)
    {
        flags |= (1U << 0U);
    }

    if (FaultManager_HasAnyFault() == TRUE)
    {
        flags |= (1U << 1U);
    }

    if (batteryData->PackTemperatureValid[0] == TRUE)
    {
        flags |= (1U << 2U);
    }

    if (batteryData->PackTemperatureValid[1] == TRUE)
    {
        flags |= (1U << 3U);
    }

    if (batteryData->PackTemperatureValid[2] == TRUE)
    {
        flags |= (1U << 4U);
    }

    txData[7] = flags;

    /*
     * Handle previous polling TX completion.
     */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_STATUS_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );
}


/* ================================================================================================
 * CAN TX pack status frame
 * ============================================================================================== */

void Bms_Can_SendPackStatus(void)
{
    const BatteryMonitor_DataType *batteryData;
    uint8 txData[8];
    uint16 pack1Raw;
    uint16 pack2Raw;
    uint16 pack3Raw;

    batteryData = BatteryMonitor_GetData();

    if (batteryData == NULL_PTR)
    {
        return;
    }

    pack1Raw = Bms_Can_VoltageToRaw(batteryData->PackV1);
    pack2Raw = Bms_Can_VoltageToRaw(batteryData->PackV2);
    pack3Raw = Bms_Can_VoltageToRaw(batteryData->PackV3);

    txData[0] = (uint8)(pack1Raw & 0xFFU);
    txData[1] = (uint8)((pack1Raw >> 8U) & 0xFFU);

    txData[2] = (uint8)(pack2Raw & 0xFFU);
    txData[3] = (uint8)((pack2Raw >> 8U) & 0xFFU);

    txData[4] = (uint8)(pack3Raw & 0xFFU);
    txData[5] = (uint8)((pack3Raw >> 8U) & 0xFFU);

    txData[6] =
        ((uint8)batteryData->Status & 0x0FU) |
        ((g_BmsCanPackStatusAliveCounter & 0x0FU) << 4U);

    txData[7] = 0U;

    if (batteryData->Valid == TRUE)
    {
        txData[7] |= 0x01U;
    }

    /* Handle previous polling TX completion. */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_PACK_STATUS_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );

    if (g_BmsCanTxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanPackStatusAliveCounter =
            (uint8)((g_BmsCanPackStatusAliveCounter + 1U) & 0x0FU);
    }
}


/* ================================================================================================
 * CAN TX contactor status frame
 * ============================================================================================== */

void Bms_Can_SendContactorStatus(void)
{
    uint8 txData[8] = {0U};

    Bms_ContactorOutputType pack1Output;
    Bms_ContactorOutputType pack2Output;
    Bms_ContactorOutputType pack3Output;

    txData[0] =
        (uint8)Bms_Contactor_GetState(BMS_PACK_1);

    txData[1] =
        (uint8)Bms_Contactor_GetState(BMS_PACK_2);

    txData[2] =
        (uint8)Bms_Contactor_GetState(BMS_PACK_3);

    pack1Output =
        Bms_Contactor_GetOutputs(BMS_PACK_1);

    pack2Output =
        Bms_Contactor_GetOutputs(BMS_PACK_2);

    pack3Output =
        Bms_Contactor_GetOutputs(BMS_PACK_3);

    /*
     * Pack 1 contactor outputs
     */
    if (pack1Output.negative == TRUE)
    {
        txData[3] |= 0x01U;
    }

    if (pack1Output.positive == TRUE)
    {
        txData[3] |= 0x02U;
    }

    if (pack1Output.precharge == TRUE)
    {
        txData[3] |= 0x04U;
    }

    /*
     * Pack 2 contactor outputs
     */
    if (pack2Output.negative == TRUE)
    {
        txData[4] |= 0x01U;
    }

    if (pack2Output.positive == TRUE)
    {
        txData[4] |= 0x02U;
    }

    if (pack2Output.precharge == TRUE)
    {
        txData[4] |= 0x04U;
    }

    /*
     * Pack 3 contactor outputs
     */
    if (pack3Output.negative == TRUE)
    {
        txData[5] |= 0x01U;
    }

    if (pack3Output.positive == TRUE)
    {
        txData[5] |= 0x02U;
    }

    if (pack3Output.precharge == TRUE)
    {
        txData[5] |= 0x04U;
    }

    txData[6] =
        (uint8)(g_BmsCanContactorAliveCounter & 0x0FU);

    txData[7] = 0U;

    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_CONTACTOR_STATUS_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );

    if (g_BmsCanTxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanContactorAliveCounter =
            (uint8)((g_BmsCanContactorAliveCounter + 1U) & 0x0FU);
    }
}


/* ================================================================================================
 * CAN TX Pack1 + Pack2 fault frame
 * CAN ID: 0x303
 * ============================================================================================== */

void Bms_Can_SendFaultStatus1(void)
{
    uint8 txData[8];

    FaultMaskType pack1Faults;
    FaultMaskType pack2Faults;

    pack1Faults =
        FaultManager_GetPackFaults(FAULT_PACK_1);

    pack2Faults =
        FaultManager_GetPackFaults(FAULT_PACK_2);

    /*
     * Bytes 0-3:
     * Pack1 fault mask, little endian
     */
    Bms_Can_PackUint32LE(
        &txData[0],
        (uint32)pack1Faults
    );

    /*
     * Bytes 4-7:
     * Pack2 fault mask, little endian
     */
    Bms_Can_PackUint32LE(
        &txData[4],
        (uint32)pack2Faults
    );

    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_FAULT_12_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );
}


/* ================================================================================================
 * CAN TX Pack3 + System fault frame
 * CAN ID: 0x304
 * ============================================================================================== */

void Bms_Can_SendFaultStatus2(void)
{
    uint8 txData[8];

    FaultMaskType pack3Faults;
    FaultMaskType systemFaults;

    pack3Faults =
        FaultManager_GetPackFaults(FAULT_PACK_3);

    systemFaults =
        FaultManager_GetSystemFaults();

    /*
     * Bytes 0-3:
     * Pack3 fault mask, little endian
     */
    Bms_Can_PackUint32LE(
        &txData[0],
        (uint32)pack3Faults
    );

    /*
     * Bytes 4-7:
     * System fault mask, little endian
     */
    Bms_Can_PackUint32LE(
        &txData[4],
        (uint32)systemFaults
    );

    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_FAULT_3_SYSTEM_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );
}


/* ================================================================================================
 * CAN TX cell summary frame
 * CAN ID: 0x305
 * ============================================================================================== */

void Bms_Can_SendCellSummary(void)
{
    uint8 txData[8];
    uint16 minCell_mV;
    uint16 maxCell_mV;
    uint16 deltaCell_mV;
    uint8 minIndex;
    uint8 maxIndex;
    uint8 statusByte;

    minCell_mV = g_BmsVafeData.MinCellVoltage_mV;
    maxCell_mV = g_BmsVafeData.MaxCellVoltage_mV;
    deltaCell_mV = g_BmsVafeData.DeltaCellVoltage_mV;

    minIndex = g_BmsVafeData.MinCellIndex;
    maxIndex = g_BmsVafeData.MaxCellIndex;

    /*
     * Byte0-1: MinCellVoltage, little-endian, 1 mV/bit
     */
    txData[0] = (uint8)(minCell_mV & 0xFFU);
    txData[1] = (uint8)((minCell_mV >> 8U) & 0xFFU);

    /*
     * Byte2-3: MaxCellVoltage
     */
    txData[2] = (uint8)(maxCell_mV & 0xFFU);
    txData[3] = (uint8)((maxCell_mV >> 8U) & 0xFFU);

    /*
     * Byte4-5: DeltaCellVoltage
     */
    txData[4] = (uint8)(deltaCell_mV & 0xFFU);
    txData[5] = (uint8)((deltaCell_mV >> 8U) & 0xFFU);

    /*
     * Byte6:
     * bit0-3 = MinCellIndex
     * bit4-7 = MaxCellIndex
     */
    txData[6] =
        (uint8)((minIndex & 0x0FU) |
               ((maxIndex & 0x0FU) << 4U));

    /*
     * Byte7:
     * bit0 = CellVoltageValid
     * bit1 = CellImbalanceFault
     */
    statusByte = 0U;

    if (g_BmsVafeData.DataValid == TRUE)
    {
        statusByte |= 0x01U;
    }

    if (FaultManager_IsSystemFaultActive(
            FAULT_CELL_IMBALANCE) == TRUE)
    {
        statusByte |= 0x02U;
    }

    txData[7] = statusByte;

    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_CELL_SUMMARY_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );
}


/* ================================================================================================
 * CAN TX pack current status frame
 * CAN ID: 0x306
 *
 * Byte0-1 : Pack1 current, signed, little endian, 0.1 A/bit
 * Byte2-3 : Pack2 current, signed, little endian, 0.1 A/bit
 * Byte4-5 : Pack3 current, signed, little endian, 0.1 A/bit
 *
 * Byte6:
 * bit0 = Pack1 current valid
 * bit1 = Pack2 current valid
 * bit2 = Pack3 current valid
 *
 * Byte7:
 * bit0-3 = alive counter
 * ============================================================================================== */

void Bms_Can_SendPackCurrent(void)
{
    const BatteryMonitor_DataType *batteryData;

    uint8 txData[8] = {0U};

    sint16 pack1Raw;
    sint16 pack2Raw;
    sint16 pack3Raw;

    uint16 pack1RawU;
    uint16 pack2RawU;
    uint16 pack3RawU;

    batteryData = BatteryMonitor_GetData();

    if (batteryData == NULL_PTR)
    {
        return;
    }

    /*
     * Convert mA -> 0.1 A/bit.
     */
    pack1Raw =
        Bms_Can_CurrentToRaw(
            batteryData->PackCurrent_mA[0]
        );

    pack2Raw =
        Bms_Can_CurrentToRaw(
            batteryData->PackCurrent_mA[1]
        );

    pack3Raw =
        Bms_Can_CurrentToRaw(
            batteryData->PackCurrent_mA[2]
        );

    /*
     * Preserve signed two's-complement bit pattern
     * while packing into CAN bytes.
     */
    pack1RawU = (uint16)pack1Raw;
    pack2RawU = (uint16)pack2Raw;
    pack3RawU = (uint16)pack3Raw;

    /*
     * Byte0-1: Pack1 current
     */
    txData[0] = (uint8)(pack1RawU & 0xFFU);
    txData[1] = (uint8)((pack1RawU >> 8U) & 0xFFU);

    /*
     * Byte2-3: Pack2 current
     */
    txData[2] = (uint8)(pack2RawU & 0xFFU);
    txData[3] = (uint8)((pack2RawU >> 8U) & 0xFFU);

    /*
     * Byte4-5: Pack3 current
     */
    txData[4] = (uint8)(pack3RawU & 0xFFU);
    txData[5] = (uint8)((pack3RawU >> 8U) & 0xFFU);

    /*
     * Byte6: current validity flags.
     */
    txData[6] = 0U;

    if (batteryData->PackCurrentValid[0] == TRUE)
    {
        txData[6] |= 0x01U;
    }

    if (batteryData->PackCurrentValid[1] == TRUE)
    {
        txData[6] |= 0x02U;
    }

    if (batteryData->PackCurrentValid[2] == TRUE)
    {
        txData[6] |= 0x04U;
    }

    /*
     * Byte7:
     * bit0-3 = alive counter
     */
    txData[7] =
        (uint8)(g_BmsCanPackCurrentAliveCounter & 0x0FU);

    /*
     * Handle previous polling TX completion.
     */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_PACK_CURRENT_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );

    if (g_BmsCanTxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanPackCurrentAliveCounter =
            (uint8)(
                (g_BmsCanPackCurrentAliveCounter + 1U) &
                0x0FU
            );
    }
}


void Bms_Can_SendPackPower(void)
{
    const BatteryMonitor_DataType *batteryData;

    uint8 txData[8] = {0U};

    sint32 powerRaw;
    uint32 powerRawU;

    batteryData = BatteryMonitor_GetData();

    if (batteryData == NULL_PTR)
    {
        return;
    }

    powerRaw =
        Bms_Can_PowerToRaw(
            batteryData->PackPower_W[0]
        );

    powerRawU = (uint32)powerRaw;

    /*
     * Byte0-3: Pack1 power, signed 32-bit,
     * little endian, 1 W/bit.
     */
    txData[0] = (uint8)(powerRawU & 0xFFU);
    txData[1] = (uint8)((powerRawU >> 8U) & 0xFFU);
    txData[2] = (uint8)((powerRawU >> 16U) & 0xFFU);
    txData[3] = (uint8)((powerRawU >> 24U) & 0xFFU);

    /*
     * Byte4 bit0: Pack1 power valid.
     */
    if (batteryData->PackPowerValid[0] == TRUE)
    {
        txData[4] |= 0x01U;
    }

    /*
     * Byte5-6 reserved.
     */

    /*
     * Byte7 bit0-3: alive counter.
     */
    txData[7] =
        (uint8)(g_BmsCanPackPowerAliveCounter & 0x0FU);

    /*
     * Handle previous polling TX completion.
     */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCanTxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN_CFG_TX_PACK_POWER_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );

    if (g_BmsCanTxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanPackPowerAliveCounter =
            (uint8)(
                (g_BmsCanPackPowerAliveCounter + 1U) &
                0x0FU
            );
    }
}


/* ================================================================================================
 * CAN RX polling
 * ============================================================================================== */

void Bms_Can_MainFunction(void)
{
    uint8 i;

    /*
     * Pack1 voltage CAN timeout supervision.
     * Called every 100 ms.
     */
    if (g_CanPack1VoltageValid == TRUE)
    {
        if ((g_CanPack1VoltageAgeMs + BMS_CAN_MAIN_PERIOD_MS) >=
            PACK1_VOLTAGE_TIMEOUT_MS)
        {
            g_CanPack1VoltageAgeMs = PACK1_VOLTAGE_TIMEOUT_MS;
            g_CanPack1VoltageValid = FALSE;
        }
        else
        {
            g_CanPack1VoltageAgeMs += BMS_CAN_MAIN_PERIOD_MS;
        }
    }

    /* Process 0x200 debug mailbox. */
    FlexCAN_Ip_MainFunctionRead(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_DEBUG_MB_INDEX
    );

    g_BmsCanRxDebugStatus = FlexCAN_Ip_GetTransferStatus(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_DEBUG_MB_INDEX
    );


    if (g_BmsCanRxDebugStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanRxId  = g_BmsCanRxDebugMessage.msgId;
        g_BmsCanRxDlc = g_BmsCanRxDebugMessage.dataLen;

        if (g_BmsCanRxDlc > 8U)
        {
            g_BmsCanRxDlc = 8U;
        }

        for (i = 0U; i < g_BmsCanRxDlc; i++)
        {
            g_BmsCanRxData[i] = g_BmsCanRxDebugMessage.data[i];
        }

        g_BmsCanRxCount++;
        Bms_Can_ProcessRxMessage();

        g_BmsCanRxDebugStatus = FlexCAN_Ip_Receive(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_RX_DEBUG_MB_INDEX,
            &g_BmsCanRxDebugMessage,
            TRUE
        );
    }


    /* Process 0x201 control mailbox. */
    FlexCAN_Ip_MainFunctionRead(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_CONTROL_MB_INDEX
    );

    g_BmsCanRxControlStatus = FlexCAN_Ip_GetTransferStatus(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_CONTROL_MB_INDEX
    );

    if (g_BmsCanRxControlStatus == FLEXCAN_STATUS_SUCCESS)
    {
        Bms_Can_ProcessControlCommand(
            g_BmsCanRxControlMessage.data,
            g_BmsCanRxControlMessage.dataLen
        );

        g_BmsCanRxControlStatus = FlexCAN_Ip_Receive(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_RX_CONTROL_MB_INDEX,
            &g_BmsCanRxControlMessage,
            TRUE
        );
    }


    /* Process CAN1 RX mailboxes (MB0..MB3). */
    for (i = 0U; i < BMS_CAN1_CFG_RX_MB_COUNT; i++)
    {
        FlexCAN_Ip_MainFunctionRead(
            BMS_CAN1_CFG_INSTANCE,
            g_BmsCan1RxMbIndex[i]
        );

        g_BmsCan1RxStatus[i] = FlexCAN_Ip_GetTransferStatus(
            BMS_CAN1_CFG_INSTANCE,
            g_BmsCan1RxMbIndex[i]
        );

        if (g_BmsCan1RxStatus[i] == FLEXCAN_STATUS_SUCCESS)
        {
            uint8 j;

            g_BmsCan1RxId[i]  = g_BmsCan1RxMessage[i].msgId;
            g_BmsCan1RxDlc[i] = g_BmsCan1RxMessage[i].dataLen;

            if (g_BmsCan1RxDlc[i] > 8U)
            {
                g_BmsCan1RxDlc[i] = 8U;
            }

            for (j = 0U; j < g_BmsCan1RxDlc[i]; j++)
            {
                g_BmsCan1RxData[i][j] = g_BmsCan1RxMessage[i].data[j];
            }

            g_BmsCan1RxCount[i]++;

            Bms_Vafe_ProcessFrame(
                g_BmsCan1RxId[i],
                (const uint8 *)g_BmsCan1RxData[i],
                g_BmsCan1RxDlc[i]
            );

            g_BmsCan1RxStatus[i] = FlexCAN_Ip_Receive(
                BMS_CAN1_CFG_INSTANCE,
                g_BmsCan1RxMbIndex[i],
                &g_BmsCan1RxMessage[i],
                TRUE
            );
        }
    }

    /*
     * Process CAN2 ADBMS2950 simulator RX - current frame.
     */
    FlexCAN_Ip_MainFunctionRead(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_CURRENT_MB_INDEX
    );

    g_BmsCan2RxStatus = FlexCAN_Ip_GetTransferStatus(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_CURRENT_MB_INDEX
    );

    if (g_BmsCan2RxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        uint8 j;

        g_BmsCan2RxId  = g_BmsCan2RxCurrentMessage.msgId;
        g_BmsCan2RxDlc = g_BmsCan2RxCurrentMessage.dataLen;

        if (g_BmsCan2RxDlc > 8U)
        {
            g_BmsCan2RxDlc = 8U;
        }

        for (j = 0U; j < g_BmsCan2RxDlc; j++)
        {
            g_BmsCan2RxData[j] =
                g_BmsCan2RxCurrentMessage.data[j];
        }

        g_BmsCan2RxCount++;

        Bms_Vpack_ProcessFrame(
            g_BmsCan2RxCurrentMessage.msgId,
            g_BmsCan2RxCurrentMessage.dataLen,
            g_BmsCan2RxCurrentMessage.data
        );

        /*
         * Re-arm CAN2 current RX MB.
         */
        g_BmsCan2RxStatus = FlexCAN_Ip_Receive(
            BMS_CAN2_CFG_INSTANCE,
            BMS_CAN2_CFG_RX_CURRENT_MB_INDEX,
            &g_BmsCan2RxCurrentMessage,
            TRUE
        );
    }

    /*
     * Process CAN2 ADBMS2950 simulator RX - voltage frame.
     */
    FlexCAN_Ip_MainFunctionRead(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX
    );

    g_BmsCan2RxStatus = FlexCAN_Ip_GetTransferStatus(
        BMS_CAN2_CFG_INSTANCE,
        BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX
    );

    if (g_BmsCan2RxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        uint8 j;

        g_BmsCan2RxId  = g_BmsCan2RxVoltageMessage.msgId;
        g_BmsCan2RxDlc = g_BmsCan2RxVoltageMessage.dataLen;

        if (g_BmsCan2RxDlc > 8U)
        {
            g_BmsCan2RxDlc = 8U;
        }

        for (j = 0U; j < g_BmsCan2RxDlc; j++)
        {
            g_BmsCan2RxData[j] =
                g_BmsCan2RxVoltageMessage.data[j];
        }

        g_BmsCan2RxCount++;

        Bms_Vpack_ProcessFrame(
            g_BmsCan2RxVoltageMessage.msgId,
            g_BmsCan2RxVoltageMessage.dataLen,
            g_BmsCan2RxVoltageMessage.data
        );

        if (g_BmsCan2RxVoltageMessage.msgId ==
            BMS_CAN2_CFG_RX_VOLTAGE_ID)
        {
            Bms_Can_ProcessPack1Voltage(
                g_BmsCan2RxVoltageMessage.data
            );
        }

        /*
         * Re-arm CAN2 voltage RX MB.
         */
        g_BmsCan2RxStatus = FlexCAN_Ip_Receive(
            BMS_CAN2_CFG_INSTANCE,
            BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX,
            &g_BmsCan2RxVoltageMessage,
            TRUE
        );
    }
}


void Bms_Can1_SendTest(void)
{
    uint8 txData[8] =
    {
        0x11U,
        0x22U,
        0x33U,
        0x44U,
        0x55U,
        0x66U,
        0x77U,
        0x88U
    };

    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN1_CFG_INSTANCE,
        BMS_CAN1_CFG_TX_MB_INDEX
    );

    g_BmsCan1TxStatus =
        FlexCAN_Ip_SendBlocking(
            BMS_CAN1_CFG_INSTANCE,
            BMS_CAN1_CFG_TX_MB_INDEX,
            &g_BmsCanTxInfo,
            BMS_CAN1_CFG_TX_TEST_ID,
            txData,
            BMS_CAN_TX_TIMEOUT_MS
        );
}
