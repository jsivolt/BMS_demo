#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Can.h"


/* ================================================================================================
 * Debug variables
 *
 * Keep these non-static so they can easily be watched in S32DS Expressions.
 * ============================================================================================== */

volatile boolean g_BmsXcpConnected = FALSE;
volatile uint32 g_BmsXcpConnectCount = 0U;

volatile uint32 g_BmsXcpMta = 0U;
volatile uint8  g_BmsXcpMtaExt = 0U;

volatile uint32 g_BmsXcpSetMtaCount = 0U;
volatile uint32 g_BmsXcpUploadCount = 0U;

volatile uint32 g_BmsXcpTestCalibration = 100U;
volatile uint32 g_BmsXcpDownloadCount = 0U;

volatile uint32 g_BmsXcpDownloadAddress = 0U;
volatile uint32 g_BmsXcpDownloadCalAddress = 0U;
volatile uint8  g_BmsXcpDownloadLength = 0U;
volatile boolean g_BmsXcpDownloadWritable = FALSE;
volatile uint32 g_BmsXcpDownloadEnteredCount = 0U;


static void Xcp_ProcessConnect(
        const uint8 *data,
        uint8 dlc)
{
    uint8 response[8];

    if ((data == NULL_PTR) || (dlc < 2U))
    {
        return;
    }

    /*
     * CONNECT:
     * Byte0 = 0xFF
     * Byte1 = mode
     *
     * Only Normal mode (0x00) supported for now.
     */
    if (data[1] != 0x00U)
    {
        return;
    }

    g_BmsXcpConnected = TRUE;
    g_BmsXcpConnectCount++;

    /*
     * XCP CONNECT positive response.
     */
    response[0] = 0xFFU;  /* RES */
    response[1] = 0x00U;  /* RESOURCE */
    response[2] = 0x00U;  /* COMM_MODE_BASIC */
    response[3] = XCP_CFG_MAX_CTO;

    response[4] = XCP_CFG_MAX_DTO;  /* MAX_DTO low */
    response[5] = 0x00U;            /* MAX_DTO high */

    response[6] = XCP_CFG_PROTOCOL_VERSION;
    response[7] = XCP_CFG_TRANSPORT_VERSION;

    Xcp_Can_SendResponse(
        response,
        8U
    );
}


/* Reads a little-endian uint32 out of a 4-byte XCP field. */
static uint32 Xcp_ReadUint32LE(
        const uint8 *data)
{
    return ((uint32)data[0]) |
           ((uint32)data[1] << 8U) |
           ((uint32)data[2] << 16U) |
           ((uint32)data[3] << 24U);
}


static boolean Xcp_IsValidRamRange(
        uint32 address,
        uint32 length)
{
    uint32 endAddress;

    if (length == 0U)
    {
        return FALSE;
    }

    endAddress = address + length - 1U;

    /*
     * First bring-up whitelist.
     * Allow only SRAM around the current application RAM area.
     *
     * We can tighten/expand this later based on the linker map.
     */
    if ((address >= XCP_CFG_VALID_RAM_START) &&
        (endAddress <= XCP_CFG_VALID_RAM_END) &&
        (endAddress >= address))
    {
        return TRUE;
    }

    return FALSE;
}


static boolean Xcp_IsWritableRange(
        uint32 address,
        uint32 length)
{
    uint32 calAddress;

    calAddress = (uint32)&g_BmsXcpTestCalibration;

    if ((address == calAddress) &&
        (length <= sizeof(g_BmsXcpTestCalibration)))
    {
        return TRUE;
    }

    return FALSE;
}


void Xcp_ProcessCommand(
        const uint8 *data,
        uint8 dlc)
{
    if ((data == NULL_PTR) || (dlc == 0U))
    {
        return;
    }

    switch (data[0])
    {
        case 0xFFU:
            /*
             * XCP CONNECT
             */
            Xcp_ProcessConnect(
                data,
                dlc
            );
            break;

        case 0xFDU:
        {
            /*
             * XCP GET_STATUS
             */
            uint8 response[8] =
            {
                0xFFU, /* RES */
                0x00U, /* Session status */
                0x00U, /* Resource protection */
                0x00U,
                0x00U,
                0x00U,
                0x00U,
                0x00U
            };

            Xcp_Can_SendResponse(
                response,
                8U
            );

            break;
        }

        case 0xF6U:
        {
            /*
             * XCP SET_MTA
             */
            uint8 response[8] = {0U};

            if (dlc < 8U)
            {
                break;
            }

            g_BmsXcpMtaExt = data[3];

            g_BmsXcpMta =
                Xcp_ReadUint32LE(&data[4]);

            g_BmsXcpSetMtaCount++;

            response[0] = 0xFFU;

            Xcp_Can_SendResponse(
                response,
                8U
            );

            break;
        }

        case 0xF5U:
        {
            /*
             * XCP UPLOAD
             */
            uint8 response[8] = {0U};
            uint8 count;
            uint8 i;
            const volatile uint8 *src;

            if (dlc < 2U)
            {
                break;
            }

            count = data[1];

            if ((count == 0U) || (count > 7U))
            {
                break;
            }

            if (Xcp_IsValidRamRange(
                    g_BmsXcpMta,
                    (uint32)count) == FALSE)
            {
                uint8 errorResponse[8] = {0U};

                errorResponse[0] = 0xFEU; /* ERR */
                errorResponse[1] = 0x22U; /* ERR_OUT_OF_RANGE */

                Xcp_Can_SendResponse(
                    errorResponse,
                    8U
                );

                break;
            }

            src = (const volatile uint8 *)g_BmsXcpMta;

            response[0] = 0xFFU;

            for (i = 0U; i < count; i++)
            {
                response[i + 1U] = src[i];
            }

            g_BmsXcpMta += count;
            g_BmsXcpUploadCount++;

            Xcp_Can_SendResponse(
                response,
                8U
            );

            break;
        }

        case 0xF4U:
        {
            /*
             * XCP SHORT_UPLOAD
             */
            uint8 response[8] = {0U};
            uint8 count;
            uint8 i;
            uint8 addressExt;
            uint32 address;
            const volatile uint8 *src;

            if (dlc < 8U)
            {
                break;
            }

            count = data[1];
            addressExt = data[3];

            address =
                Xcp_ReadUint32LE(&data[4]);

            /*
             * Classical CAN:
             * Byte0 of response is RES (0xFF),
             * so maximum payload is 7 bytes.
             */
            if ((count == 0U) || (count > 7U))
            {
                break;
            }

            /*
             * For now we only support address extension 0.
             */
            if (addressExt != 0U)
            {
                break;
            }

            if (Xcp_IsValidRamRange(
                    address,
                    (uint32)count) == FALSE)
            {
                uint8 errorResponse[8] = {0U};

                errorResponse[0] = 0xFEU; /* ERR */
                errorResponse[1] = 0x22U; /* ERR_OUT_OF_RANGE */

                Xcp_Can_SendResponse(
                    errorResponse,
                    8U
                );

                break;
            }

            src = (const volatile uint8 *)address;

            response[0] = 0xFFU;

            for (i = 0U; i < count; i++)
            {
                response[i + 1U] = src[i];
            }

            Xcp_Can_SendResponse(
                response,
                8U
            );

            break;
        }

        case 0xF0U:
        {
            /*
             * XCP DOWNLOAD
             */
            uint8 response[8] = {0U};
            uint8 count;
            uint8 i;
            volatile uint8 *dst;

            if (dlc < 2U)
            {
                break;
            }

            count = data[1];

            g_BmsXcpDownloadEnteredCount++;

            g_BmsXcpDownloadAddress =
                g_BmsXcpMta;

            g_BmsXcpDownloadCalAddress =
                (uint32)&g_BmsXcpTestCalibration;

            g_BmsXcpDownloadLength =
                count;

            g_BmsXcpDownloadWritable =
                Xcp_IsWritableRange(
                    g_BmsXcpMta,
                    (uint32)count
                );

            /*
             * Classical CAN:
             * Byte0 = command
             * Byte1 = count
             * Byte2... = payload
             *
             * Max writable data in one frame = 6 bytes.
             */
            if ((count == 0U) ||
                (count > 6U) ||
                ((uint8)(count + 2U) > dlc))
            {
                break;
            }

            if (g_BmsXcpDownloadWritable == FALSE)
            {
                response[0] = 0xFEU;
                response[1] = 0x22U;

                Xcp_Can_SendResponse(
                    response,
                    8U
                );

                break;
            }

            dst = (volatile uint8 *)g_BmsXcpMta;

            for (i = 0U; i < count; i++)
            {
                dst[i] = data[i + 2U];
            }

            g_BmsXcpMta += count;
            g_BmsXcpDownloadCount++;

            response[0] = 0xFFU;

            Xcp_Can_SendResponse(
                response,
                8U
            );

            break;
        }

        default:
            /*
             * Unsupported XCP command for now.
             */
            break;
    }
}
