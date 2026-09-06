/**
 *  @file   sil_api.h
 *  @brief  Control and observation surface exported to the Python test layer.
 *
 *  Everything here is SIL-only scaffolding. The production modules underneath
 *  are compiled unmodified from src/.
 */

#ifndef SIL_API_H
#define SIL_API_H

#include "Std_Types.h"

#if defined(_WIN32)
#define SIL_API __declspec(dllexport)
#else
#define SIL_API __attribute__((visibility("default")))
#endif

/* ================================================================================================
 * Lifecycle
 * ============================================================================================== */

/** Erase the simulated Data Flash and reset all SIL counters. Survives PowerOn. */
SIL_API void Sil_FlashWipe(void);

/**
 * Cold boot: runs the same module init order main.c uses, minus hardware.
 * Data Flash contents persist across this call, so it models a power cycle.
 */
SIL_API void Sil_PowerOn(void);

/* ================================================================================================
 * Time / task execution — deterministic, no wall clock
 * ============================================================================================== */

/** One 100 ms slot, in main.c's order for the in-scope modules. */
SIL_API void Sil_Run100ms(void);

/** One 1000 ms slot. */
SIL_API void Sil_Run1000ms(void);

/**
 * Advance virtual time. Runs a 100 ms slot per 100 ms elapsed and a 1000 ms
 * slot per full second, in the same interleaving the scheduler produces.
 */
SIL_API void Sil_AdvanceMs(uint32 ms);

/** Virtual milliseconds elapsed since the last Sil_PowerOn(). */
SIL_API uint32 Sil_ElapsedMs(void);

/* ================================================================================================
 * Stimulus — injected as real CAN frames through the production decoders
 * ============================================================================================== */

/**
 * Inject a vPACK current frame (0x410). The alive counter is advanced
 * automatically so the alive check stays satisfied; use Sil_VpackSkipAlive()
 * to deliberately break it.
 */
SIL_API void Sil_InjectVpackCurrent(sint32 current_mA, sint16 shunt_uV, uint8 status);

/** Inject a vPACK voltage frame (0x411). */
SIL_API void Sil_InjectVpackVoltage(uint32 packVoltage_mV, uint32 busVoltage_mV);

/** Make the next current frame carry a wrong alive counter. */
SIL_API void Sil_VpackSkipAlive(void);

/** Freeze the alive counter, modelling a stuck vPACK transmitter. */
SIL_API void Sil_VpackFreezeAlive(boolean enable);

/**
 * Inject a complete vAFE measurement cycle: header 0x405 followed by the four
 * cell-voltage frames 0x401-0x404. cells_mV must point at 16 values.
 */
SIL_API void Sil_InjectVafeCycle(const uint16 *cells_mV);

/** Convenience: one vAFE cycle with all 16 cells at the same voltage. */
SIL_API void Sil_InjectVafeUniform(uint16 cell_mV);

/** Drive the Bms_Adc / Bms_Ntc doubles. */
SIL_API void Sil_SetAdcPackVoltages(uint16 v2_mV, uint16 v3_mV, boolean valid);
SIL_API void Sil_SetNtc(sint16 t1_dC, sint16 t2_dC, sint16 t3_dC, boolean valid);

/* ================================================================================================
 * Observation — SOC
 * ============================================================================================== */

SIL_API uint16  Sil_SocPack_pct_x10(void);
SIL_API boolean Sil_SocPackValid(void);

SIL_API uint16  Sil_SocMin_pct_x10(void);
SIL_API uint16  Sil_SocMax_pct_x10(void);
SIL_API uint16  Sil_SocAvg_pct_x10(void);

SIL_API float32 Sil_SocMinCapacity_mAh(void);
SIL_API float32 Sil_SocMaxCapacity_mAh(void);
SIL_API float32 Sil_SocAvgCapacity_mAh(void);

SIL_API boolean Sil_SocMinValid(void);
SIL_API boolean Sil_SocMaxValid(void);
SIL_API boolean Sil_SocAvgValid(void);

/** The legacy single-value view the CAN SOC frame reads. */
SIL_API uint16  Sil_SocLegacy_pct_x10(void);
SIL_API boolean Sil_SocLegacyValid(void);

/** How the estimators were seeded at startup: 0=default, 1=OCV, 2=NVM. */
SIL_API uint8   Sil_SocInitSource(void);

SIL_API void    Sil_SetSoc_pct_x10(uint16 soc_pct_x10);
SIL_API uint16  Sil_OcvToSoc(uint16 voltage_mV);

/* ================================================================================================
 * Observation — Battery_Monitor
 * ============================================================================================== */

SIL_API sint32  Sil_PackCurrent_mA(void);
SIL_API boolean Sil_PackCurrentValid(void);
SIL_API float32 Sil_MinCellVoltage(void);
SIL_API float32 Sil_MaxCellVoltage(void);
SIL_API float32 Sil_AverageCellVoltage(void);
SIL_API float32 Sil_DeltaCellVoltage(void);
SIL_API boolean Sil_CellVoltageValid(void);
SIL_API float32 Sil_PackV1(void);

/* ================================================================================================
 * Observation / control — NVM and flash
 * ============================================================================================== */

SIL_API uint32  Sil_FlashEraseCount(void);
SIL_API uint32  Sil_FlashWriteCount(void);
SIL_API uint8   Sil_FlashByte(uint32 offset);

/** Poke a raw byte into the flash model. Test setup only - bypasses NOR rules. */
SIL_API void    Sil_FlashWriteByte(uint32 offset, uint8 value);
SIL_API void    Sil_FlashFailNextWrite(boolean enable);
SIL_API void    Sil_FlashFailNextErase(boolean enable);
SIL_API void    Sil_FlashPowerLossAfterBytes(uint32 bytes);

/** Number of 24-byte record slots currently programmed (magic present). */
SIL_API uint32  Sil_NvmRecordCount(void);

/** Read back the newest persisted triple. Returns FALSE when none is valid. */
SIL_API boolean Sil_NvmLoad(uint16 *socMin, uint16 *socMax, uint16 *socAvg);

/* ================================================================================================
 * Direct unit access — Lib_Interp
 * ============================================================================================== */

/** flatTable holds rows*2 uint16 values laid out as X,Y,X,Y,... */
SIL_API uint16 Sil_InterpLookup(const uint16 *flatTable, uint16 rows, uint16 x);

#endif /* SIL_API_H */
