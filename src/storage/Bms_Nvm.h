#ifndef BMS_NVM_H
#define BMS_NVM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

void Bms_Nvm_Init(void);

/**
 * @brief Loads the last persisted SOC triple (weak / strong / average cell).
 *
 * @param[out] SocMin_pct_x10 Weak-cell SOC. Unit: 0.1 %.
 * @param[out] SocMax_pct_x10 Strong-cell SOC. Unit: 0.1 %.
 * @param[out] SocAvg_pct_x10 Average-cell SOC. Unit: 0.1 %.
 * @return TRUE when a valid record was found and all three outputs were written.
 */
boolean Bms_Nvm_LoadSoc(
    uint16 *SocMin_pct_x10,
    uint16 *SocMax_pct_x10,
    uint16 *SocAvg_pct_x10);

/**
 * @brief Appends a new SOC triple to the persistence sector.
 *
 * @param[in] SocMin_pct_x10 Weak-cell SOC. Unit: 0.1 %, must be <= 1000.
 * @param[in] SocMax_pct_x10 Strong-cell SOC. Unit: 0.1 %, must be <= 1000.
 * @param[in] SocAvg_pct_x10 Average-cell SOC. Unit: 0.1 %, must be <= 1000.
 * @return TRUE when the record was written and read back successfully.
 */
boolean Bms_Nvm_SaveSoc(
    uint16 SocMin_pct_x10,
    uint16 SocMax_pct_x10,
    uint16 SocAvg_pct_x10);

#ifdef __cplusplus
}
#endif

#endif /* BMS_NVM_H */
