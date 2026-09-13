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

/**
 * @brief Bilinearly interpolates a value from a 2-D uint16 map.
 *
 * The map is an explicit-axis calibration table: two independent breakpoint
 * arrays and a rectangular block of dependent values. Both axes are sint32 so
 * a negative independent variable (a temperature axis, say) needs no offset.
 *
 * @p values is row-major and indexed values[(iy * xCount) + ix], so one row
 * holds every X breakpoint at a single Y. It must therefore hold exactly
 * xCount * yCount entries.
 *
 * Both axes must be sorted ascending and hold at least one breakpoint. This is
 * a caller responsibility and is not checked at runtime. An axis of one
 * breakpoint is legal and makes the map constant along that axis. Duplicate
 * neighbouring breakpoints are tolerated: the lower one wins, as in the 1-D
 * case.
 *
 * Inputs at or beyond an edge are clamped to that edge, on each axis
 * independently, so all four corners and all four edges clamp rather than
 * extrapolate. As in the 1-D case the function never reports an out-of-range
 * input; a caller that must tell "inside the map" from "clamped" has to
 * compare against the axis end points itself.
 *
 * @note Arithmetic is 32-bit. The span between neighbouring breakpoints on
 *       either axis must not exceed 65535, so that span x value-delta stays
 *       inside uint32. Also a caller responsibility, unchecked.
 *
 * @param[in] xAxis   X breakpoints, sorted ascending.
 * @param[in] xCount  Number of X breakpoints. Must be >= 1.
 * @param[in] yAxis   Y breakpoints, sorted ascending.
 * @param[in] yCount  Number of Y breakpoints. Must be >= 1.
 * @param[in] values  xCount * yCount dependent values, row-major by Y.
 * @param[in] x       X value to look up.
 * @param[in] y       Y value to look up.
 * @return Interpolated value, or 0 if any pointer is NULL_PTR or either count
 *         is zero.
 */
uint16 Lib_Interp_Lookup_2D_uint16(
    const sint32 *xAxis,
    uint16        xCount,
    const sint32 *yAxis,
    uint16        yCount,
    const uint16 *values,
    sint32        x,
    sint32        y);

#ifdef __cplusplus
}
#endif

#endif /* LIB_INTERP_H */
