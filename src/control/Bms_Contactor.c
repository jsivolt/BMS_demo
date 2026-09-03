#include "Bms_Contactor.h"
#include "Bms_Contactor_Cfg.h"
#include "Fault_Manager.h"

#include "Siul2_Dio_Ip.h"
#include "Siul2_Dio_Ip_Cfg.h"

/*
 * 1 = precharge completes on a timer instead of a bus voltage judgement.
 * Must be 0 before connecting real HV, which also requires a caller for
 * Bms_Contactor_SetBusVoltage(); nothing feeds it today.
 */
#define BMS_CONTACTOR_SIMULATION_MODE    (1U)

typedef struct
{
    Bms_ContactorStateType state;

    boolean closeRequest;
    boolean openRequest;

    float packVoltage;

    uint32 stateTimerMs;

    Bms_ContactorOutputType output;

} Bms_ContactorContextType;

static Bms_ContactorContextType
    g_Contactor[BMS_PACK_COUNT];

/*
 * Shared DC bus, common to all packs.
 */
static volatile float g_BusVoltage;


/* ---------------------------------------------------------- */

/*
 * Fault masks latched at the first entry into FAULT.
 * The live masks self-clear once the first valid measurement arrives.
 */
volatile uint32 g_DebugFaultPack1First = 0U;
volatile uint32 g_DebugFaultSystemFirst = 0U;
volatile uint32 g_DebugFaultPack1Sticky = 0U;
volatile uint32 g_DebugFaultSystemSticky = 0U;


/* ---------------------------------------------------------- */

static void Bms_Contactor_AllOff(Bms_PackIdType packId)
{
    g_Contactor[packId].output.negative = FALSE;
    g_Contactor[packId].output.positive = FALSE;
    g_Contactor[packId].output.precharge = FALSE;
}


/* ---------------------------------------------------------- */

static void Bms_Contactor_EnterState(
        Bms_PackIdType packId,
        Bms_ContactorStateType newState)
{
    g_Contactor[packId].state = newState;

    g_Contactor[packId].stateTimerMs = 0U;
}


/* ---------------------------------------------------------- */

static void Bms_Contactor_ProcessPack(Bms_PackIdType packId);


/* ---------------------------------------------------------- */

static void Bms_Contactor_WritePin(
        Siul2_Dio_Ip_GpioType * const base,
        Siul2_Dio_Ip_PinsChannelType pin,
        boolean active)
{
    Siul2_Dio_Ip_PinsLevelType level;

    if (active == TRUE)
    {
        level = (Siul2_Dio_Ip_PinsLevelType)
                BMS_CONTACTOR_OUTPUT_ACTIVE_LEVEL;
    }
    else
    {
        level = (Siul2_Dio_Ip_PinsLevelType)
                (1U - BMS_CONTACTOR_OUTPUT_ACTIVE_LEVEL);
    }

    Siul2_Dio_Ip_WritePin(base, pin, level);
}


/* ---------------------------------------------------------- */

static void Bms_Contactor_ApplyOutputs(Bms_PackIdType packId)
{
    const Bms_ContactorOutputType *out;

    /* Only Pack1 has relay hardware wired. */
    if (packId != BMS_PACK_1)
    {
        return;
    }

    out = &g_Contactor[packId].output;

    Bms_Contactor_WritePin(
            BMS_CONTACTOR_P1_NEG_PORT,
            BMS_CONTACTOR_P1_NEG_PIN,
            out->negative);

    Bms_Contactor_WritePin(
            BMS_CONTACTOR_P1_PRE_PORT,
            BMS_CONTACTOR_P1_PRE_PIN,
            out->precharge);

    Bms_Contactor_WritePin(
            BMS_CONTACTOR_P1_POS_PORT,
            BMS_CONTACTOR_P1_POS_PIN,
            out->positive);
}


/* ---------------------------------------------------------- */

void Bms_Contactor_Init(void)
{
    uint32 i;

    for (i = 0U; i < BMS_PACK_COUNT; i++)
    {
        g_Contactor[i].state = BMS_CONTACTOR_OFF;

        g_Contactor[i].closeRequest = FALSE;
        g_Contactor[i].openRequest = FALSE;

        g_Contactor[i].packVoltage = 0.0F;

        g_Contactor[i].stateTimerMs = 0U;

        Bms_Contactor_AllOff((Bms_PackIdType)i);

        Bms_Contactor_ApplyOutputs((Bms_PackIdType)i);
    }

    g_BusVoltage = 0.0F;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_RequestClose(Bms_PackIdType packId)
{
    g_Contactor[packId].closeRequest = TRUE;
    g_Contactor[packId].openRequest = FALSE;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_RequestOpen(Bms_PackIdType packId)
{
    g_Contactor[packId].openRequest = TRUE;
    g_Contactor[packId].closeRequest = FALSE;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_MainFunction(void)
{
    uint8 pack;

    for (pack = 0U;
         pack < BMS_PACK_COUNT;
         pack++)
    {
        Bms_Contactor_ProcessPack(
                (Bms_PackIdType)pack);

        /* ProcessPack has early returns, so drive the pins from here. */
        Bms_Contactor_ApplyOutputs(
                (Bms_PackIdType)pack);
    }
}


/* ---------------------------------------------------------- */

static void Bms_Contactor_ProcessPack(Bms_PackIdType packId)
{
    Bms_ContactorContextType *ctx;

    ctx = &g_Contactor[packId];

    if ((ctx->state == BMS_CONTACTOR_NEG_ON) ||
    (ctx->state == BMS_CONTACTOR_PRECHARGE) ||
    (ctx->state == BMS_CONTACTOR_POS_ON))
    {
        ctx->stateTimerMs += BMS_CONTACTOR_TASK_PERIOD_MS;
    }


    /*
     * Highest priority:
     * critical fault (this pack or system-wide)
     */
    if ((FaultManager_PackHasCriticalFault(
             (FaultPackIdType)packId) == TRUE) ||
        (FaultManager_SystemHasCriticalFault() == TRUE))
    {
        Bms_Contactor_AllOff(packId);

        ctx->closeRequest = FALSE;
        ctx->openRequest = FALSE;

        if (packId == BMS_PACK_1)
        {
            FaultMaskType packMask;
            FaultMaskType sysMask;

            packMask = FaultManager_GetPackFaults(FAULT_PACK_1);
            sysMask = FaultManager_GetSystemFaults();

            if ((g_DebugFaultPack1First == 0U) &&
                (g_DebugFaultSystemFirst == 0U))
            {
                g_DebugFaultPack1First = (uint32)packMask;
                g_DebugFaultSystemFirst = (uint32)sysMask;
            }

            g_DebugFaultPack1Sticky |= (uint32)packMask;
            g_DebugFaultSystemSticky |= (uint32)sysMask;
        }

        Bms_Contactor_EnterState(
                packId,
                BMS_CONTACTOR_FAULT);

        return;
    }


    /*
     * Open request always has priority
     */
    if (ctx->openRequest == TRUE)
    {
        Bms_Contactor_AllOff(packId);

        ctx->openRequest = FALSE;

        Bms_Contactor_EnterState(
                packId,
                BMS_CONTACTOR_OFF);

        return;
    }


    switch (ctx->state)
    {

        /* =============================================== */

        case BMS_CONTACTOR_OFF:

            Bms_Contactor_AllOff(packId);

            if (ctx->closeRequest == TRUE)
            {
                ctx->closeRequest = FALSE;

                /*
                 * Close negative contactor first
                 */
                ctx->output.negative = TRUE;

                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_NEG_ON);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_NEG_ON:

            ctx->output.negative = TRUE;

            if (ctx->stateTimerMs >=
                    BMS_CONTACTOR_NEG_DELAY_MS)
            {
                /*
                 * Start precharge
                 */
                ctx->output.precharge = TRUE;

                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_PRECHARGE);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_PRECHARGE:

            ctx->output.negative = TRUE;
            ctx->output.precharge = TRUE;
            ctx->output.positive = FALSE;


#if (BMS_CONTACTOR_SIMULATION_MODE == 1U)

            /*
             * Simulation mode:
             * no real precharge hardware / bus voltage available.
             *
             * Stay in PRECHARGE for a short period, then
             * simulate successful precharge.
             */
            if (ctx->stateTimerMs >=
                BMS_CONTACTOR_POS_DELAY_MS)
            {
                ctx->output.positive = TRUE;

                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_POS_ON);
            }

#else

            /*
             * Real hardware mode:
             * bus voltage must reach the configured fraction
             * of pack voltage before positive contactor closes.
             */
            if ((ctx->packVoltage > 0.0F) &&
                (g_BusVoltage >=
                 (ctx->packVoltage *
                  BMS_PRECHARGE_COMPLETE_RATIO)))
            {
                ctx->output.positive = TRUE;

                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_POS_ON);
            }
            else if (ctx->stateTimerMs >=
                     BMS_PRECHARGE_TIMEOUT_MS)
            {
                FaultManager_SetPack(
                        (FaultPackIdType)packId,
                        FAULT_PRECHARGE_TIMEOUT);
            }

#endif

            break;


        /* =============================================== */

        case BMS_CONTACTOR_POS_ON:

            ctx->output.negative = TRUE;
            ctx->output.positive = TRUE;
            ctx->output.precharge = TRUE;


            if (ctx->stateTimerMs >=
                    BMS_CONTACTOR_POS_DELAY_MS)
            {
                /*
                 * Positive contactor closed.
                 *
                 * Remove precharge resistor.
                 */
                ctx->output.precharge = FALSE;

                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_RUN);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_RUN:

            ctx->output.negative = TRUE;
            ctx->output.positive = TRUE;
            ctx->output.precharge = FALSE;

            break;


        /* =============================================== */

        case BMS_CONTACTOR_OPENING:

            Bms_Contactor_AllOff(packId);

            Bms_Contactor_EnterState(
                    packId,
                    BMS_CONTACTOR_OFF);

            break;


        /* =============================================== */

        case BMS_CONTACTOR_FAULT:

            Bms_Contactor_AllOff(packId);

            if ((FaultManager_PackHasCriticalFault(
                     (FaultPackIdType)packId) == FALSE) &&
                (FaultManager_SystemHasCriticalFault() == FALSE))
            {
                Bms_Contactor_EnterState(
                        packId,
                        BMS_CONTACTOR_OFF);
            }

            break;


        /* =============================================== */

        default:

            Bms_Contactor_AllOff(packId);

            Bms_Contactor_EnterState(
                    packId,
                    BMS_CONTACTOR_FAULT);

            break;
    }
}


/* ---------------------------------------------------------- */

Bms_ContactorStateType Bms_Contactor_GetState(Bms_PackIdType packId)
{
    return g_Contactor[packId].state;
}


/* ---------------------------------------------------------- */

Bms_ContactorOutputType Bms_Contactor_GetOutputs(Bms_PackIdType packId)
{
    return g_Contactor[packId].output;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_SetPackVoltage(
        Bms_PackIdType packId,
        float voltage)
{
    g_Contactor[packId].packVoltage = voltage;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_SetBusVoltage(float voltage)
{
    g_BusVoltage = voltage;
}