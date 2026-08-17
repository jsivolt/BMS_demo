#include "Bms_Can.h"
#include "Bms_Can_Cfg.h"

#include "FlexCAN_Ip.h"
#include "FlexCAN_Ip_Sa_PBcfg.h"
#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"


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
volatile Flexcan_Ip_StatusType g_BmsCanRxStatus;

volatile uint32 g_BmsCanRxCount = 0U;
volatile uint32 g_BmsCanRxInvalidCount = 0U;
volatile uint32 g_BmsCanRxId = 0U;
volatile uint8 g_BmsCanRxDlc = 0U;

volatile uint8 g_BmsCanRxData[8] =
{
    0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U
};


/*
 * RTD receives the complete CAN frame into this structure.
 */
static Flexcan_Ip_MsgBuffType g_BmsCanRxMessage;


static void Bms_Can_ProcessRxMessage(void)
{
    if (g_BmsCanRxId != BMS_CAN_CFG_RX_COMMAND_ID)
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


/* ================================================================================================
 * CAN initialization
 * ============================================================================================== */

Std_ReturnType Bms_Can_Init(void)
{
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
        BMS_CAN_CFG_RX_MB_INDEX,
        &g_BmsCanRxInfo,
        BMS_CAN_CFG_RX_COMMAND_ID
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
    g_BmsCanRxStatus = FlexCAN_Ip_Receive(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_MB_INDEX,
        &g_BmsCanRxMessage,
        TRUE
    );

    if (g_BmsCanRxStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }


    return (Std_ReturnType)E_OK;
}


/* ================================================================================================
 * CAN TX test
 * ============================================================================================== */

void Bms_Can_SendTest(void)
{
    static const uint8 testData[8] =
    {
        0x11U, 0x22U, 0x33U, 0x44U,
        0x55U, 0x66U, 0x77U, 0x88U
    };


    /*
     * Handle previous polling TX completion.
     */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX
    );


    g_BmsCanTxStatus = FlexCAN_Ip_SendBlocking(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_TX_MB_INDEX,
        &g_BmsCanTxInfo,
        BMS_CAN_CFG_TX_STATUS_ID,
        testData,
        BMS_CAN_TX_TIMEOUT_MS
    );
}


/* ================================================================================================
 * CAN RX polling
 * ============================================================================================== */

void Bms_Can_MainFunction(void)
{
    uint8 i;


    /*
     * Let RTD process polling RX completion.
     */
    FlexCAN_Ip_MainFunctionRead(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_MB_INDEX
    );


    /*
     * Check whether reception completed.
     */
    g_BmsCanRxStatus = FlexCAN_Ip_GetTransferStatus(
        BMS_CAN_CFG_INSTANCE,
        BMS_CAN_CFG_RX_MB_INDEX
    );


    if (g_BmsCanRxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCanRxId  = g_BmsCanRxMessage.msgId;
        g_BmsCanRxDlc = g_BmsCanRxMessage.dataLen;

        if (g_BmsCanRxDlc > 8U)
        {
            g_BmsCanRxDlc = 8U;
        }

        for (i = 0U; i < g_BmsCanRxDlc; i++)
        {
            g_BmsCanRxData[i] = g_BmsCanRxMessage.data[i];
        }

        g_BmsCanRxCount++;
        Bms_Can_ProcessRxMessage();

        g_BmsCanRxStatus = FlexCAN_Ip_Receive(
            BMS_CAN_CFG_INSTANCE,
            BMS_CAN_CFG_RX_MB_INDEX,
            &g_BmsCanRxMessage,
            TRUE
        );
    }
}
