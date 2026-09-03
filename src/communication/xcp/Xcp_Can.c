#include "Xcp_Can.h"
#include "Xcp_Cfg.h"
#include "Xcp.h"

#include "FlexCAN_Ip.h"
#include "FlexCAN_Ip_Sa_PBcfg.h"

#define XCP_CAN_TX_TIMEOUT_MS   (100U)


/* ================================================================================================
 * CAN5 TX/RX configuration
 * ============================================================================================== */

static Flexcan_Ip_DataInfoType g_XcpCanTxInfo =
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

static Flexcan_Ip_DataInfoType g_XcpCanRxInfo =
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

volatile Flexcan_Ip_StatusType g_BmsCan5InitStatus;
volatile Flexcan_Ip_StatusType g_BmsCan5RxStatus;
volatile Flexcan_Ip_StatusType g_BmsCan5TxStatus;

volatile uint32 g_BmsCan5RxCount = 0U;
volatile uint32 g_BmsCan5RxId = 0U;
volatile uint8  g_BmsCan5RxDlc = 0U;

volatile uint8 g_BmsCan5RxData[8] =
{
    0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U
};

/*
 * RTD receives the complete CAN frame into this structure.
 */
static Flexcan_Ip_MsgBuffType g_BmsCan5RxMessage;


/* ================================================================================================
 * CAN5 init
 * ============================================================================================== */

Std_ReturnType Xcp_Can_Init(void)
{
    /*
     * Initialize CAN5.
     * CAN5 is reserved for XCP / development communication.
     */
    g_BmsCan5InitStatus = FlexCAN_Ip_Init(
        XCP_CAN_CFG_INSTANCE,
        &FlexCAN_State3,
        &FlexCAN_Config5
    );

    if (g_BmsCan5InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Configure CAN5 RX mailbox.
     * MB0 receives CAN ID 0x600.
     */
    g_BmsCan5InitStatus = FlexCAN_Ip_ConfigRxMb(
        XCP_CAN_CFG_INSTANCE,
        XCP_CAN_CFG_RX_MB_INDEX,
        &g_XcpCanRxInfo,
        XCP_CAN_CFG_RX_ID
    );

    if (g_BmsCan5InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Start CAN5.
     */
    g_BmsCan5InitStatus = FlexCAN_Ip_SetStartMode(
        XCP_CAN_CFG_INSTANCE
    );

    if (g_BmsCan5InitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /*
     * Arm CAN5 RX.
     */
    g_BmsCan5RxStatus = FlexCAN_Ip_Receive(
        XCP_CAN_CFG_INSTANCE,
        XCP_CAN_CFG_RX_MB_INDEX,
        &g_BmsCan5RxMessage,
        TRUE
    );

    if (g_BmsCan5RxStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}


/* ================================================================================================
 * CAN5 RX polling
 * ============================================================================================== */

void Xcp_Can_MainFunction(void)
{
    uint8 j;

    /*
     * Process CAN5 XCP RX.
     */
    FlexCAN_Ip_MainFunctionRead(
        XCP_CAN_CFG_INSTANCE,
        XCP_CAN_CFG_RX_MB_INDEX
    );

    g_BmsCan5RxStatus = FlexCAN_Ip_GetTransferStatus(
        XCP_CAN_CFG_INSTANCE,
        XCP_CAN_CFG_RX_MB_INDEX
    );

    if (g_BmsCan5RxStatus == FLEXCAN_STATUS_SUCCESS)
    {
        g_BmsCan5RxId  = g_BmsCan5RxMessage.msgId;
        g_BmsCan5RxDlc = g_BmsCan5RxMessage.dataLen;

        if (g_BmsCan5RxDlc > 8U)
        {
            g_BmsCan5RxDlc = 8U;
        }

        for (j = 0U; j < g_BmsCan5RxDlc; j++)
        {
            g_BmsCan5RxData[j] =
                g_BmsCan5RxMessage.data[j];
        }

        g_BmsCan5RxCount++;

        Xcp_ProcessCommand(
            (const uint8 *)g_BmsCan5RxData,
            g_BmsCan5RxDlc
        );

        /*
         * Re-arm CAN5 RX mailbox.
         */
        g_BmsCan5RxStatus = FlexCAN_Ip_Receive(
            XCP_CAN_CFG_INSTANCE,
            XCP_CAN_CFG_RX_MB_INDEX,
            &g_BmsCan5RxMessage,
            TRUE
        );
    }
}


/* ================================================================================================
 * CAN5 TX response
 * ============================================================================================== */

void Xcp_Can_SendResponse(
        const uint8 *data,
        uint8 length)
{
    uint8 txData[8] = {0U};
    uint8 i;

    if ((data == NULL_PTR) || (length > 8U))
    {
        return;
    }

    for (i = 0U; i < length; i++)
    {
        txData[i] = data[i];
    }

    FlexCAN_Ip_MainFunctionWrite(
        XCP_CAN_CFG_INSTANCE,
        XCP_CAN_CFG_TX_MB_INDEX
    );

    g_BmsCan5TxStatus =
        FlexCAN_Ip_SendBlocking(
            XCP_CAN_CFG_INSTANCE,
            XCP_CAN_CFG_TX_MB_INDEX,
            &g_XcpCanTxInfo,
            XCP_CAN_CFG_TX_ID,
            txData,
            XCP_CAN_TX_TIMEOUT_MS
        );
}
