/**
 *  @file       Lib_Interp.c
 *  @brief      Generic table interpolation helpers.
 */

#include "Lib_Interp.h"

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

/**
 * @brief Locates the pair of breakpoints bracketing @p v on one axis.
 *
 * Returns the bracket as two indices plus the fraction between them, expressed
 * as a numerator over a denominator so the caller can blend without floats.
 * A clamped input, a single-breakpoint axis and a duplicated breakpoint all
 * come back as num = 0 over den = 1, which blends to the lower value.
 */
static void Lib_Interp_Bracket(
    const sint32 *axis,
    uint16 count,
    sint32 v,
    uint16 *i0,
    uint16 *i1,
    uint32 *num,
    uint32 *den)
{
    uint16 i;

    *num = 0U;
    *den = 1U;

    /* At or below the first breakpoint: clamp, never extrapolate. */
    if (v <= axis[0])
    {
        *i0 = 0U;
        *i1 = 0U;
        return;
    }

    /* At or above the last breakpoint: clamp, never extrapolate. */
    if (v >= axis[count - 1U])
    {
        *i0 = (uint16)(count - 1U);
        *i1 = *i0;
        return;
    }

    for (i = 0U; i < (count - 1U); i++)
    {
        if ((v >= axis[i]) && (v <= axis[i + 1U]))
        {
            *i0 = i;
            *i1 = (uint16)(i + 1U);

            if (axis[i + 1U] > axis[i])
            {
                *den = (uint32)(axis[i + 1U] - axis[i]);
                *num = (uint32)(v - axis[i]);
            }
            else
            {
                /* Duplicate breakpoint: nothing to interpolate across. */
                *i1 = i;
            }

            return;
        }
    }

    /*
     * Unreachable while the axis is sorted ascending: the clamp checks above
     * already cover v at or beyond either end. Defensive only.
     */
    *i0 = (uint16)(count - 1U);
    *i1 = *i0;
}

/**
 * @brief Linear blend of two values at fraction @p num / @p den.
 *
 * The dependent value is not required to increase with the independent one (a
 * derating curve falls), so the span is taken as a magnitude and the direction
 * applied afterwards - the same treatment the 1-D lookup uses.
 */
static uint16 Lib_Interp_Blend(uint16 v0, uint16 v1, uint32 num, uint32 den)
{
    uint32 delta;

    if ((num == 0U) || (den == 0U) || (v0 == v1))
    {
        return v0;
    }

    if (v1 > v0)
    {
        delta = ((uint32)(v1 - v0) * num) / den;

        return (uint16)((uint32)v0 + delta);
    }

    delta = ((uint32)(v0 - v1) * num) / den;

    return (uint16)((uint32)v0 - delta);
}

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

uint16 Lib_Interp_Lookup_2D_uint16(
    const sint32 *xAxis,
    uint16        xCount,
    const sint32 *yAxis,
    uint16        yCount,
    const uint16 *values,
    sint32        x,
    sint32        y)
{
    uint16 ix0;
    uint16 ix1;
    uint16 iy0;
    uint16 iy1;
    uint32 xNum;
    uint32 xDen;
    uint32 yNum;
    uint32 yDen;
    uint16 lowRow;
    uint16 highRow;

    if ((xAxis == NULL_PTR) || (yAxis == NULL_PTR) || (values == NULL_PTR) ||
        (xCount == 0U) || (yCount == 0U))
    {
        return 0U;
    }

    Lib_Interp_Bracket(xAxis, xCount, x, &ix0, &ix1, &xNum, &xDen);
    Lib_Interp_Bracket(yAxis, yCount, y, &iy0, &iy1, &yNum, &yDen);

    /* Interpolate along X on each bracketing row, then between the two rows. */
    lowRow = Lib_Interp_Blend(
        values[((uint32)iy0 * xCount) + ix0],
        values[((uint32)iy0 * xCount) + ix1],
        xNum, xDen);

    highRow = Lib_Interp_Blend(
        values[((uint32)iy1 * xCount) + ix0],
        values[((uint32)iy1 * xCount) + ix1],
        xNum, xDen);

    return Lib_Interp_Blend(lowRow, highRow, yNum, yDen);
}
