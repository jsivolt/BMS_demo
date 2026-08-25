#include "Bms_Can.h"
#include "Bms_Can_Cfg.h"

#include "FlexCAN_Ip.h"
#include "FlexCAN_Ip_Sa_PBcfg.h"
#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

#include "../battery/Battery_Monitor.h"
#include "../app/Bms_StateMachine.h"
#include "../safety/Fault_Manager.h"


#define BMS_CAN_TX_TIMEOUT_MS   (100U)
#define BMS_CAN_LED_PORT         PTA_H_HALF
#define BMS_CAN_LED_PIN          (14U)


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
        g_BmsCanPackStatusAliveCounter++;

        if (g_BmsCanPackStatusAliveCounter >= 16U)
        {
            g_BmsCanPackStatusAliveCounter = 0U;
        }
    }
}


/* ================================================================================================
 * CAN RX polling
 * ============================================================================================== */

void Bms_Can_MainFunction(void)
{
    uint8 i;


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
}
