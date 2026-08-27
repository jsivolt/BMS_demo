#ifndef BMS_VPACK_H
#define BMS_VPACK_H

#include "Std_Types.h"

/*
 * Virtual ADBMS2950 CAN frame IDs
 */
#define BMS_VPACK_CAN_ID_CURRENT        (0x410U)

/*
 * Status bits
 */
#define BMS_VPACK_STATUS_CURRENT_FAULT  (0x01U)
#define BMS_VPACK_STATUS_SHUNT_FAULT    (0x02U)
#define BMS_VPACK_STATUS_COMM_FAULT     (0x04U)

/*
 * Decoded ADBMS2950 simulated measurements
 */
typedef struct
{
    sint32 PackCurrent_mA;
    sint16 ShuntVoltage_uV;

    uint8 AliveCounter;
    uint8 Status;

    boolean Valid;

} Bms_Vpack_DataType;


/*
 * Latest decoded pack-monitor data.
 *
 * Exposed for bring-up/debug first.
 * We can encapsulate this later.
 */
extern volatile Bms_Vpack_DataType g_BmsVpackData;

extern volatile uint32 g_BmsVpackFrameCount;


/*
 * Initialize virtual ADBMS2950 interface.
 */
void Bms_Vpack_Init(void);


/*
 * Process one CAN2 frame.
 */
void Bms_Vpack_ProcessFrame(
    uint32 canId,
    uint8 dlc,
    const uint8 *data
);

#endif /* BMS_VPACK_H */