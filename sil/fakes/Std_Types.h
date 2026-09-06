/**
 *  @file   Std_Types.h  (SIL fake)
 *  @brief  Host-side stand-in for the RTD/AUTOSAR base types.
 *
 *  Mirrors the widths the target build uses (S32K344, 32-bit ARM):
 *  on x86-64 these map to the same underlying sizes, so struct layouts
 *  of the application types are identical between SIL and target.
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;
typedef int64_t  sint64;

typedef float    float32;
typedef double   float64;

typedef bool     boolean;

typedef uint8    Std_ReturnType;

#ifndef TRUE
#define TRUE  true
#endif

#ifndef FALSE
#define FALSE false
#endif

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

#ifndef E_OK
#define E_OK     ((Std_ReturnType)0x00U)
#endif

#ifndef E_NOT_OK
#define E_NOT_OK ((Std_ReturnType)0x01U)
#endif

#endif /* STD_TYPES_H */
