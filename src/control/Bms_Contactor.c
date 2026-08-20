#include "Bms_Contactor.h"
#include "Bms_Contactor_Cfg.h"
#include "Fault_Manager.h"


static volatile Bms_ContactorStateType g_State;

static volatile boolean g_CloseRequest;
static volatile boolean g_OpenRequest;

static volatile float g_PackVoltage;
static volatile float g_BusVoltage;

static volatile uint32 g_StateTimerMs;

static Bms_ContactorOutputType g_Output;


/* ---------------------------------------------------------- */

static void Bms_Contactor_AllOff(void)
{
    g_Output.negative = FALSE;
    g_Output.positive = FALSE;
    g_Output.precharge = FALSE;
}


/* ---------------------------------------------------------- */

static void Bms_Contactor_EnterState(
        Bms_ContactorStateType newState)
{
    g_State = newState;

    g_StateTimerMs = 0U;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_Init(void)
{
    g_State = BMS_CONTACTOR_OFF;

    g_CloseRequest = FALSE;
    g_OpenRequest = FALSE;

    g_PackVoltage = 0.0F;
    g_BusVoltage = 0.0F;

    g_StateTimerMs = 0U;

    Bms_Contactor_AllOff();
}


/* ---------------------------------------------------------- */

void Bms_Contactor_RequestClose(void)
{
    g_CloseRequest = TRUE;
    g_OpenRequest = FALSE;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_RequestOpen(void)
{
    g_OpenRequest = TRUE;
    g_CloseRequest = FALSE;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_MainFunction(void)
{
    g_StateTimerMs += BMS_CONTACTOR_TASK_PERIOD_MS;


    /*
     * Highest priority:
     * critical fault
     */
    if (FaultManager_HasCriticalFault() == TRUE)
    {
        Bms_Contactor_AllOff();

        g_CloseRequest = FALSE;
        g_OpenRequest = FALSE;

        Bms_Contactor_EnterState(
                BMS_CONTACTOR_FAULT);

        return;
    }


    /*
     * Open request always has priority
     */
    if (g_OpenRequest == TRUE)
    {
        Bms_Contactor_AllOff();

        g_OpenRequest = FALSE;

        Bms_Contactor_EnterState(
                BMS_CONTACTOR_OFF);

        return;
    }


    switch (g_State)
    {

        /* =============================================== */

        case BMS_CONTACTOR_OFF:

            Bms_Contactor_AllOff();

            if (g_CloseRequest == TRUE)
            {
                g_CloseRequest = FALSE;

                /*
                 * Close negative contactor first
                 */
                g_Output.negative = TRUE;

                Bms_Contactor_EnterState(
                        BMS_CONTACTOR_NEG_ON);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_NEG_ON:

            g_Output.negative = TRUE;

            if (g_StateTimerMs >=
                    BMS_CONTACTOR_NEG_DELAY_MS)
            {
                /*
                 * Start precharge
                 */
                g_Output.precharge = TRUE;

                Bms_Contactor_EnterState(
                        BMS_CONTACTOR_PRECHARGE);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_PRECHARGE:

            g_Output.negative = TRUE;
            g_Output.precharge = TRUE;
            g_Output.positive = FALSE;


            if ((g_PackVoltage > 0.0F) &&
                (g_BusVoltage >=
                 (g_PackVoltage *
                  BMS_PRECHARGE_COMPLETE_RATIO)))
            {
                /*
                 * Precharge success: close positive contactor
                 */
                g_Output.positive = TRUE;

                Bms_Contactor_EnterState(
                        BMS_CONTACTOR_POS_ON);
            }
            else if (g_StateTimerMs >=
                     BMS_PRECHARGE_TIMEOUT_MS)
            {
                FaultManager_Set(
                        FAULT_PRECHARGE_TIMEOUT);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_POS_ON:

            g_Output.negative = TRUE;
            g_Output.positive = TRUE;
            g_Output.precharge = TRUE;


            if (g_StateTimerMs >=
                    BMS_CONTACTOR_POS_DELAY_MS)
            {
                /*
                 * Positive contactor closed.
                 *
                 * Remove precharge resistor.
                 */
                g_Output.precharge = FALSE;

                Bms_Contactor_EnterState(
                        BMS_CONTACTOR_RUN);
            }

            break;


        /* =============================================== */

        case BMS_CONTACTOR_RUN:

            g_Output.negative = TRUE;
            g_Output.positive = TRUE;
            g_Output.precharge = FALSE;

            break;


        /* =============================================== */

        case BMS_CONTACTOR_OPENING:

            Bms_Contactor_AllOff();

            Bms_Contactor_EnterState(
                    BMS_CONTACTOR_OFF);

            break;


        /* =============================================== */

        case BMS_CONTACTOR_FAULT:

            Bms_Contactor_AllOff();

            if (FaultManager_HasCriticalFault() == FALSE)
            {
                Bms_Contactor_EnterState(
                        BMS_CONTACTOR_OFF);
            }

            break;


        /* =============================================== */

        default:

            Bms_Contactor_AllOff();

            Bms_Contactor_EnterState(
                    BMS_CONTACTOR_FAULT);

            break;
    }
}


/* ---------------------------------------------------------- */

Bms_ContactorStateType Bms_Contactor_GetState(void)
{
    return g_State;
}


/* ---------------------------------------------------------- */

Bms_ContactorOutputType Bms_Contactor_GetOutputs(void)
{
    return g_Output;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_SetPackVoltage(float voltage)
{
    g_PackVoltage = voltage;
}


/* ---------------------------------------------------------- */

void Bms_Contactor_SetBusVoltage(float voltage)
{
    g_BusVoltage = voltage;
}