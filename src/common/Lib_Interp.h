/**
 *  @file       Lib_Interp.h
 *  @brief      Generic table interpolation helpers.
 *
 *  Reusable, module-independent lookup routines. Nothing here knows about
 *  batteries, voltages or SOC: callers supply a sorted table and an input,
 *  and get back an interpolated output.
 */

#ifndef LIB_INTERP_H
#define LIB_INTERP_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Linearly interpolates Y for the given X in a 1-D uint16 table.
 *
 * The table is a 2-column array: table[i][0] is the independent value (X),
 * table[i][1] the dependent value (Y). It must be sorted ascending by X and
 * hold at least one row. This is a caller responsibility and is not checked
 * at runtime.
 *
 * Inputs at or beyond either end of the table are clamped to that end's Y;
 * the function never extrapolates and never reports an out-of-range input.
 * Callers that must distinguish "inside the table" from "clamped" have to
 * compare against the table's first/last X themselves.
 *
 * @param[in] table     Table rows {X, Y}, sorted ascending by X.
 * @param[in] tableSize Number of rows in @p table. Must be >= 1.
 * @param[in] x         Value to look up.
 * @return Interpolated Y, or the nearest end row's Y when clamped.
 */
uint16 Lib_Interp_Lookup_1D_uint16(
    const uint16 table[][2],
    uint16 tableSize,
    uint16 x);

#ifdef __cplusplus
}
#endif

#endif /* LIB_INTERP_H */
