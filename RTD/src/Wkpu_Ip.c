/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : Emios Siul2 Wkpu LpCmp
*   Dependencies         : none
*
*   Autosar Version      : 4.9.0
*   Autosar Revision     : ASR_REL_4_9_REV_0000
*   Autosar Conf.Variant :
*   SW Version           : 7.0.1
*   Build Version        : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
*   Copyright 2020 - 2026 NXP
*
*   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
 *     @file
 *
 *     @addtogroup wkpu_icu_ip WKPU IPL
 *     @{
 */

#ifdef __cplusplus
extern "C"{
#endif


/*==================================================================================================
*                                          INCLUDE FILES
*  1) system and project includes
*  2) needed interfaces from external units
*  3) internal and external interfaces from this unit
==================================================================================================*/
#include "Std_Types.h"
#include "Wkpu_Ip.h"
#include "OsIf.h"

#if (STD_ON == WKPU_IP_USED)
    #if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
        #include "Devassert.h"
    #endif


#endif /* WKPU_IP_USED */

#include "SchM_Icu.h"
/*==================================================================================================
*                                         SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define WKPU_IP_VENDOR_ID_C                    43
#define WKPU_IP_AR_RELEASE_MAJOR_VERSION_C     4
#define WKPU_IP_AR_RELEASE_MINOR_VERSION_C     9
#define WKPU_IP_AR_RELEASE_REVISION_VERSION_C  0
#define WKPU_IP_SW_MAJOR_VERSION_C             7
#define WKPU_IP_SW_MINOR_VERSION_C             0
#define WKPU_IP_SW_PATCH_VERSION_C             1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    /* Check if header file and Std_Types.h file are of the same Autosar version */
    #if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION_C != STD_AR_RELEASE_MAJOR_VERSION) || \
         (WKPU_IP_AR_RELEASE_MINOR_VERSION_C != STD_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Wkpu_Ip.c and Std_Types.h are different"
    #endif

    /* Check if source file and OsIf.h are of the same AUTOSAR version */
    #if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION_C != OSIF_AR_RELEASE_MAJOR_VERSION) || \
         (WKPU_IP_AR_RELEASE_MINOR_VERSION_C != OSIF_AR_RELEASE_MINOR_VERSION))
        #error "AUTOSAR version numbers of Wkpu_Ip.c and OsIf.h are different."
    #endif

    #if (STD_ON == WKPU_IP_USED)
        #if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
            /* Check if this header file and Devassert.h file are of the same Autosar version */
            #if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION_C != DEVASSERT_AR_RELEASE_MAJOR_VERSION) || \
                (WKPU_IP_AR_RELEASE_MINOR_VERSION_C != DEVASSERT_AR_RELEASE_MINOR_VERSION))
                #error "AutoSar Version Numbers of Wkpu_Ip.c and Devassert.h are different"
            #endif
        #endif
    #endif /* WKPU_IP_USED */

    /* Check if this header file and SchM_Icu.h file are of the same Autosar version */
    #if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION_C != SCHM_ICU_AR_RELEASE_MAJOR_VERSION) || \
        (WKPU_IP_AR_RELEASE_MINOR_VERSION_C != SCHM_ICU_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Wkpu_Ip.c and SchM_Icu.h are different"
    #endif
#endif

/* Check if source file and ICU header file are of the same vendor */
#if (WKPU_IP_VENDOR_ID_C != WKPU_IP_VENDOR_ID)
    #error "Wkpu_Ip.c and Wkpu_Ip.h have different vendor IDs"
#endif
/* Check if source file and ICU header file are of the same AutoSar version */
#if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION_C  != WKPU_IP_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_AR_RELEASE_MINOR_VERSION_C  != WKPU_IP_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_AR_RELEASE_REVISION_VERSION_C   != WKPU_IP_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip.c and Wkpu_Ip.h are different"
#endif
/* Check if source file and ICU header file are of the same Software version */
#if ((WKPU_IP_SW_MAJOR_VERSION_C  != WKPU_IP_SW_MAJOR_VERSION) || \
     (WKPU_IP_SW_MINOR_VERSION_C  != WKPU_IP_SW_MINOR_VERSION) || \
     (WKPU_IP_SW_PATCH_VERSION_C  != WKPU_IP_SW_PATCH_VERSION))
#error "Software Version Numbers of Wkpu_Ip.c and Wkpu_Ip.h are different"
#endif
/*==================================================================================================
*                           LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/


/*==================================================================================================
*                                        LOCAL CONSTANTS
==================================================================================================*/
/**
 * @brief Bitmask used to constrain core shift values within the supported range.
 *
 * This macro defines a mask value of 0x1F, which corresponds to the lower 5 bits of a 32-bit word.
 * It is used to ensure that bitwise operations involving core indices remain within the valid range,
 * supporting up to 32 distinct shift positions (0 to 31).
 */
#define WKPU_IP_MAX_CORE_SHIFT_MASK    0x1F
/*==================================================================================================
*                                        LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/
#if (STD_ON == WKPU_IP_USED)
#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_START_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#else
    #define ICU_START_SEC_VAR_CLEARED_UNSPECIFIED
#endif
#include "Icu_MemMap.h"

/* Table of initialized WKPU channels */
Wkpu_Ip_State Wkpu_Ip_u32ChState[WKPU_IP_NUM_OF_CHANNELS_USED];

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_STOP_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#else
    #define ICU_STOP_SEC_VAR_CLEARED_UNSPECIFIED
#endif
#include "Icu_MemMap.h"

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_START_SEC_VAR_INIT_8_NO_CACHEABLE
#else
    #define ICU_START_SEC_VAR_INIT_8
#endif
#include "Icu_MemMap.h"

/* This array stores the positions in the Wkpu_Ip_u32ChState array of the configured Wkpu channels. */
uint8 Wkpu_Ip_IndexInChState[WKPU_IP_NUM_OF_CHANNELS] = WKPU_IP_INITIAL_INDEX_OF_CHANNELS;

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_STOP_SEC_VAR_INIT_8_NO_CACHEABLE
#else
    #define ICU_STOP_SEC_VAR_INIT_8
#endif
#include "Icu_MemMap.h"

#define ICU_START_SEC_CONST_UNSPECIFIED
#include "Icu_MemMap.h"
/* Table of base addresses for WKPU instances. */
WKPU_Type * const Wkpu_Ip_apxBase[] = IP_WKPU_BASE_PTRS;
#define ICU_STOP_SEC_CONST_UNSPECIFIED
#include "Icu_MemMap.h"


/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
/**
 * @brief        Enables or disables wakeup request for WKPU channel
 * @details      This function enables or disables wakeup request by setting/clearing
 *               the corresponding bit in WRER register
 *
 * @param[in]    base        Pointer to the WKPU peripheral base address
 * @param[in]    hwChannel   WKPU Hardware channel
 * @param[in]    enable      Enable/disable flag:
 *                           - true: Enable wakeup request
 *                           - false: Disable wakeup request
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_WakeupRequest(WKPU_Type * const base, uint8 hwChannel, boolean enable);

/**
 * @brief        Enables or disables interrupt request for WKPU channel
 * @details      This function enables or disables interrupt request by setting/clearing
 *               the corresponding bit in IRER register
 *
 * @param[in]    base        Pointer to the WKPU peripheral base address
 * @param[in]    hwChannel   WKPU Hardware channel
 * @param[in]    enable      Enable/disable flag:
 *                           - true: Enable interrupt request
 *                           - false: Disable interrupt request
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_InterruptRequest(WKPU_Type * const base, uint8 hwChannel, boolean enable);

/**
 * @brief Enables or disables rising edge event
 *
 * This function enables or disables rising edge event
 *
 * @param[in] base The WKPU peripheral base address
 * @param[in] channelMask The channel mask
 * @param[in] enable Enables or disables rising edge event
 */
static inline void Wkpu_Ip_EnableRisingEdge(WKPU_Type * const base, uint32 hwChannel, boolean enable);

/**
 * @brief        Enables or disables rising edge event for WKPU channel
 * @details      This function enables or disables rising edge event by setting/clearing
 *               the corresponding bit in WIREER register
 *
 * @param[in]    base        Pointer to the WKPU peripheral base address
 * @param[in]    hwChannel   WKPU Hardware channel
 * @param[in]    enable      Enable/disable flag:
 *                           - true: Enable rising edge event detection
 *                           - false: Disable rising edge event detection
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_EnableFallingEdge(WKPU_Type * const base, uint32 hwChannel, boolean enable);

/**
 * @brief        Enables or disables falling edge event for WKPU channel
 * @details      This function enables or disables falling edge event by setting/clearing
 *               the corresponding bit in WIFEER register
 *
 * @param[in]    base        Pointer to the WKPU peripheral base address
 * @param[in]    hwChannel   WKPU Hardware channel
 * @param[in]    enable      Enable/disable flag:
 *                           - true: Enable falling edge event detection
 *                           - false: Disable falling edge event detection
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_Filter(WKPU_Type * const base, uint8 hwChannel, boolean enable);

#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
/**
 * @brief        Gets NMI/Reset configuration lock status
 * @details      This function checks if NMI/Reset configuration is locked for the specified core
 *               by reading the NLOCK bit in NCR register. When locked, NMI configuration cannot
 *               be modified until next reset.
 *
 * @param[in]    base        Pointer to WKPU peripheral base address
 * @param[in]    coreNumber  NMI core number (0 to maximum supported cores)
 *
 * @return       boolean
 *               - TRUE: Configuration is locked (cannot be modified)
 *               - FALSE: Configuration is unlocked (can be modified)
 *
 */
static inline boolean Wkpu_Ip_IsNMIConfigLock(const WKPU_Type * base, uint8 coreNumber);

/**
 * @brief        Sets activation condition of WKPU NMI channel
 * @details      This function sets activation condition of WKPU NMI channel by configuring
 *               the edge event enable bits in NCR register. The function handles different edge types:
 *               - WKPU_IP_RISING_EDGE: Enables NREE bit, disables NFEE bit
 *               - WKPU_IP_FALLING_EDGE: Enables NFEE bit, disables NREE bit
 *               - Other cases: Enables both NREE and NFEE bits
 *
 * @param[in]    base        Pointer to WKPU peripheral base address
 * @param[in]    coreNumber  NMI core number (0 to maximum supported cores)
 * @param[in]    edgeEvent   Edge type for activation (Wkpu_Ip_EdgeType)
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_IsNMISetActivationCondition(WKPU_Type * base, uint8 coreNumber, Wkpu_Ip_EdgeType edgeEvent);

/**
 * @brief        Configures single NMI core with specified settings
 * @details      This function configures a single NMI core by setting up all NMI-related
 *               parameters in the appropriate NCR register. The configuration includes:
 *               - Destination source configuration (NDSS bits)
 *               - Wake-up request enable/disable (NWRE bits)
 *               - Glitch filter enable/disable (NFE bits)
 *               - Edge event activation condition (NREE/NFEE bits)
 *               - Configuration lock setting (NLOCK bits)
 *               Supports different core ranges with appropriate register sets.
 *
 * @param[in]    instance    WKPU instance number
 * @param[in]    config      Pointer to NMI configuration structure for one core
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_ConfigureSingleNMICore(uint8 instance, const Wkpu_Ip_NmiCfgType * config);
#endif /* STD_ON == WKPU_IP_NMI_API */

/*==================================================================================================
*                                        LOCAL FUNCTIONS
==================================================================================================*/
#define ICU_START_SEC_CODE
#include "Icu_MemMap.h"


static inline void Wkpu_Ip_WakeupRequest(WKPU_Type * const base, uint8 hwChannel, boolean enable)
{
    /* Enable wake-up request */
    if (enable)
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WRER |= ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WRER_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
    /* Disable wake-up request */
    else
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WRER &= ~((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WRER_64 &= ~((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
}

static inline void Wkpu_Ip_InterruptRequest(WKPU_Type * const base, uint8 hwChannel, boolean enable)
{
    /* Enable interrupt request */
    if (enable)
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->IRER |= ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->IRER_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
    /* Disable interrupt request */
    else
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->IRER &= ~((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->IRER_64 &= ~((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
}

static inline void Wkpu_Ip_EnableRisingEdge(WKPU_Type * const base, uint32 hwChannel, boolean enable)
{
    /* Enables Wakeup/Interrupt Rising edge event enable Register */
    if (enable)
    {
        if(32U > hwChannel)
        {
            base->WIREER |= ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            base->WIREER_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
    /* Disables Wakeup/Interrupt Rising edge event enable Register */
    else
    {
        if(32U > hwChannel)
        {
            base->WIREER &= ~((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            base->WIREER_64 &= ~((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
}

static inline void Wkpu_Ip_EnableFallingEdge(WKPU_Type * const base, uint32 hwChannel, boolean enable)
{
    /* Enables Wakeup/Interrupt Falling edge event enable Register */
    if (enable)
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WIFEER |= ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WIFEER_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
    /* Disables Wakeup/Interrupt Falling edge event enable Register */
    else
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WIFEER &= ~((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WIFEER_64 &= ~((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
}

static inline void Wkpu_Ip_Filter(WKPU_Type * const base, uint8 hwChannel, boolean enable)
{
    /* Enables Wakeup/Interrupt Filter Enable Register */
    if (enable)
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WIFER |= ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WIFER_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
    /* Disables Wakeup/Interrupt Filter Enable Register */
    else
    {
        if(32U > hwChannel)
        {
            /* Channels 0-31: Set bit in WRER register */
            base->WIFER &= ~((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Set bit in WRER_64 register */
            base->WIFER_64 &= ~((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif
    }
}

#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
static inline boolean Wkpu_Ip_IsNMIConfigLock(const WKPU_Type * base, uint8 coreNumber)
{
    boolean result = FALSE;
    uint32 coreShift;

        /* Cores 0-3: Check NLOCK bits in NCR register */
        coreShift = (uint32)((uint32)coreNumber * (uint32)WKPU_IP_CORE_OFFSET_SIZE) & (uint32)WKPU_IP_MAX_CORE_SHIFT_MASK;
        result = (((base->NCR & (WKPU_NCR_NLOCK0_MASK >> coreShift)) != 0U) ? TRUE : FALSE);

    return result;
}

static inline void Wkpu_Ip_IsNMISetActivationCondition(WKPU_Type * base, uint8 coreNumber, Wkpu_Ip_EdgeType edgeEvent)
{
    uint32 coreShift;

        /* Cores 0-3: Configure edge events in NCR register */
        coreShift = (uint32)((uint32)coreNumber * (uint32)WKPU_IP_CORE_OFFSET_SIZE) & (uint32)WKPU_IP_MAX_CORE_SHIFT_MASK;

        /* Configure edge events based on requested type */
        if (WKPU_IP_RISING_EDGE == edgeEvent)
        {
            /* Rising edge only: Enable NREE, disable NFEE */
    #ifdef ERR_IPV_WKPU_E050394
            base->NCR |= WKPU_NCR_NFEE0_MASK >> coreShift;
    #endif
            base->NCR |= WKPU_NCR_NREE0_MASK >> coreShift;
            base->NCR &= ~(WKPU_NCR_NFEE0_MASK >> coreShift);
        }
        else if (WKPU_IP_FALLING_EDGE == edgeEvent)
        {
            /* Falling edge only: Enable NFEE, disable NREE */
            base->NCR &= ~(WKPU_NCR_NREE0_MASK >> coreShift);
            base->NCR |= WKPU_NCR_NFEE0_MASK >> coreShift;
        }
        else
        {
            /* Both edges or default case: Enable both NREE and NFEE */
            base->NCR |= WKPU_NCR_NFEE0_MASK >> coreShift;
            base->NCR |= WKPU_NCR_NREE0_MASK >> coreShift;
        }
}

static inline void Wkpu_Ip_ConfigureSingleNMICore(uint8 instance, const Wkpu_Ip_NmiCfgType * config)
{
    uint8 coreNumber = (uint8)config->core;
    uint32 coreShift;
    WKPU_Type * base = Wkpu_Ip_apxBase[instance];

        /* Cores 0-3: Configure NMI settings in NCR register */
        coreShift = (uint32)((uint32)coreNumber * (uint32)WKPU_IP_CORE_OFFSET_SIZE) & (uint32)WKPU_IP_MAX_CORE_SHIFT_MASK;

        /* Configure destination source */
        base->NCR &= ~(WKPU_NCR_NDSS0_MASK >> coreShift);
        base->NCR |= WKPU_NCR_NDSS0((uint8)config->destination) >> coreShift;

        /* Configure wake-up request enable/disable */
        base->NCR &= ~(WKPU_NCR_NWRE0_MASK >> coreShift);
        base->NCR |= WKPU_NCR_NWRE0(config->wkpReqEn ? 1UL : 0UL) >> coreShift;

    #ifndef WKPU_IP_SUPPORT_FILTER_ON_EACH_CORE
        /* Filter configuration limited to Core 0 only */
        if ( 0UL == coreNumber)
    #endif
        {
           /* Configure glitch filter enable/disable */
            base->NCR &= ~(WKPU_NCR_NFE0_MASK >> coreShift);
            base->NCR |= WKPU_NCR_NFE0(config->filterEn ? 1UL : 0UL) >> coreShift;
        }

        /* Configure edge events (rising/falling edge detection) */
        Wkpu_Ip_IsNMISetActivationCondition(base, coreNumber, config->edgeEvent);
        /* Configure lock bit (prevents further configuration changes) */
        base->NCR |= WKPU_NCR_NLOCK0(config->lockEn ? 1UL : 0UL) >> coreShift;
}
#endif /* STD_ON == WKPU_IP_NMI_API */

/*==================================================================================================
*                                        GLOBAL FUNCTIONS
==================================================================================================*/

/** @implements   Wkpu_Ip_EnableInterrupt_Activity */
void Wkpu_Ip_EnableInterrupt(uint8 instance, uint8 hwChannel)
{
    WKPU_Type * base;

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

    base = Wkpu_Ip_apxBase[instance];

    SchM_Enter_Icu_ICU_EXCLUSIVE_AREA_57();

    /* Clear any pending interrupt status flag by writing 1 to WISR register */
    if(32U > hwChannel)
    {
        /* Channels 0-31: Set bit in WRER register */
        base->WISR |= ((uint32)1UL << (uint32)hwChannel);
    }
#ifdef WKPU_IP_64_CH_USED
    else
    {
        /* Channels 32-63: Set bit in WRER_64 register */
        base->WISR_64 |= ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
    }
#endif

    /* Enable interrupt request for the channel */
    Wkpu_Ip_InterruptRequest(base, hwChannel, TRUE);
    /* Enable wakeup request for the channel */
    Wkpu_Ip_WakeupRequest(base, hwChannel, TRUE);

    SchM_Exit_Icu_ICU_EXCLUSIVE_AREA_57();
}

/** @implements   Wkpu_Ip_DisableInterrupt_Activity */
void Wkpu_Ip_DisableInterrupt(uint8 instance, uint8 hwChannel)
{
    WKPU_Type * base;

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

    base = Wkpu_Ip_apxBase[instance];

    SchM_Enter_Icu_ICU_EXCLUSIVE_AREA_58();

    /* Disable interrupt request for the channel */
    Wkpu_Ip_InterruptRequest(base, hwChannel, FALSE);
    /* Disable wakeup request for the channel */
    Wkpu_Ip_WakeupRequest(base, hwChannel, FALSE);

    SchM_Exit_Icu_ICU_EXCLUSIVE_AREA_58();
}

/** @implements   Wkpu_Ip_Init_Activity */
Wkpu_Ip_StatusType Wkpu_Ip_Init (uint8 instance, const Wkpu_Ip_IrqConfigType* userConfig)
{
    WKPU_Type * base;
    uint8 hwChannel;
    uint8 index;
#ifdef WKPU_IP_STANDBY_WAKEUP_SUPPORT
#if (WKPU_IP_STANDBY_WAKEUP_SUPPORT == STD_ON)
    uint32 u32regWkpuWISR = (uint32)0U;
#endif
#endif

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(userConfig != NULL_PTR);
#endif

    /* Get base address for the specified WKPU instance */
    base = Wkpu_Ip_apxBase[instance];


    for(index=0; index < userConfig->numChannels; index++)
    {
         /* Get hardware channel number for current configuration */
        hwChannel = (*userConfig->pChannelsConfig)[index].hwChannel;
        /* Save in state structure the callback information */
        Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].callback = (*userConfig->pChannelsConfig)[index].callback;
        Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].callbackParam = (*userConfig->pChannelsConfig)[index].callbackParam;
        Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].WkpuChannelNotification = (*userConfig->pChannelsConfig)[index].WkpuChannelNotification;

#ifdef WKPU_IP_STANDBY_WAKEUP_SUPPORT
#if (WKPU_IP_STANDBY_WAKEUP_SUPPORT == STD_ON)
        /* Check if this channel caused wakeup from standby mode */
        if(32U > hwChannel)
        {
            u32regWkpuWISR = base->WISR & ((uint32)1UL << (uint32)hwChannel);
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            u32regWkpuWISR = base->WISR_64 & ((uint32)1UL << ((uint32)hwChannel - (uint32)32U));
        }
#endif

#endif /* WKPU_IP_STANDBY_WAKEUP_SUPPORT == STD_ON */
#endif /* WKPU_IP_STANDBY_WAKEUP_SUPPORT */

#ifdef WKPU_IP_STANDBY_WAKEUP_SUPPORT
#if (WKPU_IP_STANDBY_WAKEUP_SUPPORT == STD_ON)
        /* Only disable interrupt if channel did not cause standby wakeup */
        if (0U == u32regWkpuWISR)
        {
#endif
#endif
            /* Disable interrupt request for this channel */
            Wkpu_Ip_DisableInterrupt(instance, hwChannel);
#ifdef WKPU_IP_STANDBY_WAKEUP_SUPPORT
#if (WKPU_IP_STANDBY_WAKEUP_SUPPORT == STD_ON)
        }
#endif
#endif

        /* Set Wakeup/Interrupt Filter Enable Register */
        Wkpu_Ip_Filter(base, hwChannel, (*userConfig->pChannelsConfig)[index].filterEn);

        /* Set edge events enable registers */
        Wkpu_Ip_SetActivationCondition(instance, hwChannel, (*userConfig->pChannelsConfig)[index].edgeEvent);
        if(WKPU_IP_NUM_OF_CHANNELS > hwChannel)
        {
            Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].chInit = TRUE;
        }
    }

    return WKPU_IP_SUCCESS;
}

/** @implements   Wkpu_Ip_DeInit_Activity */
Wkpu_Ip_StatusType Wkpu_Ip_DeInit(uint8 instance)
{
    uint32 u32ChannelMask;
    uint8 index;

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
#endif
    for(index=0U; index < WKPU_IP_NUM_OF_CHANNELS; index++)
    {
        /* Channels 0-31: Use standard registers */
        if((uint8)32U > index)
        {
            u32ChannelMask = (uint32)1U << (uint32)(index);
            /* Disable IRQ Interrupt */
            Wkpu_Ip_DisableInterrupt(instance, index);
            /* Clear Wakeup/Interrupt Filter Enable Register  */
            Wkpu_Ip_apxBase[instance]->WIFER &= ~u32ChannelMask;
            /* Clear edge event enable registers */
            Wkpu_Ip_apxBase[instance]->WIREER &= ~u32ChannelMask;
            Wkpu_Ip_apxBase[instance]->WIFEER &= ~u32ChannelMask;
            /* Clear Interrupt Filter Enable Register */
            Wkpu_Ip_apxBase[instance]->WIFER &= ~u32ChannelMask;
            /* Write 1 to wakeup/interrupt status flag register to clear the flag. */
            Wkpu_Ip_apxBase[instance]->WISR  = u32ChannelMask;
        }
#ifdef WKPU_IP_64_CH_USED
        else
        {
            /* Channels 32-63: Use mid-range registers */
            u32ChannelMask = (uint32)1U << ((uint32)index - (uint32)32U);
            /* Disable IRQ Interrupt */
            Wkpu_Ip_DisableInterrupt(instance, index);
            /* Clear Wakeup/Interrupt Filter Enable Register  */
            Wkpu_Ip_apxBase[instance]->WIFER_64 &= ~u32ChannelMask;
            /* Clear edge event enable registers */
            Wkpu_Ip_apxBase[instance]->WIREER_64 &= ~u32ChannelMask;
            Wkpu_Ip_apxBase[instance]->WIFEER_64 &= ~u32ChannelMask;
            /* Clear Interrupt Filter Enable Register */
            Wkpu_Ip_apxBase[instance]->WIFER_64 &= ~u32ChannelMask;
			/* Write 1 to wakeup/interrupt status flag register to clear the flag. */
            Wkpu_Ip_apxBase[instance]->WISR_64  = u32ChannelMask;
        }
#endif
    }
    return WKPU_IP_SUCCESS;
}

/** @implements   Wkpu_Ip_SetActivationCondition_Activity */
void Wkpu_Ip_SetActivationCondition(uint8 instance, uint8 hwChannel, Wkpu_Ip_EdgeType edge)
{
    WKPU_Type * base = Wkpu_Ip_apxBase[instance];

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

    /* Configure edge detection based on requested activation condition */
    switch (edge)
    {
        case WKPU_IP_RISING_EDGE:
        {
            /* Enable rising edge detection, disable falling edge */
            Wkpu_Ip_EnableRisingEdge(base, hwChannel, TRUE);
            Wkpu_Ip_EnableFallingEdge(base, hwChannel, FALSE);
            break;
        }
        case WKPU_IP_FALLING_EDGE:
        {
            /* Enable falling edge detection, disable rising edge */
            Wkpu_Ip_EnableRisingEdge(base, hwChannel, FALSE);
            Wkpu_Ip_EnableFallingEdge(base, hwChannel, TRUE);
            break;
        }
        case WKPU_IP_NONE_EDGE:
        {
            /* Disable both rising and falling edge detection */
            Wkpu_Ip_EnableRisingEdge(base, hwChannel, FALSE);
            Wkpu_Ip_EnableFallingEdge(base, hwChannel, FALSE);
            break;
        }
        case WKPU_IP_BOTH_EDGES:
        default:
        {
            /* Enable both rising and falling edge detection (default case) */
            Wkpu_Ip_EnableRisingEdge(base, hwChannel, TRUE);
            Wkpu_Ip_EnableFallingEdge(base, hwChannel, TRUE);
            break;
        }
    }
}

/** @implements   Wkpu_Ip_GetInputState_Activity */
boolean Wkpu_Ip_GetInputState(uint8 instance, uint8 hwChannel)
{
    boolean bstate = FALSE;
    uint32 u32regWkpuWISR = 0;
    uint32 u32regWkpuIRER = 0;
    uint32 channelMask;

    WKPU_Type * base;

#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

    base = Wkpu_Ip_apxBase[instance];

    if((uint8)32U > hwChannel)
    {
        /* Channels 0-31: Use standard registers */
        channelMask = 1UL << hwChannel;
        u32regWkpuWISR = base->WISR & channelMask;
        u32regWkpuIRER = base->IRER & channelMask;

        /* Channel is active if status flag is set and interrupt is disabled */
        if ((0x0U != u32regWkpuWISR) && (0x0U == u32regWkpuIRER))
        {
            /* Clear the status flag and return active state */
            base->WISR = channelMask;
            bstate = TRUE;
        }
    }
#ifdef WKPU_IP_64_CH_USED
    else
    {
        /* Channels 32-63: Use mid-range registers */
        channelMask = 1UL << (hwChannel - 32U);
        u32regWkpuWISR = base->WISR_64 & channelMask;
        u32regWkpuIRER = base->IRER_64 & channelMask;
        if ((0x0U != u32regWkpuWISR) && (0x0U == u32regWkpuIRER))
        {
            base->WISR_64 = channelMask;
            bstate = TRUE;
        }
    }
#endif
    return bstate;
}

/** @implements Wkpu_Ip_EnableNotification_Activity */
void Wkpu_Ip_EnableNotification(uint8 hwChannel)
{
#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

/* Set notification enable to TRUE, notification function will be called in interrupt function when this variable is set */
    Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].notificationEnable = TRUE;
}

/** @implements Wkpu_Ip_DisableNotification_Activity */
void Wkpu_Ip_DisableNotification(uint8 hwChannel)
{
#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(hwChannel < WKPU_IP_NUM_OF_CHANNELS);
#endif

/* Set notification enable to FALSE, notification function will not be called in interrupt function when this variable is cleared */
    Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].notificationEnable = FALSE;
}

#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
/** @implements   Wkpu_Ip_InitNMI_Activity */
Wkpu_Ip_StatusType Wkpu_Ip_InitNMI(uint8 instance, const Wkpu_Ip_IrqConfigType* userConfig)
{
#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
    DevAssert(userConfig->pNMIChannelsConfig != NULL_PTR);
#endif

    uint8 coreId;
    Wkpu_Ip_StatusType retVal = WKPU_IP_SUCCESS;
    const Wkpu_Ip_NmiCfgType * userConfigNMI = (*userConfig->pNMIChannelsConfig);


    retVal = Wkpu_Ip_DeinitNMI(instance);

    if(WKPU_IP_SUCCESS == retVal)
    {
        for (coreId = 0U; coreId < WKPU_IP_NMI_CORE_CNT; coreId++)
        {
            /* Configure single NMI core with all required parameters */
            Wkpu_Ip_ConfigureSingleNMICore(instance, &userConfigNMI[coreId]);
        }
    }
    return retVal;
}

/** @implements   Wkpu_Ip_DeinitNMI_Activity */
Wkpu_Ip_StatusType Wkpu_Ip_DeinitNMI(uint8 instance)
{
#if(WKPU_IP_DEV_ERROR_DETECT == STD_ON)
    DevAssert(instance < WKPU_INSTANCE_COUNT);
#endif
    uint8 i;
    uint32 coreShift = 0U;
    Wkpu_Ip_StatusType retVal = WKPU_IP_SUCCESS;
    uint8 u8coreNumber = 0;
    static const Wkpu_Ip_CoreType coreNumber[WKPU_IP_NMI_NUM_CORES] = WKPU_IP_CORE_ARRAY;

    WKPU_Type * base = Wkpu_Ip_apxBase[instance];

    for (i = 0U; i < WKPU_IP_NMI_NUM_CORES; i++)
    {
        u8coreNumber = (uint8)coreNumber[i];
        /* Check if NMI configuration is locked for this core */
        if (Wkpu_Ip_IsNMIConfigLock(base, u8coreNumber) == FALSE)
        {
                /* Cores 0-3: Reset NMI configuration in NCR register */
                coreShift = (uint32)((uint32)u8coreNumber * (uint32)WKPU_IP_CORE_OFFSET_SIZE) & (uint32)WKPU_IP_MAX_CORE_SHIFT_MASK;
                /* Clear status flag and overrun status flag */
                base->NSR = ((WKPU_NSR_NIF0_MASK | WKPU_NSR_NOVF0_MASK) >> coreShift);
                /* Clear edge events */
                base->NCR &= ~(WKPU_NCR_NREE0_MASK >> coreShift);
                base->NCR &= ~(WKPU_NCR_NFEE0_MASK >> coreShift);

            #ifndef WKPU_IP_SUPPORT_FILTER_ON_EACH_CORE
                /* Only set filter for Core 0 */
                if ((uint8)WKPU_CORE0 == u8coreNumber)
            #endif /* #ifndef WKPU_IP_SUPPORT_FILTER_ON_EACH_CORE */
                {
                    /* Disable glitch filter */
                    base->NCR &= ~(WKPU_NCR_NFE0_MASK >> coreShift);
                }

                /* Disable wake-up request */
                base->NCR &= ~(WKPU_NCR_NWRE0_MASK >> coreShift);

            #ifdef WKPU_IP_SUPPORT_NONE_REQUEST
                /* Configure destination source */
                base->NCR &= ~(WKPU_NCR_NDSS0_MASK >> coreShift);
                base->NCR |= WKPU_NCR_NDSS0((uint8)WKPU_IP_NMI_NONE) >> coreShift;
            #endif /* #ifndef WKPU_IP_SUPPORT_NONE_REQUEST */
        }
        else
        {
            retVal = WKPU_IP_ERROR;
            break;
        }
    }
    return retVal;
}
#endif /* STD_ON == WKPU_IP_NMI_API */

#define ICU_STOP_SEC_CODE
#include "Icu_MemMap.h"

#endif /* WKPU_IP_USED */

#ifdef __cplusplus
}
#endif

/** @} */

