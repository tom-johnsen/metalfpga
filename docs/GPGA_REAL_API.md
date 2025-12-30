# GPGA Real Library API Reference

## Overview

The `gpga_real.h` library provides a complete software-defined IEEE-754 double-precision floating-point implementation for Metal GPU programming. This library implements over 346 functions supporting high-precision mathematical operations when hardware double-precision is unavailable or insufficient.

**Key Features:**
- Full IEEE-754 binary64 (double precision) implementation in software
- Support for all 4 IEEE-754 rounding modes (round to nearest, up, down, toward zero)
- CRLibM-compatible correctly-rounded mathematical functions
- High-precision intermediate calculations using double-double and triple-double arithmetic
- Optimized for Apple GPU (Metal Shading Language)
- Special π-scaled trigonometric functions for improved accuracy

---

## Table of Contents

1. [Core Type Definitions](#core-type-definitions)
2. [Trace and Debugging](#trace-and-debugging)
3. [Bit Manipulation Utilities](#bit-manipulation-utilities)
4. [IEEE-754 Double Precision Helpers](#ieee-754-double-precision-helpers)
5. [Mathematical Constants](#mathematical-constants)
6. [Low-Level Arithmetic Helpers](#low-level-arithmetic-helpers)
7. [Type Conversion Functions](#type-conversion-functions)
8. [Basic Arithmetic Operations](#basic-arithmetic-operations)
9. [Comparison Operations](#comparison-operations)
10. [Rounding Functions](#rounding-functions)
11. [Exponential and Logarithm Functions](#exponential-and-logarithm-functions)
12. [Power and Square Root](#power-and-square-root)
13. [Trigonometric Functions](#trigonometric-functions)
14. [Inverse Trigonometric Functions](#inverse-trigonometric-functions)
15. [Hyperbolic Functions](#hyperbolic-functions)
16. [Double-Double Arithmetic](#double-double-arithmetic)
17. [Rounding Mode Utilities](#rounding-mode-utilities)
18. [SCS (Software Carried Significand) Library](#scs-software-carried-significand-library)
19. [Rounding Modes Reference](#rounding-modes-reference)

---

## Core Type Definitions

### `gpga_double`

```cpp
typedef ulong gpga_double;
```

Software-defined double precision floating-point type. Internally represented as a 64-bit unsigned integer (`ulong`) with IEEE-754 binary64 layout:
- Bit 63: Sign bit
- Bits 62-52: Biased exponent (11 bits)
- Bits 51-0: Mantissa/significand (52 bits)

---

## Trace and Debugging

### `gpga_real_trace_counters()`

```cpp
inline GpgaRealTraceCounters& gpga_real_trace_counters()
```

**Description:** Returns reference to trace counters for monitoring fallback usage.

**Returns:** Reference to global trace counters structure.

**Availability:** Only available when `GPGA_REAL_TRACE` is defined and not in Metal environment.

---

### `gpga_real_trace_reset()`

```cpp
inline void gpga_real_trace_reset()
```

**Description:** Resets all trace counters to zero.

**Availability:** Only available when `GPGA_REAL_TRACE` is defined and not in Metal environment.

---

## Bit Manipulation Utilities

### `gpga_u64_hi()`

```cpp
inline uint gpga_u64_hi(ulong value)
```

**Description:** Extracts the high 32 bits from a 64-bit value.

**Parameters:**
- `value`: 64-bit unsigned integer

**Returns:** High 32 bits as `uint`.

---

### `gpga_u64_lo()`

```cpp
inline uint gpga_u64_lo(ulong value)
```

**Description:** Extracts the low 32 bits from a 64-bit value.

**Parameters:**
- `value`: 64-bit unsigned integer

**Returns:** Low 32 bits as `uint`.

---

### `gpga_u64_from_words()`

```cpp
inline gpga_double gpga_u64_from_words(uint hi, uint lo)
```

**Description:** Constructs a 64-bit value from two 32-bit words.

**Parameters:**
- `hi`: High 32 bits
- `lo`: Low 32 bits

**Returns:** Combined 64-bit value.

---

## IEEE-754 Double Precision Helpers

### Component Extraction

#### `gpga_double_sign()`

```cpp
inline uint gpga_double_sign(gpga_double d)
```

**Description:** Extracts the sign bit from a double.

**Parameters:**
- `d`: Double precision value

**Returns:** Sign bit (0 for positive, 1 for negative).

---

#### `gpga_double_exp()`

```cpp
inline uint gpga_double_exp(gpga_double d)
```

**Description:** Extracts the biased exponent field (11 bits).

**Parameters:**
- `d`: Double precision value

**Returns:** Biased exponent value (0-2047).

**Notes:**
- 0 indicates zero or denormal
- 2047 (0x7FF) indicates infinity or NaN
- For normalized numbers, actual exponent = returned value - 1023

---

#### `gpga_double_mantissa()`

```cpp
inline ulong gpga_double_mantissa(gpga_double d)
```

**Description:** Extracts the mantissa/significand (52 bits).

**Parameters:**
- `d`: Double precision value

**Returns:** 52-bit mantissa value.

**Notes:** Does not include the implicit leading 1 bit for normalized numbers.

---

#### `gpga_double_exponent()`

```cpp
inline int gpga_double_exponent(gpga_double d)
```

**Description:** Returns the actual (unbiased) exponent, handling denormals correctly.

**Parameters:**
- `d`: Double precision value

**Returns:** Actual exponent value.

**Notes:**
- For normalized numbers: exponent - 1023
- For denormals: adjusts for leading zeros
- For zero: returns -1023

---

### Component Construction

#### `gpga_double_pack()`

```cpp
inline gpga_double gpga_double_pack(uint sign, uint exp, ulong mantissa)
```

**Description:** Packs sign, exponent, and mantissa into a double.

**Parameters:**
- `sign`: Sign bit (0 or 1)
- `exp`: Biased exponent (0-2047)
- `mantissa`: 52-bit mantissa

**Returns:** Packed double precision value.

---

### Special Value Constructors

#### `gpga_double_zero()`

```cpp
inline gpga_double gpga_double_zero(uint sign)
```

**Description:** Creates a signed zero value.

**Parameters:**
- `sign`: 0 for +0.0, 1 for -0.0

**Returns:** Signed zero.

---

#### `gpga_double_inf()`

```cpp
inline gpga_double gpga_double_inf(uint sign)
```

**Description:** Creates a signed infinity value.

**Parameters:**
- `sign`: 0 for +Inf, 1 for -Inf

**Returns:** Signed infinity.

---

#### `gpga_double_nan()`

```cpp
inline gpga_double gpga_double_nan()
```

**Description:** Creates a quiet NaN (Not a Number) value.

**Returns:** NaN value.

---

### Classification Functions

#### `gpga_double_is_zero()`

```cpp
inline bool gpga_double_is_zero(gpga_double d)
```

**Description:** Tests if a value is zero (positive or negative).

**Parameters:**
- `d`: Double precision value

**Returns:** `true` if zero, `false` otherwise.

---

#### `gpga_double_is_inf()`

```cpp
inline bool gpga_double_is_inf(gpga_double d)
```

**Description:** Tests if a value is infinity (positive or negative).

**Parameters:**
- `d`: Double precision value

**Returns:** `true` if infinity, `false` otherwise.

---

#### `gpga_double_is_nan()`

```cpp
inline bool gpga_double_is_nan(gpga_double d)
```

**Description:** Tests if a value is NaN.

**Parameters:**
- `d`: Double precision value

**Returns:** `true` if NaN, `false` otherwise.

---

### Bit Conversion

#### `gpga_bits_to_real()`

```cpp
inline gpga_double gpga_bits_to_real(ulong bits)
```

**Description:** Reinterprets a 64-bit integer as a double (no conversion).

**Parameters:**
- `bits`: 64-bit integer with IEEE-754 bit pattern

**Returns:** Double with same bit pattern.

---

#### `gpga_real_to_bits()`

```cpp
inline ulong gpga_real_to_bits(gpga_double value)
```

**Description:** Gets the raw bit representation of a double.

**Parameters:**
- `value`: Double precision value

**Returns:** 64-bit integer representation.

---

### Utility Functions

#### `gpga_double_abs()`

```cpp
inline gpga_double gpga_double_abs(gpga_double x)
```

**Description:** Returns the absolute value by clearing the sign bit.

**Parameters:**
- `x`: Double precision value

**Returns:** Absolute value of `x`.

---

#### `gpga_double_next_up()`

```cpp
inline gpga_double gpga_double_next_up(gpga_double x)
```

**Description:** Returns the next representable value towards +∞.

**Parameters:**
- `x`: Double precision value

**Returns:** Next larger representable double.

**Notes:** Useful for interval arithmetic and directed rounding.

---

#### `gpga_double_next_down()`

```cpp
inline gpga_double gpga_double_next_down(gpga_double x)
```

**Description:** Returns the next representable value towards -∞.

**Parameters:**
- `x`: Double precision value

**Returns:** Next smaller representable double.

---

#### `gpga_double_raw_inc()`

```cpp
inline gpga_double gpga_double_raw_inc(gpga_double x)
```

**Description:** Increments the raw bit representation by 1.

**Parameters:**
- `x`: Double precision value

**Returns:** Value with bits incremented.

**Warning:** Use with caution - does not respect floating-point semantics.

---

#### `gpga_double_raw_dec()`

```cpp
inline gpga_double gpga_double_raw_dec(gpga_double x)
```

**Description:** Decrements the raw bit representation by 1.

**Parameters:**
- `x`: Double precision value

**Returns:** Value with bits decremented.

**Warning:** Use with caution - does not respect floating-point semantics.

---

## Mathematical Constants

All constants are returned as `gpga_double` values with high precision.

### Basic Constants

#### `gpga_double_const_one()`
```cpp
inline gpga_double gpga_double_const_one()
```
**Returns:** 1.0

---

#### `gpga_double_const_two()`
```cpp
inline gpga_double gpga_double_const_two()
```
**Returns:** 2.0

---

#### `gpga_double_const_minus_one()`
```cpp
inline gpga_double gpga_double_const_minus_one()
```
**Returns:** -1.0

---

### Transcendental Constants

#### `gpga_double_const_pi()`
```cpp
inline gpga_double gpga_double_const_pi()
```
**Returns:** π (3.141592653589793...)

---

#### `gpga_double_const_two_pi()`
```cpp
inline gpga_double gpga_double_const_two_pi()
```
**Returns:** 2π

---

#### `gpga_double_const_half_pi()`
```cpp
inline gpga_double gpga_double_const_half_pi()
```
**Returns:** π/2

---

#### `gpga_double_const_quarter_pi()`
```cpp
inline gpga_double gpga_double_const_quarter_pi()
```
**Returns:** π/4

---

#### `gpga_double_const_inv_two_pi()`
```cpp
inline gpga_double gpga_double_const_inv_two_pi()
```
**Returns:** 1/(2π)

---

### Logarithmic Constants

#### `gpga_double_const_ln2()`
```cpp
inline gpga_double gpga_double_const_ln2()
```
**Returns:** ln(2) (natural logarithm of 2)

---

#### `gpga_double_const_ln10()`
```cpp
inline gpga_double gpga_double_const_ln10()
```
**Returns:** ln(10)

---

#### `gpga_double_const_log10e()`
```cpp
inline gpga_double gpga_double_const_log10e()
```
**Returns:** log₁₀(e)

---

#### `gpga_double_const_inv_ln2()`
```cpp
inline gpga_double gpga_double_const_inv_ln2()
```
**Returns:** 1/ln(2)

---

### Reciprocal Constants

#### `gpga_double_const_inv2()` through `gpga_double_const_inv15()`
```cpp
inline gpga_double gpga_double_const_inv2()   // 1/2
inline gpga_double gpga_double_const_inv3()   // 1/3
inline gpga_double gpga_double_const_inv5()   // 1/5
inline gpga_double gpga_double_const_inv7()   // 1/7
inline gpga_double gpga_double_const_inv9()   // 1/9
inline gpga_double gpga_double_const_inv11()  // 1/11
inline gpga_double gpga_double_const_inv13()  // 1/13
inline gpga_double gpga_double_const_inv15()  // 1/15
```

**Returns:** Reciprocals of odd numbers (used in series expansions).

---

### Factorial Reciprocals

#### `gpga_double_const_inv6()` through `gpga_double_const_inv6227020800()`
```cpp
inline gpga_double gpga_double_const_inv6()          // 1/3! = 1/6
inline gpga_double gpga_double_const_inv24()         // 1/4! = 1/24
inline gpga_double gpga_double_const_inv120()        // 1/5! = 1/120
inline gpga_double gpga_double_const_inv720()        // 1/6! = 1/720
inline gpga_double gpga_double_const_inv5040()       // 1/7!
inline gpga_double gpga_double_const_inv40320()      // 1/8!
inline gpga_double gpga_double_const_inv362880()     // 1/9!
inline gpga_double gpga_double_const_inv3628800()    // 1/10!
inline gpga_double gpga_double_const_inv39916800()   // 1/11!
inline gpga_double gpga_double_const_inv479001600()  // 1/12!
inline gpga_double gpga_double_const_inv6227020800() // 1/13!
```

**Returns:** Reciprocals of factorials (used in Taylor series).

---

### Special Constants

#### `gpga_double_dekker_const()`
```cpp
inline gpga_double gpga_double_dekker_const()
```
**Returns:** Constant used for Dekker splitting in error-free transformations.

---

## Low-Level Arithmetic Helpers

### `gpga_clz64()`

```cpp
inline uint gpga_clz64(ulong value)
```

**Description:** Counts leading zeros in a 64-bit value.

**Parameters:**
- `value`: 64-bit unsigned integer

**Returns:** Number of leading zero bits (0-64).

**Notes:** Used for normalization in arithmetic operations.

---

### `gpga_shift_right_sticky()`

```cpp
inline ulong gpga_shift_right_sticky(ulong value, uint shift)
```

**Description:** Right shifts with sticky bit preservation for correct rounding.

**Parameters:**
- `value`: Value to shift
- `shift`: Number of bits to shift

**Returns:** Shifted value with sticky bit set if any 1 bits were shifted out.

**Notes:** The sticky bit ensures rounding correctness by remembering if non-zero bits were lost.

---

### `gpga_mul_64()`

```cpp
inline void gpga_mul_64(ulong a, ulong b, thread ulong* hi, thread ulong* lo)
```

**Description:** Performs 64×64 → 128-bit multiplication.

**Parameters:**
- `a`: First operand
- `b`: Second operand
- `hi`: Pointer to receive high 64 bits of result
- `lo`: Pointer to receive low 64 bits of result

---

### `gpga_shift_right_sticky_128()`

```cpp
inline ulong gpga_shift_right_sticky_128(ulong hi, ulong lo, uint shift)
```

**Description:** Right shifts a 128-bit value with sticky bit preservation.

**Parameters:**
- `hi`: High 64 bits
- `lo`: Low 64 bits
- `shift`: Number of bits to shift

**Returns:** Low 64 bits of shifted result with sticky bit.

---

### `gpga_double_round_pack()`

```cpp
inline gpga_double gpga_double_round_pack(uint sign, int exp, ulong mantissa_hi, ulong mantissa_lo)
```

**Description:** Internal function to round and pack components into a double.

**Parameters:**
- `sign`: Sign bit (0 or 1)
- `exp`: Unbiased exponent
- `mantissa_hi`: High part of mantissa
- `mantissa_lo`: Low part of mantissa (for rounding)

**Returns:** Properly rounded and packed double.

**Notes:** Handles normalization, rounding, and special cases.

---

### `gpga_div_mantissa()`

```cpp
inline ulong gpga_div_mantissa(ulong num, ulong den)
```

**Description:** Helper for mantissa division.

**Parameters:**
- `num`: Numerator mantissa
- `den`: Denominator mantissa

**Returns:** Quotient mantissa.

---

## Type Conversion Functions

### Integer to Double

#### `gpga_double_from_u64()`

```cpp
inline gpga_double gpga_double_from_u64(ulong value)
```

**Description:** Converts unsigned 64-bit integer to double.

**Parameters:**
- `value`: Unsigned 64-bit integer

**Returns:** Double precision representation.

**Notes:** Rounds to nearest if value cannot be represented exactly.

---

#### `gpga_double_from_s64()`

```cpp
inline gpga_double gpga_double_from_s64(long value)
```

**Description:** Converts signed 64-bit integer to double.

**Parameters:**
- `value`: Signed 64-bit integer

**Returns:** Double precision representation.

---

#### `gpga_double_from_u32()`

```cpp
inline gpga_double gpga_double_from_u32(uint value)
```

**Description:** Converts unsigned 32-bit integer to double.

**Parameters:**
- `value`: Unsigned 32-bit integer

**Returns:** Double precision representation (exact).

---

#### `gpga_double_from_s32()`

```cpp
inline gpga_double gpga_double_from_s32(int value)
```

**Description:** Converts signed 32-bit integer to double.

**Parameters:**
- `value`: Signed 32-bit integer

**Returns:** Double precision representation (exact).

---

### Double to Integer

#### `gpga_double_to_s64()`

```cpp
inline long gpga_double_to_s64(gpga_double d)
```

**Description:** Converts double to signed 64-bit integer (truncates towards zero).

**Parameters:**
- `d`: Double precision value

**Returns:** Truncated integer value.

**Notes:** Undefined behavior if value is out of range or NaN.

---

#### `gpga_double_to_s32()`

```cpp
inline int gpga_double_to_s32(gpga_double d)
```

**Description:** Converts double to signed 32-bit integer (truncates towards zero).

**Parameters:**
- `d`: Double precision value

**Returns:** Truncated integer value.

**Notes:** Undefined behavior if value is out of range or NaN.

---

#### `gpga_double_round_to_s64()`

```cpp
inline long gpga_double_round_to_s64(gpga_double d)
```

**Description:** Converts double to signed 64-bit integer (rounds to nearest).

**Parameters:**
- `d`: Double precision value

**Returns:** Rounded integer value.

**Notes:** Ties round to even.

---

### Scaling Functions

#### `gpga_double_ldexp()`

```cpp
inline gpga_double gpga_double_ldexp(gpga_double x, int exp)
```

**Description:** Multiplies by 2^exp (load exponent).

**Parameters:**
- `x`: Double precision value
- `exp`: Integer exponent

**Returns:** x × 2^exp

**Notes:** Efficiently adjusts the exponent field. Handles overflow/underflow.

---

#### `gpga_double_frexp()`

```cpp
inline gpga_double gpga_double_frexp(gpga_double x, thread int* exp_out)
```

**Description:** Extracts mantissa and exponent (fraction and exponent).

**Parameters:**
- `x`: Double precision value
- `exp_out`: Pointer to receive exponent

**Returns:** Normalized mantissa in range [0.5, 1.0)

**Notes:** Satisfies: x = returned_value × 2^(*exp_out)

---

## Basic Arithmetic Operations

### `gpga_double_neg()`

```cpp
inline gpga_double gpga_double_neg(gpga_double d)
```

**Description:** Negates a value (flips sign bit).

**Parameters:**
- `d`: Double precision value

**Returns:** -d

**Notes:** Works correctly for all values including ±0, ±∞, and NaN.

---

### `gpga_double_negate()`

```cpp
inline gpga_double gpga_double_negate(gpga_double d)
```

**Description:** Alias for `gpga_double_neg()`.

---

### `gpga_double_add()`

```cpp
inline gpga_double gpga_double_add(gpga_double a, gpga_double b)
```

**Description:** Adds two double precision values.

**Parameters:**
- `a`: First operand
- `b`: Second operand

**Returns:** a + b (correctly rounded to nearest)

**Notes:** Handles special cases (±∞, NaN, cancellation).

---

### `gpga_double_sub()`

```cpp
inline gpga_double gpga_double_sub(gpga_double a, gpga_double b)
```

**Description:** Subtracts two double precision values.

**Parameters:**
- `a`: Minuend
- `b`: Subtrahend

**Returns:** a - b (correctly rounded to nearest)

---

### `gpga_double_mul()`

```cpp
inline gpga_double gpga_double_mul(gpga_double a, gpga_double b)
```

**Description:** Multiplies two double precision values.

**Parameters:**
- `a`: First operand
- `b`: Second operand

**Returns:** a × b (correctly rounded to nearest)

**Notes:** Handles overflow, underflow, and special cases.

---

### `gpga_double_div()`

```cpp
inline gpga_double gpga_double_div(gpga_double a, gpga_double b)
```

**Description:** Divides two double precision values.

**Parameters:**
- `a`: Numerator
- `b`: Denominator

**Returns:** a ÷ b (correctly rounded to nearest)

**Notes:** Returns ±∞ for division by zero, NaN for 0/0 or ∞/∞.

---

## Comparison Operations

All comparison functions handle special values according to IEEE-754 rules:
- NaN compares unordered with everything (all comparisons return false except !=)
- -0 equals +0
- ±∞ compare as expected

### `gpga_double_eq()`

```cpp
inline bool gpga_double_eq(gpga_double a, gpga_double b)
```

**Description:** Tests for equality.

**Returns:** `true` if a == b, `false` otherwise (including if either is NaN).

---

### `gpga_double_lt()`

```cpp
inline bool gpga_double_lt(gpga_double a, gpga_double b)
```

**Description:** Tests for less than.

**Returns:** `true` if a < b, `false` otherwise.

---

### `gpga_double_le()`

```cpp
inline bool gpga_double_le(gpga_double a, gpga_double b)
```

**Description:** Tests for less than or equal.

**Returns:** `true` if a ≤ b, `false` otherwise.

---

### `gpga_double_gt()`

```cpp
inline bool gpga_double_gt(gpga_double a, gpga_double b)
```

**Description:** Tests for greater than.

**Returns:** `true` if a > b, `false` otherwise.

---

### `gpga_double_ge()`

```cpp
inline bool gpga_double_ge(gpga_double a, gpga_double b)
```

**Description:** Tests for greater than or equal.

**Returns:** `true` if a ≥ b, `false` otherwise.

---

## Rounding Functions

### `gpga_double_floor()`

```cpp
inline gpga_double gpga_double_floor(gpga_double x)
```

**Description:** Rounds towards -∞ (floor function).

**Parameters:**
- `x`: Double precision value

**Returns:** Largest integer ≤ x

**Examples:**
- floor(2.7) = 2.0
- floor(-2.3) = -3.0

---

### `gpga_double_ceil()`

```cpp
inline gpga_double gpga_double_ceil(gpga_double x)
```

**Description:** Rounds towards +∞ (ceiling function).

**Parameters:**
- `x`: Double precision value

**Returns:** Smallest integer ≥ x

**Examples:**
- ceil(2.3) = 3.0
- ceil(-2.7) = -2.0

---

## Exponential and Logarithm Functions

All exponential and logarithm functions are available in four rounding modes (see [Rounding Modes Reference](#rounding-modes-reference)).

### Natural Exponential (e^x)

#### `gpga_exp_rn()`

```cpp
inline gpga_double gpga_exp_rn(gpga_double x)
```

**Description:** Computes e^x (round to nearest).

**Parameters:**
- `x`: Exponent value

**Returns:** e^x

**Special Cases:**
- exp(+∞) = +∞
- exp(-∞) = +0
- exp(NaN) = NaN

**Aliases:** `gpga_double_exp_real()`

---

#### `gpga_exp_rd()`
```cpp
inline gpga_double gpga_exp_rd(gpga_double x)
```
**Description:** Computes e^x (round down).

---

#### `gpga_exp_ru()`
```cpp
inline gpga_double gpga_exp_ru(gpga_double x)
```
**Description:** Computes e^x (round up).

---

#### `gpga_exp_rz()`
```cpp
inline gpga_double gpga_exp_rz(gpga_double x)
```
**Description:** Computes e^x (round towards zero).

---

### Base-2 Exponential (2^x)

#### `gpga_exp2_rn()`

```cpp
inline gpga_double gpga_exp2_rn(gpga_double x)
```

**Description:** Computes 2^x (round to nearest).

**Parameters:**
- `x`: Exponent value

**Returns:** 2^x

---

#### `gpga_exp2_rd()`, `gpga_exp2_ru()`, `gpga_exp2_rz()`

Other rounding modes for 2^x.

---

### Exponential Minus One (e^x - 1)

#### `gpga_expm1_rn()`

```cpp
inline gpga_double gpga_expm1_rn(gpga_double x)
```

**Description:** Computes e^x - 1 (round to nearest).

**Parameters:**
- `x`: Exponent value

**Returns:** e^x - 1

**Notes:** Provides accurate results for small x where e^x ≈ 1.

**Aliases:** `gpga_double_expm1()`

---

#### `gpga_expm1_rd()`, `gpga_expm1_ru()`, `gpga_expm1_rz()`

Other rounding modes for e^x - 1.

---

### Natural Logarithm (ln/log)

#### `gpga_log_rn()`

```cpp
inline gpga_double gpga_log_rn(gpga_double x)
```

**Description:** Computes ln(x) = log_e(x) (round to nearest).

**Parameters:**
- `x`: Argument (must be > 0)

**Returns:** Natural logarithm of x

**Special Cases:**
- log(+0) = -∞
- log(x < 0) = NaN
- log(+∞) = +∞
- log(1) = +0

**Aliases:** `gpga_double_ln()`

---

#### `gpga_log_rd()`, `gpga_log_ru()`, `gpga_log_rz()`

Other rounding modes for ln(x).

---

### Logarithm Plus One (ln(1 + x))

#### `gpga_log1p_rn()`

```cpp
inline gpga_double gpga_log1p_rn(gpga_double x)
```

**Description:** Computes ln(1 + x) (round to nearest).

**Parameters:**
- `x`: Argument (must be > -1)

**Returns:** ln(1 + x)

**Notes:** Provides accurate results for small x where ln(1 + x) ≈ x.

---

#### `gpga_log1p_rd()`, `gpga_log1p_ru()`, `gpga_log1p_rz()`

Other rounding modes for ln(1 + x).

---

### Base-2 Logarithm

#### `gpga_log2_rn()`

```cpp
inline gpga_double gpga_log2_rn(gpga_double x)
```

**Description:** Computes log₂(x) (round to nearest).

**Parameters:**
- `x`: Argument (must be > 0)

**Returns:** Base-2 logarithm of x

**Aliases:** `gpga_double_log2()`

---

#### `gpga_log2_rd()`, `gpga_log2_ru()`, `gpga_log2_rz()`

Other rounding modes for log₂(x).

---

### Base-10 Logarithm

#### `gpga_log10_rn()`

```cpp
inline gpga_double gpga_log10_rn(gpga_double x)
```

**Description:** Computes log₁₀(x) (round to nearest).

**Parameters:**
- `x`: Argument (must be > 0)

**Returns:** Base-10 logarithm of x

**Aliases:** `gpga_double_log10()`

---

#### `gpga_log10_rd()`, `gpga_log10_ru()`, `gpga_log10_rz()`

Other rounding modes for log₁₀(x).

---

## Power and Square Root

### `gpga_double_sqrt()`

```cpp
inline gpga_double gpga_double_sqrt(gpga_double x)
```

**Description:** Computes square root (round to nearest).

**Parameters:**
- `x`: Argument (must be ≥ 0)

**Returns:** √x

**Special Cases:**
- sqrt(-0) = -0
- sqrt(+0) = +0
- sqrt(x < 0) = NaN
- sqrt(+∞) = +∞

---

### `gpga_double_pow_int()`

```cpp
inline gpga_double gpga_double_pow_int(gpga_double base, long exp)
```

**Description:** Computes base^exp for integer exponent.

**Parameters:**
- `base`: Base value
- `exp`: Integer exponent

**Returns:** base^exp

**Notes:** More efficient than general power function for integer exponents.

---

### `gpga_pow_rn()`

```cpp
inline gpga_double gpga_pow_rn(gpga_double x, gpga_double y)
```

**Description:** Computes x^y (round to nearest).

**Parameters:**
- `x`: Base
- `y`: Exponent

**Returns:** x^y

**Special Cases:**
- pow(±0, y < 0) = ±∞
- pow(±0, y > 0) = ±0
- pow(x < 0, non-integer y) = NaN
- pow(±1, ±∞) = 1
- Many other special cases per IEEE-754

**Aliases:** `gpga_double_pow()`

---

## Trigonometric Functions

All trigonometric functions are available in four rounding modes and have "pi-scaled" variants for improved accuracy when working with multiples of π.

### Sine

#### `gpga_sin_rn()`

```cpp
inline gpga_double gpga_sin_rn(gpga_double x)
```

**Description:** Computes sin(x) (round to nearest).

**Parameters:**
- `x`: Angle in radians

**Returns:** Sine of x

**Range:** [-1, 1]

**Aliases:** `gpga_double_sin()`

---

#### `gpga_sin_ru()`, `gpga_sin_rd()`, `gpga_sin_rz()`

Other rounding modes for sin(x).

---

#### `scs_sin_rn()`, `scs_sin_rd()`, `scs_sin_ru()`, `scs_sin_rz()`

SCS (high-precision) versions of sine.

---

### Cosine

#### `gpga_cos_rn()`

```cpp
inline gpga_double gpga_cos_rn(gpga_double x)
```

**Description:** Computes cos(x) (round to nearest).

**Parameters:**
- `x`: Angle in radians

**Returns:** Cosine of x

**Range:** [-1, 1]

**Aliases:** `gpga_double_cos()`

---

#### `gpga_cos_ru()`, `gpga_cos_rd()`, `gpga_cos_rz()`

Other rounding modes for cos(x).

---

#### `scs_cos_rn()`, `scs_cos_rd()`, `scs_cos_ru()`, `scs_cos_rz()`

SCS (high-precision) versions of cosine.

---

### Tangent

#### `gpga_tan_rn()`

```cpp
inline gpga_double gpga_tan_rn(gpga_double x)
```

**Description:** Computes tan(x) (round to nearest).

**Parameters:**
- `x`: Angle in radians

**Returns:** Tangent of x

**Range:** (-∞, +∞)

**Aliases:** `gpga_double_tan()`

---

#### `gpga_tan_ru()`, `gpga_tan_rd()`, `gpga_tan_rz()`

Other rounding modes for tan(x).

---

#### `scs_tan_rn()`, `scs_tan_rd()`, `scs_tan_ru()`, `scs_tan_rz()`

SCS (high-precision) versions of tangent.

---

### π-Scaled Sine (sin(πx))

#### `gpga_sinpi_rn()`

```cpp
inline gpga_double gpga_sinpi_rn(gpga_double x)
```

**Description:** Computes sin(π × x) (round to nearest).

**Parameters:**
- `x`: Argument (angle in units of π radians)

**Returns:** sin(π × x)

**Notes:** More accurate than `sin(gpga_double_const_pi() * x)` because it avoids π approximation error.

**Examples:**
- sinpi(0.5) = sin(π/2) = 1.0 (exact)
- sinpi(1.0) = sin(π) = 0.0 (exact)

---

#### `gpga_sinpi_rd()`, `gpga_sinpi_ru()`, `gpga_sinpi_rz()`

Other rounding modes for sin(πx).

---

### π-Scaled Cosine (cos(πx))

#### `gpga_cospi_rn()`

```cpp
inline gpga_double gpga_cospi_rn(gpga_double x)
```

**Description:** Computes cos(π × x) (round to nearest).

**Parameters:**
- `x`: Argument (angle in units of π radians)

**Returns:** cos(π × x)

**Examples:**
- cospi(0.5) = cos(π/2) = 0.0 (exact)
- cospi(1.0) = cos(π) = -1.0 (exact)

---

#### `gpga_cospi_rd()`, `gpga_cospi_ru()`, `gpga_cospi_rz()`

Other rounding modes for cos(πx).

---

### π-Scaled Tangent (tan(πx))

#### `gpga_tanpi_rn()`

```cpp
inline gpga_double gpga_tanpi_rn(gpga_double x)
```

**Description:** Computes tan(π × x) (round to nearest).

**Parameters:**
- `x`: Argument (angle in units of π radians)

**Returns:** tan(π × x)

**Examples:**
- tanpi(0.25) = tan(π/4) = 1.0 (exact)
- tanpi(0.5) = tan(π/2) = ±∞

---

#### `gpga_tanpi_rd()`, `gpga_tanpi_ru()`, `gpga_tanpi_rz()`

Other rounding modes for tan(πx).

---

## Inverse Trigonometric Functions

### Arc Sine

#### `gpga_asin_rn()`

```cpp
inline gpga_double gpga_asin_rn(gpga_double x)
```

**Description:** Computes arcsin(x) (round to nearest).

**Parameters:**
- `x`: Argument (must be in [-1, 1])

**Returns:** Angle in radians, range [-π/2, π/2]

**Special Cases:**
- asin(x) = NaN if |x| > 1

**Aliases:** `gpga_double_asin()`

---

#### `gpga_asin_ru()`, `gpga_asin_rd()`, `gpga_asin_rz()`

Other rounding modes for arcsin(x).

---

### Arc Cosine

#### `gpga_acos_rn()`

```cpp
inline gpga_double gpga_acos_rn(gpga_double x)
```

**Description:** Computes arccos(x) (round to nearest).

**Parameters:**
- `x`: Argument (must be in [-1, 1])

**Returns:** Angle in radians, range [0, π]

**Aliases:** `gpga_double_acos()`

---

#### `gpga_acos_ru()`, `gpga_acos_rd()`, `gpga_acos_rz()`

Other rounding modes for arccos(x).

---

### Arc Tangent

#### `gpga_atan_rn()`

```cpp
inline gpga_double gpga_atan_rn(gpga_double x)
```

**Description:** Computes arctan(x) (round to nearest).

**Parameters:**
- `x`: Argument (any real number)

**Returns:** Angle in radians, range (-π/2, π/2)

**Aliases:** `gpga_double_atan()`

---

#### `gpga_atan_rd()`, `gpga_atan_ru()`, `gpga_atan_rz()`

Other rounding modes for arctan(x).

---

#### `scs_atan_rn()`, `scs_atan_rd()`, `scs_atan_ru()`

SCS (high-precision) versions of arctangent.

---

### π-Scaled Arc Sine (arcsin(x)/π)

#### `gpga_asinpi_rn()`

```cpp
inline gpga_double gpga_asinpi_rn(gpga_double x)
```

**Description:** Computes arcsin(x)/π (round to nearest).

**Parameters:**
- `x`: Argument (must be in [-1, 1])

**Returns:** Angle in units of π radians, range [-0.5, 0.5]

**Notes:** Result is in units of π, so multiply by π to get radians.

---

#### `gpga_asinpi_rd()`, `gpga_asinpi_ru()`, `gpga_asinpi_rz()`

Other rounding modes for arcsin(x)/π.

---

### π-Scaled Arc Cosine (arccos(x)/π)

#### `gpga_acospi_rn()`

```cpp
inline gpga_double gpga_acospi_rn(gpga_double x)
```

**Description:** Computes arccos(x)/π (round to nearest).

**Parameters:**
- `x`: Argument (must be in [-1, 1])

**Returns:** Angle in units of π radians, range [0, 1]

---

#### `gpga_acospi_rd()`, `gpga_acospi_ru()`, `gpga_acospi_rz()`

Other rounding modes for arccos(x)/π.

---

### π-Scaled Arc Tangent (arctan(x)/π)

#### `gpga_atanpi_rn()`

```cpp
inline gpga_double gpga_atanpi_rn(gpga_double x)
```

**Description:** Computes arctan(x)/π (round to nearest).

**Parameters:**
- `x`: Argument (any real number)

**Returns:** Angle in units of π radians, range (-0.5, 0.5)

---

#### `gpga_atanpi_rd()`, `gpga_atanpi_ru()`, `gpga_atanpi_rz()`

Other rounding modes for arctan(x)/π.

---

#### `scs_atanpi_rn()`, `scs_atanpi_rd()`, `scs_atanpi_ru()`

SCS (high-precision) versions of arctan(x)/π.

---

## Hyperbolic Functions

### Hyperbolic Sine

#### `gpga_sinh_rn()`

```cpp
inline gpga_double gpga_sinh_rn(gpga_double x)
```

**Description:** Computes sinh(x) = (e^x - e^(-x))/2 (round to nearest).

**Parameters:**
- `x`: Argument (any real number)

**Returns:** Hyperbolic sine of x

**Aliases:** `gpga_double_sinh()`

---

#### `gpga_sinh_ru()`, `gpga_sinh_rd()`, `gpga_sinh_rz()`

Other rounding modes for sinh(x).

---

### Hyperbolic Cosine

#### `gpga_cosh_rn()`

```cpp
inline gpga_double gpga_cosh_rn(gpga_double x)
```

**Description:** Computes cosh(x) = (e^x + e^(-x))/2 (round to nearest).

**Parameters:**
- `x`: Argument (any real number)

**Returns:** Hyperbolic cosine of x

**Range:** [1, +∞)

**Aliases:** `gpga_double_cosh()`

---

#### `gpga_cosh_ru()`, `gpga_cosh_rd()`, `gpga_cosh_rz()`

Other rounding modes for cosh(x).

---

## Double-Double Arithmetic

These functions implement error-free transformations and high-precision arithmetic using pairs or triples of doubles. The "double-double" representation stores a number as the unevaluated sum of two doubles for extended precision.

**Naming Convention:**
- `Add12`: 1 double + 1 double → 2 doubles (high + low)
- `Add22`: 2 doubles + 2 doubles → 2 doubles
- `Mul122`: 1 double × 2 doubles → 2 doubles
- `Add133`: 1 double + 3 doubles → 3 doubles
- "Cond" suffix: Conditional version (requires magnitude ordering)

### Error-Free Addition

#### `Add12()`

```cpp
inline void Add12(thread gpga_double* s, thread gpga_double* r,
                  gpga_double a, gpga_double b)
```

**Description:** Error-free addition: s + r = a + b exactly.

**Parameters:**
- `s`: Pointer to receive sum (high part)
- `r`: Pointer to receive error/remainder (low part)
- `a`: First operand
- `b`: Second operand

**Postcondition:** a + b = (*s) + (*r) exactly

---

#### `Add12Cond()`

```cpp
inline void Add12Cond(thread gpga_double* s, thread gpga_double* r,
                      gpga_double a, gpga_double b)
```

**Description:** Conditional error-free addition (assumes |a| ≥ |b|).

**Precondition:** |a| ≥ |b|

**Notes:** Faster than Add12 when ordering is guaranteed.

---

#### `Fast2Sum()`

```cpp
inline void Fast2Sum(thread gpga_double* s, thread gpga_double* r,
                     gpga_double a, gpga_double b)
```

**Description:** Fast two-sum algorithm (assumes |a| ≥ |b|).

**Precondition:** |a| ≥ |b|

**Notes:** More efficient than Add12 when precondition holds.

---

### Double-Double Addition

#### `Add22()`

```cpp
inline void Add22(thread gpga_double* zh, thread gpga_double* zl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl)
```

**Description:** Adds two double-double numbers.

**Parameters:**
- `zh`, `zl`: Pointers to receive result (high, low)
- `xh`, `xl`: First operand (high, low)
- `yh`, `yl`: Second operand (high, low)

**Notes:** (xh + xl) + (yh + yl) ≈ (zh + zl)

---

#### `Add22Cond()`

Conditional version of Add22 (requires ordering).

---

### Error-Free Multiplication

#### `Mul12()`

```cpp
inline void Mul12(thread gpga_double* rh, thread gpga_double* rl,
                  gpga_double u, gpga_double v)
```

**Description:** Error-free multiplication: rh + rl = u × v exactly.

**Parameters:**
- `rh`: Pointer to receive product (high part)
- `rl`: Pointer to receive error (low part)
- `u`: First operand
- `v`: Second operand

**Postcondition:** u × v = (*rh) + (*rl) exactly

---

### Double-Double Multiplication

#### `Mul22()`

```cpp
inline void Mul22(thread gpga_double* zh, thread gpga_double* zl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl)
```

**Description:** Multiplies two double-double numbers.

**Parameters:**
- `zh`, `zl`: Pointers to receive result (high, low)
- `xh`, `xl`: First operand (high, low)
- `yh`, `yl`: Second operand (high, low)

---

### Mixed Precision Operations

#### `Mul122()`

```cpp
inline void Mul122(thread gpga_double* resh, thread gpga_double* resl,
                   gpga_double a, gpga_double bh, gpga_double bl)
```

**Description:** Multiplies 1 double by 2 doubles → 2 doubles.

**Notes:** Computes a × (bh + bl) → (resh + resl)

---

#### `Add122()`

```cpp
inline void Add122(thread gpga_double* resh, thread gpga_double* resl,
                   gpga_double a, gpga_double bh, gpga_double bl)
```

**Description:** Adds 1 double to 2 doubles → 2 doubles.

**Notes:** Computes a + (bh + bl) → (resh + resl)

---

### Double-Double Division

#### `Div22()`

```cpp
inline void Div22(thread gpga_double* pzh, thread gpga_double* pzl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl)
```

**Description:** Divides two double-double numbers.

**Parameters:**
- `pzh`, `pzl`: Pointers to receive quotient (high, low)
- `xh`, `xl`: Numerator (high, low)
- `yh`, `yl`: Denominator (high, low)

---

### Triple-Double Arithmetic

#### `Renormalize3()`

```cpp
inline void Renormalize3(thread gpga_double* resh,
                         thread gpga_double* resm,
                         thread gpga_double* resl,
                         gpga_double ah, gpga_double am, gpga_double al)
```

**Description:** Renormalizes a triple-double number.

**Notes:** Ensures proper magnitude ordering: |resh| > |resm| > |resl|

---

#### `Add33()`

```cpp
inline void Add33(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl,
                  gpga_double ah, gpga_double am, gpga_double al,
                  gpga_double bh, gpga_double bm, gpga_double bl)
```

**Description:** Adds two triple-double numbers.

**Parameters:**
- `resh`, `resm`, `resl`: Pointers to receive result (high, mid, low)
- `ah`, `am`, `al`: First operand (high, mid, low)
- `bh`, `bm`, `bl`: Second operand (high, mid, low)

---

#### `Mul33()`

```cpp
inline void Mul33(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl,
                  gpga_double ah, gpga_double am, gpga_double al,
                  gpga_double bh, gpga_double bm, gpga_double bl)
```

**Description:** Multiplies two triple-double numbers.

---

### Special Operations

#### `gpga_double_fma()`

```cpp
inline gpga_double gpga_double_fma(gpga_double a, gpga_double b, gpga_double c)
```

**Description:** Fused multiply-add: a × b + c.

**Parameters:**
- `a`: Multiplicand
- `b`: Multiplier
- `c`: Addend

**Returns:** a × b + c (single rounding)

**Notes:** More accurate than separate multiply and add.

---

## Rounding Mode Utilities

These functions convert high-precision (triple-double) intermediate results to final double precision with specified rounding modes.

### `ReturnRoundToNearest3()`

```cpp
inline gpga_double ReturnRoundToNearest3(gpga_double xh, gpga_double xm,
                                         gpga_double xl)
```

**Description:** Rounds triple-double to nearest double.

**Parameters:**
- `xh`, `xm`, `xl`: Triple-double value (high, mid, low)

**Returns:** Correctly rounded double (round to nearest, ties to even)

---

### `ReturnRoundUpwards3()`

```cpp
inline gpga_double ReturnRoundUpwards3(gpga_double xh, gpga_double xm,
                                       gpga_double xl)
```

**Description:** Rounds triple-double upward (towards +∞).

**Returns:** Correctly rounded double (round up)

---

### `ReturnRoundDownwards3()`

```cpp
inline gpga_double ReturnRoundDownwards3(gpga_double xh, gpga_double xm,
                                         gpga_double xl)
```

**Description:** Rounds triple-double downward (towards -∞).

**Returns:** Correctly rounded double (round down)

---

### `ReturnRoundTowardsZero3()`

```cpp
inline gpga_double ReturnRoundTowardsZero3(gpga_double xh, gpga_double xm,
                                           gpga_double xl)
```

**Description:** Rounds triple-double towards zero (truncation).

**Returns:** Correctly rounded double (round towards zero)

---

### Test and Return Functions

#### `gpga_test_and_return_ru()`

```cpp
inline bool gpga_test_and_return_ru(gpga_double yh, gpga_double yl,
                                    gpga_double x, thread gpga_double* out)
```

**Description:** Tests if rounding up is needed and returns result.

**Parameters:**
- `yh`, `yl`: Computed result (high, low)
- `x`: Original argument
- `out`: Pointer to receive rounded result

**Returns:** `true` if rounding was applied

---

#### `gpga_test_and_return_rd()`, `gpga_test_and_return_rz()`

Similar functions for round down and round towards zero.

---

## SCS (Software Carried Significand) Library

SCS is a multiple-precision representation using an array of digits in a high radix. It's used for intermediate calculations requiring precision beyond double-double.

### SCS Structure

```cpp
struct scs {
  uint h_word;           // Holds exponent and sign
  uint index;            // Index of most significant digit
  uint exception;        // Exception flags
  ulong array[8];        // Digit array
};
```

### SCS Constants and Utilities

#### `scs_radix_one_double()`

```cpp
inline gpga_double scs_radix_one_double()
```

**Description:** Returns 1.0 in SCS radix representation.

---

### SCS Conversion Functions

#### `scs_set_d()`

```cpp
inline void scs_set_d(scs_ptr result, gpga_double x)
```

**Description:** Converts a double to SCS format.

**Parameters:**
- `result`: Pointer to SCS structure to receive result
- `x`: Double precision value to convert

---

#### `scs_get_d()`

```cpp
inline void scs_get_d(thread gpga_double* result, scs_ptr x)
```

**Description:** Converts SCS to double (default rounding).

**Parameters:**
- `result`: Pointer to receive double result
- `x`: SCS value to convert

---

#### `scs_get_d_nearest()`

```cpp
inline void scs_get_d_nearest(thread gpga_double* result, scs_ptr x)
```

**Description:** Converts SCS to double (round to nearest).

---

#### `scs_get_d_pinf()`, `scs_get_d_minf()`, `scs_get_d_zero()`

Convert SCS to double with directed rounding (towards +∞, -∞, or zero).

---

### SCS Arithmetic

#### `scs_add()`

```cpp
inline void scs_add(scs_ptr result, scs_ptr x, scs_ptr y)
```

**Description:** Adds two SCS numbers.

**Parameters:**
- `result`: Pointer to SCS result
- `x`: First operand
- `y`: Second operand

**Notes:** Multiple overloads accept combinations of `scs_ptr` and `scs_const_ptr`.

---

#### `scs_sub()`

```cpp
inline void scs_sub(scs_ptr result, scs_ptr x, scs_ptr y)
```

**Description:** Subtracts two SCS numbers.

---

#### `scs_mul()`

```cpp
inline void scs_mul(scs_ptr result, scs_ptr x, scs_ptr y)
```

**Description:** Multiplies two SCS numbers.

---

#### `scs_div()`

```cpp
inline void scs_div(scs_ptr result, scs_ptr x, scs_ptr y)
```

**Description:** Divides two SCS numbers.

---

#### `scs_fma()`

```cpp
inline void scs_fma(scs_ptr result, scs_ptr x, scs_ptr y, scs_ptr z)
```

**Description:** Fused multiply-add for SCS: x × y + z.

---

### SCS Utilities

#### `scs_zero()`

```cpp
inline void scs_zero(scs_ptr result)
```

**Description:** Sets an SCS value to zero.

---

#### `scs_renorm()`

```cpp
inline void scs_renorm(scs_ptr result)
```

**Description:** Renormalizes an SCS value (adjusts digit array).

**Notes:** Call after arithmetic operations to maintain proper form.

---

## Rounding Modes Reference

The library supports all four IEEE-754 rounding modes through function name suffixes:

### `_rn` - Round to Nearest (ties to even)

**Default mode.** Rounds to the nearest representable value. When exactly halfway between two values, rounds to the one with an even least significant bit.

**Examples:**
- 2.5 → 2.0 (even)
- 3.5 → 4.0 (even)
- 2.7 → 3.0

**Use cases:**
- Default for most applications
- Minimizes rounding bias over many operations
- Required by most numerical algorithms

---

### `_ru` - Round Up (towards +∞)

Rounds towards positive infinity. Always rounds up if not exact.

**Examples:**
- 2.1 → 3.0
- -2.9 → -2.0
- 2.0 → 2.0 (exact)

**Use cases:**
- Interval arithmetic (upper bounds)
- Conservative overestimation
- Guaranteed safe upper limits

---

### `_rd` - Round Down (towards -∞)

Rounds towards negative infinity. Always rounds down if not exact.

**Examples:**
- 2.9 → 2.0
- -2.1 → -3.0
- 2.0 → 2.0 (exact)

**Use cases:**
- Interval arithmetic (lower bounds)
- Conservative underestimation
- Guaranteed safe lower limits

---

### `_rz` - Round towards Zero (truncation)

Rounds towards zero, discarding fractional parts.

**Examples:**
- 2.9 → 2.0
- -2.9 → -2.0
- 2.0 → 2.0 (exact)

**Use cases:**
- Integer conversion
- Matching truncation semantics
- Specific numerical methods

---

## Usage Examples

### Basic Arithmetic

```cpp
#include <gpga_real.h>

// Create doubles from integers
gpga_double a = gpga_double_from_s32(42);
gpga_double b = gpga_double_from_s32(17);

// Arithmetic operations
gpga_double sum = gpga_double_add(a, b);      // 59.0
gpga_double diff = gpga_double_sub(a, b);     // 25.0
gpga_double prod = gpga_double_mul(a, b);     // 714.0
gpga_double quot = gpga_double_div(a, b);     // 2.470588...

// Comparisons
if (gpga_double_gt(a, b)) {
    // a > b is true
}

// Convert back to integer
int result = gpga_double_to_s32(sum);  // 59
```

---

### Trigonometry

```cpp
// Compute sin(π/4) using pi-scaled function (more accurate)
gpga_double quarter = gpga_double_const_inv2();
quarter = gpga_double_mul(quarter, gpga_double_const_inv2()); // 0.25
gpga_double sin_pi_4 = gpga_sinpi_rn(quarter);  // ≈ 0.707107...

// Traditional sine
gpga_double pi_4 = gpga_double_div(gpga_double_const_pi(),
                                    gpga_double_from_s32(4));
gpga_double sin_val = gpga_sin_rn(pi_4);

// Inverse trig
gpga_double x = gpga_double_const_inv2();  // 0.5
gpga_double angle = gpga_asin_rn(x);       // π/6 ≈ 0.5236...
```

---

### Exponential and Logarithm

```cpp
// Natural exponential
gpga_double two = gpga_double_const_two();
gpga_double e_squared = gpga_exp_rn(two);  // e² ≈ 7.389...

// Logarithm
gpga_double ten = gpga_double_from_s32(10);
gpga_double ln10 = gpga_log_rn(ten);       // ln(10) ≈ 2.302...
gpga_double log10_val = gpga_log10_rn(ten); // log₁₀(10) = 1.0

// Power
gpga_double three = gpga_double_from_s32(3);
gpga_double eight = gpga_pow_rn(two, three); // 2³ = 8.0
```

---

### Interval Arithmetic with Directed Rounding

```cpp
// Compute bounds for 1/3
gpga_double one = gpga_double_const_one();
gpga_double three = gpga_double_from_s32(3);

// Get interval [lower, upper] that contains exact value
gpga_double lower = gpga_div_rd(one, three);  // Round down
gpga_double upper = gpga_div_ru(one, three);  // Round up

// Now: lower <= 1/3 <= upper (guaranteed)
```

---

### High-Precision Intermediate Calculations

```cpp
// Using double-double for extended precision
gpga_double a = /* some value */;
gpga_double b = /* some value */;

gpga_double prod_hi, prod_lo;
Mul12(&prod_hi, &prod_lo, a, b);  // Error-free multiplication

// prod_hi + prod_lo = a × b exactly (within double-double precision)

// Can continue with high-precision operations
gpga_double c = /* another value */;
gpga_double result_hi, result_lo;
Add122(&result_hi, &result_lo, c, prod_hi, prod_lo);
```

---

## Performance Considerations

1. **Choose Appropriate Precision**: Use basic operations when possible; reserve double-double and SCS for critical calculations.

2. **Rounding Mode Selection**: Round-to-nearest is typically fastest as it's the default mode.

3. **Pi-scaled Functions**: Use `sinpi`, `cospi`, etc., instead of multiplying by π for better accuracy and sometimes better performance.

4. **Integer Exponents**: Use `gpga_double_pow_int()` for integer powers instead of general `gpga_pow_rn()`.

5. **Special Value Checks**: Early testing for special values (zero, infinity, NaN) can improve performance in many cases.

---

## Accuracy Guarantees

The library implements **correctly-rounded** functions compatible with CRLibM (Correctly Rounded mathematical library):

- **Basic Arithmetic** (+, -, ×, ÷): Correctly rounded per IEEE-754
- **Square Root**: Correctly rounded
- **Transcendental Functions**: Most are correctly rounded to nearest (within 0.5 ULP)
- **Directed Rounding Modes**: Guaranteed correct rounding direction

**ULP (Unit in the Last Place)**: The error bound for most functions is ≤ 0.5 ULP in round-to-nearest mode, and ≤ 1 ULP in directed rounding modes.

---

## Limitations and Notes

1. **Performance**: Software floating-point is significantly slower than hardware. Use hardware double when available.

2. **Denormal Numbers**: Fully supported but may have reduced performance.

3. **NaN Propagation**: NaN handling follows IEEE-754 but quiets signaling NaNs.

4. **Thread Safety**: Functions use `thread` keyword for pointer parameters, indicating thread-local storage in Metal.

5. **Large Arguments**: Range reduction for trigonometric functions handles arbitrarily large arguments but with reduced accuracy for extremely large values.

---

## Related Documentation

- IEEE-754 Standard for Floating-Point Arithmetic
- CRLibM (Correctly Rounded mathematical library)
- Metal Shading Language Specification

---

## Summary

The GPGA Real library provides a complete, correctly-rounded, IEEE-754 compliant double-precision floating-point implementation in software. With over 346 functions spanning basic arithmetic, transcendental functions, and high-precision utilities, it enables accurate mathematical computation on GPU hardware without native double-precision support.

**Total Functions: ~346**

**Function Categories:**
- IEEE-754 Helpers: 18
- Mathematical Constants: 24
- Arithmetic Operations: 6
- Type Conversions: 9
- Comparison: 5
- Exponential/Logarithm: 45
- Trigonometric: 56
- Inverse Trigonometric: 36
- Hyperbolic: 16
- Power/Square Root: 15
- Double-Double Arithmetic: 35+
- SCS Library: 42+
- Utilities: 20+

---

*Generated from include/gpga_real.h - MetalFPGA Project*
