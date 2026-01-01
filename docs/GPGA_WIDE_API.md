# GPGA Wide Integer API Documentation

## Overview

The `gpga_wide.h` header provides a comprehensive library for handling arbitrary-width integers (greater than 64 bits) in Metal Shading Language (MSL). This library uses a macro-based code generation system to create type-safe multi-word integer operations for specific widths.

**Key Features:**
- Support for integers wider than 64 bits (e.g., 128-bit, 256-bit, etc.)
- Full suite of arithmetic operations (add, subtract, multiply, divide, modulo)
- Bitwise operations (AND, OR, XOR, NOT, shifts)
- Comparison operations (signed and unsigned)
- Reduction operations (AND, OR, XOR)
- Bit manipulation utilities
- Sign extension and zero extension
- Type conversion and resizing between different widths
- Four-state logic support integration

**Implementation Strategy:**
The library uses C preprocessor macros (`GPGA_WIDE_DEFINE`, `GPGA_WIDE_DEFINE_RESIZE`, `GPGA_WIDE_DEFINE_FS`) to generate specialized functions for specific bit widths. Each width gets its own struct type and complete function set, enabling type-safe operations at compile time.

---

## Table of Contents

1. [Core Macro Definitions](#core-macro-definitions)
2. [Data Structures](#data-structures)
3. [Construction Functions](#construction-functions)
4. [Type Conversion Functions](#type-conversion-functions)
5. [Bit Manipulation Functions](#bit-manipulation-functions)
6. [Utility Functions](#utility-functions)
7. [Bitwise Operations](#bitwise-operations)
8. [Arithmetic Operations](#arithmetic-operations)
9. [Shift Operations](#shift-operations)
10. [Comparison Operations](#comparison-operations)
11. [Reduction Operations](#reduction-operations)
12. [Advanced Operations](#advanced-operations)
13. [Resizing and Extension](#resizing-and-extension)
14. [Four-State Logic Support](#four-state-logic-support)

---

## Core Macro Definitions

### `GPGA_WIDE_DEFINE(width, words, last_mask)`

**Purpose**: Master macro that generates a complete set of wide integer operations for a specific bit width.

**Parameters**:
- `width`: The total number of bits (e.g., 128, 256)
- `words`: Number of 64-bit words needed to represent the value (width / 64, rounded up)
- `last_mask`: Bitmask for the most significant word to ensure unused bits are zeroed

**Generated Types**:
- `GpgaWide{width}`: Struct containing an array of `ulong` words

**Generated Functions**: All functions listed in the following sections with `_{width}` suffix

**Example Usage**:
```cpp
// Generate 128-bit wide integer support
GPGA_WIDE_DEFINE(128, 2, 0xFFFFFFFFFFFFFFFFul)

// Generate 100-bit wide integer support (2 words, last word uses only 36 bits)
GPGA_WIDE_DEFINE(100, 2, 0x0000000FFFFFFFFFul)
```

---

### `GPGA_WIDE_DEFINE_RESIZE(dst, src, dst_words, src_words, dst_last_mask, src_mod)`

**Purpose**: Generates functions to convert between different wide integer widths.

**Parameters**:
- `dst`: Destination bit width
- `src`: Source bit width
- `dst_words`: Number of words in destination
- `src_words`: Number of words in source
- `dst_last_mask`: Bitmask for destination's last word
- `src_mod`: Number of bits used in source's last word (src % 64)

**Generated Functions**:
- `gpga_wide_resize_{dst}_from_{src}()`: Zero-extension resize
- `gpga_wide_sext_{dst}_from_{src}()`: Sign-extension resize

---

### `GPGA_WIDE_DEFINE_FS(width)`

**Purpose**: Defines a four-state logic structure for wide integers.

**Parameters**:
- `width`: The bit width

**Generated Types**:
- `GpgaWideFs{width}`: Struct with `val` and `xz` fields for four-state logic

---

## Data Structures

### `GpgaWide{width}`

```cpp
struct GpgaWide{width} { ulong w[words]; };
```

**Description**: Represents an unsigned integer of the specified width using an array of 64-bit words. Words are stored in little-endian order (word 0 contains the least significant 64 bits).

**Fields**:
- `w`: Array of `ulong` words containing the integer value

**Example**:
```cpp
GpgaWide128 value;  // 128-bit integer stored in 2 words
value.w[0] = 0x123456789ABCDEF0ul;  // Low 64 bits
value.w[1] = 0xFEDCBA9876543210ul;  // High 64 bits
```

---

### `GpgaWideFs{width}`

```cpp
struct GpgaWideFs{width} { GpgaWide{width} val; GpgaWide{width} xz; };
```

**Description**: Represents a wide integer with four-state logic support (0, 1, X, Z).

**Fields**:
- `val`: The bit values for known 0s and 1s
- `xz`: Marks which bits are unknown (X) or high-impedance (Z)

---

## Construction Functions

### `gpga_wide_zero_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_zero_{width}()
```

**Description**: Creates a wide integer with all bits set to zero.

**Returns**: `GpgaWide{width}` with value 0.

**Example**:
```cpp
GpgaWide128 zero = gpga_wide_zero_128();
// zero.w[0] == 0, zero.w[1] == 0
```

---

### `gpga_wide_mask_const_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_mask_const_{width}()
```

**Description**: Creates a wide integer with all valid bits set to 1 (respecting the width's last_mask).

**Returns**: `GpgaWide{width}` with all bits in the valid range set to 1.

**Notes**: For widths that aren't multiples of 64, the most significant word is properly masked.

**Example**:
```cpp
GpgaWide100 mask = gpga_wide_mask_const_100();
// mask.w[0] == 0xFFFFFFFFFFFFFFFFul
// mask.w[1] == 0x0000000FFFFFFFFFul  // Only 36 bits set
```

---

### `gpga_wide_from_u64_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_from_u64_{width}(ulong value)
```

**Description**: Constructs a wide integer from a 64-bit unsigned value.

**Parameters**:
- `value`: 64-bit unsigned integer to convert

**Returns**: `GpgaWide{width}` with the lower 64 bits set to `value` and upper bits zeroed.

**Example**:
```cpp
GpgaWide128 val = gpga_wide_from_u64_128(0x123456789ABCDEFul);
// val.w[0] == 0x123456789ABCDEFul
// val.w[1] == 0
```

---

## Type Conversion Functions

### `gpga_wide_to_u64_{width}()`

```cpp
inline ulong gpga_wide_to_u64_{width}(GpgaWide{width} v)
```

**Description**: Extracts the lower 64 bits of a wide integer as a standard `ulong`.

**Parameters**:
- `v`: Wide integer to convert

**Returns**: Lower 64 bits of the wide integer.

**Notes**: Higher bits are discarded. This is a truncating conversion.

**Example**:
```cpp
GpgaWide128 val;
val.w[0] = 0x123456789ABCDEFul;
val.w[1] = 0xFEDCBA987654321ul;
ulong lower = gpga_wide_to_u64_128(val);
// lower == 0x123456789ABCDEFul
```

---

### `gpga_wide_sext_from_u64_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_sext_from_u64_{width}(ulong value, uint src_width)
```

**Description**: Constructs a wide integer from a smaller unsigned value with sign extension.

**Parameters**:
- `value`: 64-bit value containing the source integer
- `src_width`: The actual width of the source value (1-64 bits)

**Returns**: `GpgaWide{width}` with sign-extended value.

**Notes**: If the sign bit (bit at position `src_width-1`) is set, all higher bits are set to 1; otherwise, they remain 0.

**Example**:
```cpp
// Sign-extend an 8-bit value -1 (0xFF) to 128 bits
GpgaWide128 val = gpga_wide_sext_from_u64_128(0xFFul, 8u);
// val.w[0] == 0xFFFFFFFFFFFFFFFFul
// val.w[1] == 0xFFFFFFFFFFFFFFFFul
```

---

## Bit Manipulation Functions

### `gpga_wide_get_bit_{width}()`

```cpp
inline uint gpga_wide_get_bit_{width}(GpgaWide{width} v, uint idx)
```

**Description**: Extracts a single bit from a wide integer.

**Parameters**:
- `v`: Wide integer to read from
- `idx`: Bit index (0 = LSB, width-1 = MSB)

**Returns**: 0 or 1 as `uint`. Returns 0 if `idx` is out of range.

**Example**:
```cpp
GpgaWide128 val = gpga_wide_from_u64_128(0b1010ul);
uint bit0 = gpga_wide_get_bit_128(val, 0u);  // 0
uint bit1 = gpga_wide_get_bit_128(val, 1u);  // 1
uint bit2 = gpga_wide_get_bit_128(val, 2u);  // 0
uint bit3 = gpga_wide_get_bit_128(val, 3u);  // 1
```

---

### `gpga_wide_set_bit_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_set_bit_{width}(GpgaWide{width} v, uint idx, uint bit)
```

**Description**: Sets or clears a single bit in a wide integer.

**Parameters**:
- `v`: Wide integer to modify
- `idx`: Bit index to modify (0 = LSB, width-1 = MSB)
- `bit`: New bit value (0 or non-zero)

**Returns**: Modified wide integer. If `idx` is out of range, returns `v` unchanged.

**Example**:
```cpp
GpgaWide128 val = gpga_wide_zero_128();
val = gpga_wide_set_bit_128(val, 5u, 1u);   // Set bit 5
val = gpga_wide_set_bit_128(val, 100u, 1u); // Set bit 100
```

---

### `gpga_wide_set_word_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_set_word_{width}(GpgaWide{width} v, uint word, ulong value)
```

**Description**: Sets an entire 64-bit word within the wide integer.

**Parameters**:
- `v`: Wide integer to modify
- `word`: Word index (0 = least significant word)
- `value`: 64-bit value to set

**Returns**: Modified wide integer. If `word` index is out of range, returns `v` unchanged.

**Notes**: Automatically masks the last word if necessary to respect the width constraint.

**Example**:
```cpp
GpgaWide256 val = gpga_wide_zero_256();
val = gpga_wide_set_word_256(val, 0u, 0x123456789ABCDEFul);
val = gpga_wide_set_word_256(val, 1u, 0xFEDCBA987654321ul);
```

---

### `gpga_wide_signbit_{width}()`

```cpp
inline uint gpga_wide_signbit_{width}(GpgaWide{width} v)
```

**Description**: Extracts the most significant bit (sign bit) of a wide integer.

**Parameters**:
- `v`: Wide integer to examine

**Returns**: 0 or 1 as `uint`.

**Notes**: For signed integer interpretation, a 1 indicates a negative value.

**Example**:
```cpp
GpgaWide128 pos = gpga_wide_from_u64_128(100ul);
GpgaWide128 neg = gpga_wide_mask_const_128();  // All 1s
uint sign_pos = gpga_wide_signbit_128(pos);  // 0
uint sign_neg = gpga_wide_signbit_128(neg);  // 1
```

---

## Utility Functions

### `gpga_wide_mask_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_mask_{width}(GpgaWide{width} v)
```

**Description**: Ensures the wide integer is properly masked to its declared width by clearing unused bits in the most significant word.

**Parameters**:
- `v`: Wide integer to mask

**Returns**: Masked wide integer with all bits outside the valid range set to 0.

**Notes**: This is primarily used internally to maintain invariants after operations.

---

### `gpga_wide_any_{width}()`

```cpp
inline bool gpga_wide_any_{width}(GpgaWide{width} v)
```

**Description**: Tests whether any bit in the wide integer is set.

**Parameters**:
- `v`: Wide integer to test

**Returns**: `true` if at least one bit is 1, `false` if all bits are 0.

**Example**:
```cpp
GpgaWide128 zero = gpga_wide_zero_128();
GpgaWide128 val = gpga_wide_from_u64_128(1ul);
bool any_zero = gpga_wide_any_128(zero);  // false
bool any_val = gpga_wide_any_128(val);    // true
```

---

### `gpga_wide_select_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_select_{width}(bool cond, GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Selects between two wide integers based on a condition.

**Parameters**:
- `cond`: Boolean condition
- `a`: Value to return if `cond` is true
- `b`: Value to return if `cond` is false

**Returns**: `a` if `cond` is true, otherwise `b`.

**Notes**: Equivalent to the ternary operator `cond ? a : b`.

**Example**:
```cpp
GpgaWide128 x = gpga_wide_from_u64_128(10ul);
GpgaWide128 y = gpga_wide_from_u64_128(20ul);
GpgaWide128 max = gpga_wide_select_128(
    gpga_wide_gt_u_128(x, y), x, y
);  // Returns y
```

---

## Bitwise Operations

### `gpga_wide_not_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_not_{width}(GpgaWide{width} a)
```

**Description**: Performs bitwise NOT operation (one's complement).

**Parameters**:
- `a`: Wide integer to invert

**Returns**: Wide integer with all bits flipped (0→1, 1→0).

**Example**:
```cpp
GpgaWide128 val = gpga_wide_from_u64_128(0xF0F0ul);
GpgaWide128 inv = gpga_wide_not_128(val);
// inv.w[0] == 0xFFFFFFFFFFFF0F0Ful (lower 64 bits)
```

---

### `gpga_wide_and_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_and_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs bitwise AND operation.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: Wide integer with each bit being `a[i] & b[i]`.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(0b1100ul);
GpgaWide128 b = gpga_wide_from_u64_128(0b1010ul);
GpgaWide128 result = gpga_wide_and_128(a, b);
// result == 0b1000
```

---

### `gpga_wide_or_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_or_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs bitwise OR operation.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: Wide integer with each bit being `a[i] | b[i]`.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(0b1100ul);
GpgaWide128 b = gpga_wide_from_u64_128(0b1010ul);
GpgaWide128 result = gpga_wide_or_128(a, b);
// result == 0b1110
```

---

### `gpga_wide_xor_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_xor_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs bitwise XOR operation.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: Wide integer with each bit being `a[i] ^ b[i]`.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(0b1100ul);
GpgaWide128 b = gpga_wide_from_u64_128(0b1010ul);
GpgaWide128 result = gpga_wide_xor_128(a, b);
// result == 0b0110
```

---

## Arithmetic Operations

### `gpga_wide_add_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_add_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs addition with carry propagation across all words.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `a + b` modulo 2^width (overflow wraps around).

**Notes**: Carry is propagated correctly across word boundaries.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(0xFFFFFFFFFFFFFFFFul);
GpgaWide128 b = gpga_wide_from_u64_128(1ul);
GpgaWide128 sum = gpga_wide_add_128(a, b);
// sum.w[0] == 0, sum.w[1] == 1 (carry propagated)
```

---

### `gpga_wide_sub_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_sub_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs subtraction with borrow propagation across all words.

**Parameters**:
- `a`: Minuend
- `b`: Subtrahend

**Returns**: `a - b` modulo 2^width (underflow wraps around).

**Notes**: Borrow is propagated correctly across word boundaries.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(1000ul);
GpgaWide128 b = gpga_wide_from_u64_128(300ul);
GpgaWide128 diff = gpga_wide_sub_128(a, b);
// diff == 700
```

---

### `gpga_wide_mul_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_mul_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Performs multiplication using shift-and-add algorithm.

**Parameters**:
- `a`: Multiplicand
- `b`: Multiplier

**Returns**: `a * b` modulo 2^width (overflow discards high bits).

**Notes**: Implements grade-school binary multiplication. For large widths, this operation can be expensive.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(123ul);
GpgaWide128 b = gpga_wide_from_u64_128(456ul);
GpgaWide128 product = gpga_wide_mul_128(a, b);
// product == 56088
```

---

### `gpga_wide_div_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_div_{width}(GpgaWide{width} num, GpgaWide{width} den)
```

**Description**: Performs unsigned integer division.

**Parameters**:
- `num`: Dividend (numerator)
- `den`: Divisor (denominator)

**Returns**: Quotient `num / den`. Returns 0 if `den` is 0.

**Notes**: Implements long division algorithm. Does not trap on division by zero.

**Example**:
```cpp
GpgaWide128 num = gpga_wide_from_u64_128(100ul);
GpgaWide128 den = gpga_wide_from_u64_128(7ul);
GpgaWide128 quot = gpga_wide_div_128(num, den);
// quot == 14
```

---

### `gpga_wide_mod_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_mod_{width}(GpgaWide{width} num, GpgaWide{width} den)
```

**Description**: Performs unsigned modulo operation (remainder after division).

**Parameters**:
- `num`: Dividend
- `den`: Divisor

**Returns**: Remainder `num % den`. Returns 0 if `den` is 0.

**Example**:
```cpp
GpgaWide128 num = gpga_wide_from_u64_128(100ul);
GpgaWide128 den = gpga_wide_from_u64_128(7ul);
GpgaWide128 rem = gpga_wide_mod_128(num, den);
// rem == 2 (because 100 = 14*7 + 2)
```

---

## Shift Operations

### `gpga_wide_shl_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_shl_{width}(GpgaWide{width} a, uint shift)
```

**Description**: Performs logical left shift.

**Parameters**:
- `a`: Value to shift
- `shift`: Number of bit positions to shift left

**Returns**: `a << shift`. Bits shifted in from the right are 0. If `shift >= width`, returns 0.

**Notes**: Correctly handles shifts across word boundaries.

**Example**:
```cpp
GpgaWide128 val = gpga_wide_from_u64_128(1ul);
GpgaWide128 shifted = gpga_wide_shl_128(val, 65u);
// shifted.w[0] == 0, shifted.w[1] == 2
```

---

### `gpga_wide_shr_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_shr_{width}(GpgaWide{width} a, uint shift)
```

**Description**: Performs logical right shift (unsigned).

**Parameters**:
- `a`: Value to shift
- `shift`: Number of bit positions to shift right

**Returns**: `a >> shift`. Bits shifted in from the left are 0. If `shift >= width`, returns 0.

**Notes**: This is a logical shift; the sign bit is not preserved.

**Example**:
```cpp
GpgaWide128 val = gpga_wide_from_u64_128(0x8000000000000000ul);
GpgaWide128 shifted = gpga_wide_shr_128(val, 4u);
// shifted.w[0] == 0x0800000000000000ul
```

---

### `gpga_wide_sar_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_sar_{width}(GpgaWide{width} a, uint shift)
```

**Description**: Performs arithmetic right shift (signed).

**Parameters**:
- `a`: Value to shift
- `shift`: Number of bit positions to shift right

**Returns**: `a >> shift` with sign extension. If the sign bit is 1, bits shifted in from the left are 1; otherwise 0.

**Notes**: Preserves the sign bit, making this suitable for signed division by powers of 2.

**Example**:
```cpp
// Negative value (MSB set)
GpgaWide128 neg = gpga_wide_mask_const_128();
GpgaWide128 shifted = gpga_wide_sar_128(neg, 4u);
// All bits still set to 1 (sign-extended)
```

---

## Comparison Operations

### `gpga_wide_eq_{width}()`

```cpp
inline bool gpga_wide_eq_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Tests for equality.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a == b`, `false` otherwise.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(42ul);
GpgaWide128 b = gpga_wide_from_u64_128(42ul);
GpgaWide128 c = gpga_wide_from_u64_128(99ul);
bool eq1 = gpga_wide_eq_128(a, b);  // true
bool eq2 = gpga_wide_eq_128(a, c);  // false
```

---

### `gpga_wide_ne_{width}()`

```cpp
inline bool gpga_wide_ne_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Tests for inequality.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a != b`, `false` otherwise.

---

### `gpga_wide_lt_u_{width}()`

```cpp
inline bool gpga_wide_lt_u_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Unsigned less-than comparison.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a < b` (unsigned), `false` otherwise.

**Notes**: Compares from most significant word to least significant word.

**Example**:
```cpp
GpgaWide128 a = gpga_wide_from_u64_128(10ul);
GpgaWide128 b = gpga_wide_from_u64_128(20ul);
bool lt = gpga_wide_lt_u_128(a, b);  // true
```

---

### `gpga_wide_gt_u_{width}()`

```cpp
inline bool gpga_wide_gt_u_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Unsigned greater-than comparison.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a > b` (unsigned), `false` otherwise.

---

### `gpga_wide_le_u_{width}()`

```cpp
inline bool gpga_wide_le_u_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Unsigned less-than-or-equal comparison.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a <= b` (unsigned), `false` otherwise.

---

### `gpga_wide_ge_u_{width}()`

```cpp
inline bool gpga_wide_ge_u_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Unsigned greater-than-or-equal comparison.

**Parameters**:
- `a`: First operand
- `b`: Second operand

**Returns**: `true` if `a >= b` (unsigned), `false` otherwise.

---

### `gpga_wide_lt_s_{width}()`

```cpp
inline bool gpga_wide_lt_s_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Signed less-than comparison.

**Parameters**:
- `a`: First operand (interpreted as signed)
- `b`: Second operand (interpreted as signed)

**Returns**: `true` if `a < b` (signed), `false` otherwise.

**Notes**: Uses the MSB as the sign bit. Negative values (MSB=1) are less than positive values (MSB=0).

**Example**:
```cpp
GpgaWide128 pos = gpga_wide_from_u64_128(10ul);
GpgaWide128 neg = gpga_wide_mask_const_128();  // All 1s = -1 in two's complement
bool lt = gpga_wide_lt_s_128(neg, pos);  // true (-1 < 10)
```

---

### `gpga_wide_gt_s_{width}()`

```cpp
inline bool gpga_wide_gt_s_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Signed greater-than comparison.

**Parameters**:
- `a`: First operand (interpreted as signed)
- `b`: Second operand (interpreted as signed)

**Returns**: `true` if `a > b` (signed), `false` otherwise.

---

### `gpga_wide_le_s_{width}()`

```cpp
inline bool gpga_wide_le_s_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Signed less-than-or-equal comparison.

**Parameters**:
- `a`: First operand (interpreted as signed)
- `b`: Second operand (interpreted as signed)

**Returns**: `true` if `a <= b` (signed), `false` otherwise.

---

### `gpga_wide_ge_s_{width}()`

```cpp
inline bool gpga_wide_ge_s_{width}(GpgaWide{width} a, GpgaWide{width} b)
```

**Description**: Signed greater-than-or-equal comparison.

**Parameters**:
- `a`: First operand (interpreted as signed)
- `b`: Second operand (interpreted as signed)

**Returns**: `true` if `a >= b` (signed), `false` otherwise.

---

## Reduction Operations

### `gpga_wide_red_and_{width}()`

```cpp
inline uint gpga_wide_red_and_{width}(GpgaWide{width} a)
```

**Description**: Reduction AND - tests if all bits are 1.

**Parameters**:
- `a`: Wide integer to test

**Returns**: 1 if all bits in `a` are 1, otherwise 0.

**Notes**: Equivalent to checking if `a` equals the all-ones mask.

**Example**:
```cpp
GpgaWide128 all_ones = gpga_wide_mask_const_128();
GpgaWide128 partial = gpga_wide_from_u64_128(0xFFFFFFFFFFFFFFFFul);
uint r1 = gpga_wide_red_and_128(all_ones);  // 1
uint r2 = gpga_wide_red_and_128(partial);   // 0 (upper word is 0)
```

---

### `gpga_wide_red_or_{width}()`

```cpp
inline uint gpga_wide_red_or_{width}(GpgaWide{width} a)
```

**Description**: Reduction OR - tests if any bit is 1.

**Parameters**:
- `a`: Wide integer to test

**Returns**: 1 if at least one bit in `a` is 1, otherwise 0.

**Notes**: Equivalent to `gpga_wide_any_{width}()` but returns `uint` instead of `bool`.

**Example**:
```cpp
GpgaWide128 zero = gpga_wide_zero_128();
GpgaWide128 one = gpga_wide_from_u64_128(1ul);
uint r1 = gpga_wide_red_or_128(zero);  // 0
uint r2 = gpga_wide_red_or_128(one);   // 1
```

---

### `gpga_wide_red_xor_{width}()`

```cpp
inline uint gpga_wide_red_xor_{width}(GpgaWide{width} a)
```

**Description**: Reduction XOR - computes the parity of all bits.

**Parameters**:
- `a`: Wide integer to test

**Returns**: 1 if an odd number of bits are 1, otherwise 0.

**Notes**: Useful for parity checking and error detection.

**Example**:
```cpp
GpgaWide128 val1 = gpga_wide_from_u64_128(0b111ul);  // 3 bits set
GpgaWide128 val2 = gpga_wide_from_u64_128(0b1111ul); // 4 bits set
uint r1 = gpga_wide_red_xor_128(val1);  // 1 (odd parity)
uint r2 = gpga_wide_red_xor_128(val2);  // 0 (even parity)
```

---

## Advanced Operations

### `gpga_wide_pow_u_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_pow_u_{width}(GpgaWide{width} base, GpgaWide{width} exp)
```

**Description**: Computes unsigned integer exponentiation using binary exponentiation algorithm.

**Parameters**:
- `base`: Base value
- `exp`: Exponent (only lower 64 bits are used)

**Returns**: `base ^ exp` modulo 2^width.

**Notes**: Uses fast exponentiation by squaring. Only the lower 64 bits of `exp` are considered.

**Example**:
```cpp
GpgaWide128 base = gpga_wide_from_u64_128(2ul);
GpgaWide128 exp = gpga_wide_from_u64_128(10ul);
GpgaWide128 result = gpga_wide_pow_u_128(base, exp);
// result == 1024
```

---

### `gpga_wide_pow_s_{width}()`

```cpp
inline GpgaWide{width} gpga_wide_pow_s_{width}(GpgaWide{width} base, GpgaWide{width} exp)
```

**Description**: Computes signed integer exponentiation.

**Parameters**:
- `base`: Base value
- `exp`: Exponent (interpreted as signed, only lower 64 bits used)

**Returns**: `base ^ exp` if `exp >= 0`, otherwise 0.

**Notes**: Negative exponents return 0 (integer division semantics). Delegates to `gpga_wide_pow_u_{width}()` for non-negative exponents.

---

## Resizing and Extension

### `gpga_wide_resize_{dst}_from_{src}()`

```cpp
inline GpgaWide{dst} gpga_wide_resize_{dst}_from_{src}(GpgaWide{src} v)
```

**Description**: Converts a wide integer from one width to another with zero extension.

**Parameters**:
- `v`: Source wide integer

**Returns**: Destination wide integer with zero-extended or truncated value.

**Notes**:
- If `dst > src`: Upper bits are filled with 0 (zero extension)
- If `dst < src`: Upper bits are discarded (truncation)
- If `dst == src`: Returns a copy

**Example**:
```cpp
GpgaWide128 val128 = gpga_wide_from_u64_128(0x123456789ABCDEFul);
GpgaWide256 val256 = gpga_wide_resize_256_from_128(val128);
// val256.w[0..1] copied from val128, val256.w[2..3] = 0
```

---

### `gpga_wide_sext_{dst}_from_{src}()`

```cpp
inline GpgaWide{dst} gpga_wide_sext_{dst}_from_{src}(GpgaWide{src} v)
```

**Description**: Converts a wide integer from one width to another with sign extension.

**Parameters**:
- `v`: Source wide integer

**Returns**: Destination wide integer with sign-extended or truncated value.

**Notes**:
- If `dst > src`: Upper bits are filled with the source's sign bit (sign extension)
- If `dst <= src`: Behaves like `gpga_wide_resize_{dst}_from_{src}()`

**Example**:
```cpp
// Create a negative 128-bit value
GpgaWide128 neg128 = gpga_wide_mask_const_128();  // All 1s
GpgaWide256 neg256 = gpga_wide_sext_256_from_128(neg128);
// All words in neg256 are 0xFFFFFFFFFFFFFFFFul
```

---

## Four-State Logic Support

### `GpgaWideFs{width}`

**Purpose**: Enables four-state logic (0, 1, X, Z) for wide integers, compatible with Verilog/SystemVerilog semantics.

**Structure**:
```cpp
struct GpgaWideFs{width} {
    GpgaWide{width} val;  // Bit values
    GpgaWide{width} xz;   // Unknown/high-Z markers
};
```

**Interpretation**:
- `xz[i] == 0, val[i] == 0`: Bit i is **0**
- `xz[i] == 0, val[i] == 1`: Bit i is **1**
- `xz[i] == 1, val[i] == 0`: Bit i is **X** (unknown)
- `xz[i] == 1, val[i] == 1`: Bit i is **Z** (high-impedance)

**Usage Notes**:
- This structure is defined but operations on four-state wide integers are expected to be implemented by the user or in separate modules
- The structure layout matches the four-state logic patterns used in `gpga_4state.h` for consistency

**Example**:
```cpp
GpgaWideFs128 fs_val;
fs_val.val = gpga_wide_from_u64_128(0xF0ul);  // Bits 4-7 are 1
fs_val.xz = gpga_wide_from_u64_128(0x0Ful);   // Bits 0-3 are X/Z
// Result: bits 0-3 are X, bits 4-7 are 1, rest are 0
```

---

## Implementation Notes

### Memory Layout

Wide integers use **little-endian word order**:
- `w[0]` contains bits 0-63 (least significant)
- `w[1]` contains bits 64-127
- `w[2]` contains bits 128-191
- And so on...

Within each 64-bit word, the standard system byte order applies (typically little-endian on Apple GPUs).

---

### Performance Considerations

1. **Operation Complexity**:
   - Bit operations, shifts (by word boundaries), comparisons: O(words)
   - Addition, subtraction: O(words)
   - Multiplication: O(width) bit iterations
   - Division, modulo: O(width) bit iterations

2. **GPU Optimization**:
   - All operations are inline and should be fully unrolled by the compiler
   - No dynamic loops over runtime-determined sizes
   - Type-specific code generation ensures optimal GPU register usage

3. **Width Selection**:
   - Choose the smallest width that meets your needs
   - Prefer widths that are multiples of 64 for optimal performance
   - Non-multiple-of-64 widths incur minimal overhead for masking

---

### Edge Cases

1. **Division by Zero**: Returns 0 (no trap/exception)
2. **Overflow**: All arithmetic wraps around (modulo 2^width)
3. **Out-of-Range Bit Access**: Returns 0 for reads, no-op for writes
4. **Large Shifts**: Shifts >= width return 0 (logical) or sign-extended value (arithmetic)

---

## Usage Examples

### Example 1: Basic Arithmetic

```cpp
// Define 256-bit wide integer support
GPGA_WIDE_DEFINE(256, 4, 0xFFFFFFFFFFFFFFFFul)

// Create two 256-bit values
GpgaWide256 a = gpga_wide_from_u64_256(1000000ul);
GpgaWide256 b = gpga_wide_from_u64_256(500000ul);

// Perform arithmetic
GpgaWide256 sum = gpga_wide_add_256(a, b);      // 1500000
GpgaWide256 diff = gpga_wide_sub_256(a, b);     // 500000
GpgaWide256 prod = gpga_wide_mul_256(a, b);     // 500000000000
GpgaWide256 quot = gpga_wide_div_256(a, b);     // 2

// Test result
bool is_positive = !gpga_wide_signbit_256(sum);  // true
```

---

### Example 2: Bit Manipulation

```cpp
GPGA_WIDE_DEFINE(128, 2, 0xFFFFFFFFFFFFFFFFul)

GpgaWide128 flags = gpga_wide_zero_128();

// Set specific flag bits
flags = gpga_wide_set_bit_128(flags, 0u, 1u);    // Set bit 0
flags = gpga_wide_set_bit_128(flags, 100u, 1u);  // Set bit 100
flags = gpga_wide_set_bit_128(flags, 127u, 1u);  // Set bit 127 (MSB)

// Test specific flags
uint bit0 = gpga_wide_get_bit_128(flags, 0u);    // 1
uint bit50 = gpga_wide_get_bit_128(flags, 50u);  // 0
uint bit100 = gpga_wide_get_bit_128(flags, 100u);// 1

// Check if any flags are set
bool any = gpga_wide_any_128(flags);  // true
```

---

### Example 3: Signed vs Unsigned Comparison

```cpp
GPGA_WIDE_DEFINE(128, 2, 0xFFFFFFFFFFFFFFFFul)

// Create a large value with MSB set (negative if signed)
GpgaWide128 large = gpga_wide_mask_const_128();  // All 1s
GpgaWide128 small = gpga_wide_from_u64_128(100ul);

// Unsigned comparison: large is very big
bool ult = gpga_wide_lt_u_128(large, small);  // false
bool ugt = gpga_wide_gt_u_128(large, small);  // true

// Signed comparison: large is negative (-1)
bool slt = gpga_wide_lt_s_128(large, small);  // true
bool sgt = gpga_wide_gt_s_128(large, small);  // false
```

---

### Example 4: Resizing with Extension

```cpp
GPGA_WIDE_DEFINE(64, 1, 0xFFFFFFFFFFFFFFFFul)
GPGA_WIDE_DEFINE(128, 2, 0xFFFFFFFFFFFFFFFFul)
GPGA_WIDE_DEFINE_RESIZE(128, 64, 2, 1, 0xFFFFFFFFFFFFFFFFul, 0)

// Positive value
GpgaWide64 pos64 = gpga_wide_from_u64_64(42ul);
GpgaWide128 pos128 = gpga_wide_resize_128_from_64(pos64);
// pos128.w[0] = 42, pos128.w[1] = 0

// Negative value (MSB set in 64-bit)
GpgaWide64 neg64 = gpga_wide_from_u64_64(0x8000000000000000ul);
GpgaWide128 zext = gpga_wide_resize_128_from_64(neg64);
// zext.w[0] = 0x8000000000000000ul, zext.w[1] = 0 (zero-extended)

GpgaWide128 sext = gpga_wide_sext_128_from_64(neg64);
// sext.w[0] = 0x8000000000000000ul, sext.w[1] = 0xFFFFFFFFFFFFFFFFul (sign-extended)
```

---

## See Also

- [GPGA_4STATE_API.md](GPGA_4STATE_API.md) - Four-state logic operations
- [GPGA_REAL_API.md](GPGA_REAL_API.md) - Software floating-point operations
- [GPGA_KEYWORDS.md](GPGA_KEYWORDS.md) - Verilog keyword mappings

---

## Version History

- **Initial Version**: Wide integer library for Metal Shading Language supporting arbitrary-width integer operations beyond 64 bits.
