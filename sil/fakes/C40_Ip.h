/**
 *  @file   C40_Ip.h  (SIL fake)
 *  @brief  Host-side stand-in for the S32K3 C40 Data Flash IP driver.
 *
 *  Models the flash semantics that actually matter to Bms_Nvm:
 *    - erased state is 0xFF
 *    - programming can only clear bits (1 -> 0), never set them
 *    - erase granularity is a whole sector
 *    - program length must be a multiple of 8 bytes and stay in-sector
 *
 *  Those rules are enforced, not merely emulated, so the SIL build fails
 *  the same way the target would on a misaligned or out-of-bounds write.
 */

#ifndef C40_IP_H
#define C40_IP_H

#include "Std_Types.h"

typedef enum
{
    C40_IP_STATUS_SUCCESS = 0,
    C40_IP_STATUS_BUSY,
    C40_IP_STATUS_ERROR,
    C40_IP_STATUS_ERROR_INPUT_PARAM,
    C40_IP_STATUS_ERROR_TIMEOUT,
    C40_IP_STATUS_ERROR_PROGRAM_VERIFY,
    C40_IP_STATUS_ERROR_BLANK_CHECK
} C40_Ip_StatusType;

typedef uint32 C40_Ip_VirtualSectorsType;

typedef struct
{
    uint32 Dummy;
} C40_Ip_ConfigType;

extern const C40_Ip_ConfigType C40_Ip_InitCfg;

C40_Ip_StatusType C40_Ip_Init(const C40_Ip_ConfigType *ConfigPtr);

C40_Ip_StatusType C40_Ip_Read(uint32 LogicalAddress,
                              uint32 Length,
                              uint8 *Dest);

C40_Ip_StatusType C40_Ip_ClearLock(C40_Ip_VirtualSectorsType VirtualSector,
                                   uint8 DomainIdValue);

C40_Ip_StatusType C40_Ip_MainInterfaceWrite(uint32 LogicalAddress,
                                            uint32 Length,
                                            const uint8 *Src,
                                            uint8 DomainIdValue);

C40_Ip_StatusType C40_Ip_MainInterfaceWriteStatus(void);

C40_Ip_StatusType C40_Ip_MainInterfaceSectorErase(C40_Ip_VirtualSectorsType VirtualSector,
                                                  uint8 DomainIdValue);

C40_Ip_StatusType C40_Ip_MainInterfaceSectorEraseStatus(void);

/* ================================================================================================
 * SIL-only control and inspection surface (not present on target)
 * ============================================================================================== */

#define SIL_FLASH_BASE          (0x10000000UL)
#define SIL_FLASH_SIZE          (0x2000UL)

/** Wipe the simulated sector to the erased state and reset all counters. */
void Sil_C40_Wipe(void);

/** Raw byte access for test inspection. Offset is relative to SIL_FLASH_BASE. */
uint8  Sil_C40_ReadByte(uint32 offset);
void   Sil_C40_WriteByteRaw(uint32 offset, uint8 value);

/** Counters, so tests can assert on wear behaviour. */
uint32 Sil_C40_EraseCount(void);
uint32 Sil_C40_WriteCount(void);

/** Fault injection: make the next write / erase report failure. */
void Sil_C40_FailNextWrite(boolean enable);
void Sil_C40_FailNextErase(boolean enable);

/**
 * Power-loss injection: after this many further byte-programs, the flash
 * stops accepting data (simulating loss of supply mid-operation).
 * 0 disables the trap.
 */
void Sil_C40_PowerLossAfterBytes(uint32 bytes);

#endif /* C40_IP_H */
