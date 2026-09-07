/**
 *  @file       Lib_Interp.c
 *  @brief      Generic table interpolation helpers.
 */

#include "Lib_Interp.h"

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

uint16 Lib_Interp_Lookup_1D_uint16(
    const uint16 table[][2],
    uint16 tableSize,
    uint16 x)
{
    uint16 i;
    uint16 y0;
    uint16 y1;
    uint32 xSpan;
    uint32 ySpan;
    uint32 xOffset;
    uint32 delta;

    if ((table == NULL_PTR) || (tableSize == 0U))
    {
        return 0U;
    }

    /* Below the first breakpoint: clamp, never extrapolate. */
    if (x <= table[0][0])
    {
        return table[0][1];
    }

    /* At or above the last breakpoint: clamp, never extrapolate. */
    if (x >= table[tableSize - 1U][0])
    {
        return table[tableSize - 1U][1];
    }

    for (i = 0U; i < (tableSize - 1U); i++)
    {
        if ((x >= table[i][0]) && (x <= table[i + 1U][0]))
        {
            xSpan = (uint32)table[i + 1U][0] - (uint32)table[i][0];

            if (xSpan == 0U)
            {
                /* Duplicate X in the table: nothing to interpolate across. */
                return table[i][1];
            }

            y0 = table[i][1];
            y1 = table[i + 1U][1];

            xOffset = (uint32)x - (uint32)table[i][0];

            /*
             * Y is not required to increase with X (a derating or NTC curve
             * falls), so the span is taken as a magnitude and the direction
             * applied afterwards. Worst case 65535 * 65535 still fits uint32.
             */
            if (y1 >= y0)
            {
                ySpan = (uint32)y1 - (uint32)y0;
                delta = (xOffset * ySpan) / xSpan;

                return (uint16)((uint32)y0 + delta);
            }

            ySpan = (uint32)y0 - (uint32)y1;
            delta = (xOffset * ySpan) / xSpan;

            return (uint16)((uint32)y0 - delta);
        }
    }

    /*
     * Unreachable while the table is sorted ascending: the clamp checks above
     * already cover x at or beyond either end. Defensive only.
     */
    return table[tableSize - 1U][1];
}
