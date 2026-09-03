#ifndef BMS_NVM_H
#define BMS_NVM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

void Bms_Nvm_Init(void);

boolean Bms_Nvm_LoadSoc(uint16 *Soc_pct_x10);

boolean Bms_Nvm_SaveSoc(uint16 Soc_pct_x10);

#ifdef __cplusplus
}
#endif

#endif /* BMS_NVM_H */