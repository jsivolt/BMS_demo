#include "Bms_Can.h"

#include "FlexCAN_Ip.h"
#include "FlexCAN_Ip_Sa_PBcfg.h"

#define BMS_CAN_INSTANCE        (0U)
#define BMS_CAN_TX_MB           (0U)
#define BMS_CAN_TEST_ID         (0x123U)
#define BMS_CAN_TX_TIMEOUT_MS   (100U)

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

/* temporary: expose last init/TX status for debugging (success/timeout/bus-off) */
volatile Flexcan_Ip_StatusType g_BmsCanInitStatus;
volatile Flexcan_Ip_StatusType g_BmsCanTxStatus;

Std_ReturnType Bms_Can_Init(void)
{
    g_BmsCanInitStatus = FlexCAN_Ip_Init(
        BMS_CAN_INSTANCE,
        &FlexCAN_State0,
        &FlexCAN_Config0
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsCanInitStatus = FlexCAN_Ip_SetStartMode(
        BMS_CAN_INSTANCE
    );

    if (g_BmsCanInitStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}

void Bms_Can_SendTest(void)
{
    static const uint8 testData[8] =
    {
        0x11U, 0x22U, 0x33U, 0x44U,
        0x55U, 0x66U, 0x77U, 0x88U
    };

    /* 清理/处理上一次 polling TX completion */
    FlexCAN_Ip_MainFunctionWrite(
        BMS_CAN_INSTANCE,
        BMS_CAN_TX_MB
    );

    g_BmsCanTxStatus = FlexCAN_Ip_SendBlocking(
        BMS_CAN_INSTANCE,
        BMS_CAN_TX_MB,
        &g_BmsCanTxInfo,
        BMS_CAN_TEST_ID,
        testData,
        BMS_CAN_TX_TIMEOUT_MS
    );
}