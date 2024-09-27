/**
 * @file definitions.h
 * @author Bastien Morino
 *
 * @brief Useful definitions.
 *
 * This header defines type abbreviations, as well as a very basic
 * @ref bool "boolean" type.
 * There also is a preprocessor check for DEBUG and TRACE macros.
 *
 * #### Numeric type name convention
 * Numeric type names defined in the following header are composed of
 * a letter representing the type of number, followed by the size in bits
 * of a number of this type in memory.
 *
 * Avcailable number types are :
 * - `u`: Unsigned integer.
 * - `i`: Signed integer.
 * - `f`: Floating point number.
 *
 * Available sizes are :
 * - 8, 16, 32 and 64 bits for integer types,
 * - 32 and 64 bits for floating point number types.
 *
 * @note The `char` built-in type should be preferred when dealing with *characters*,
 * instead of the `u8` and `i8` types, as these are intended to represent *numbers*.
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#ifdef TRACE
#ifndef DEBUG
#error Macro "DEBUG" must be defined if "TRACE" is defined.
#endif
#endif

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long int u64;

typedef signed char i8;
typedef signed short int i16;
typedef signed int i32;
typedef signed long int i64;

typedef float f32;
typedef double f64;

/** Basic boolean type. */
typedef unsigned char bool;

/** Boolean `false` constant. */
#define FALSE (bool)0
/** Boolean `true` constant. */
#define TRUE (bool)1
#define FAIL (size_t)-1

#endif /* ! DEFINITIONS_H */
