# GPGA Four-State Logic API Documentation

## Overview

The `gpga_4state.h` header provides a comprehensive library for handling four-state logic in Metal Shading Language (MSL). This library implements Verilog's four-value logic system where each bit can be:
- **0**: Logic low
- **1**: Logic high
- **X**: Unknown/don't care
- **Z**: High impedance

## Data Structures

### FourState32
```c
struct FourState32 { uint val; uint xz; };
```
Represents a 32-bit four-state value. The `val` field contains the bit values, and the `xz` field marks which bits are unknown (X) or high-impedance (Z).

### FourState64
```c
struct FourState64 { ulong val; ulong xz; };
```
Represents a 64-bit four-state value with the same structure as FourState32.

## Utility Functions

### fs_mask32
```c
inline uint fs_mask32(uint width)
```
**Purpose**: Creates a bitmask for the specified width (32-bit version).

**Parameters**:
- `width`: The number of bits in the desired mask (0-32)

**Returns**: A uint with the lower `width` bits set to 1. If width ≥ 32, returns all bits set.

**Example**: `fs_mask32(5)` returns `0x1F` (binary: `11111`)

---

### fs_mask64
```c
inline ulong fs_mask64(uint width)
```
**Purpose**: Creates a bitmask for the specified width (64-bit version).

**Parameters**:
- `width`: The number of bits in the desired mask (0-64)

**Returns**: A ulong with the lower `width` bits set to 1. If width ≥ 64, returns all bits set.

---

## Construction Functions

### fs_make32
```c
inline FourState32 fs_make32(uint val, uint xz, uint width)
```
**Purpose**: Constructs a four-state 32-bit value with specified value and unknown bits.

**Parameters**:
- `val`: The bit values (for known 0s and 1s)
- `xz`: Marks which bits are X or Z
- `width`: The effective width to mask to

**Returns**: FourState32 with both fields masked to the specified width.

**Notes**: Bits where `xz` is 1 are considered unknown; the corresponding `val` bits are typically 0.

---

### fs_make64
```c
inline FourState64 fs_make64(ulong val, ulong xz, uint width)
```
**Purpose**: Constructs a four-state 64-bit value (64-bit version of fs_make32).

**Parameters**:
- `val`: The bit values
- `xz`: Marks which bits are X or Z
- `width`: The effective width

**Returns**: FourState64 with both fields masked to the specified width.

---

### fs_allx32
```c
inline FourState32 fs_allx32(uint width)
```
**Purpose**: Creates a four-state value where all bits are unknown (X).

**Parameters**:
- `width`: Number of bits to set to X

**Returns**: FourState32 with `val=0` and `xz` set to all 1s in the specified width.

---

### fs_allx64
```c
inline FourState64 fs_allx64(uint width)
```
**Purpose**: Creates a four-state value where all bits are unknown (X) (64-bit version).

**Parameters**:
- `width`: Number of bits to set to X

**Returns**: FourState64 with `val=0` and `xz` set to all 1s in the specified width.

---

## Size Manipulation Functions

### fs_resize32
```c
inline FourState32 fs_resize32(FourState32 a, uint width)
```
**Purpose**: Resizes a four-state value to a new width by truncation or zero-extension.

**Parameters**:
- `a`: Input four-state value
- `width`: Target width

**Returns**: Four-state value truncated or zero-extended to the specified width.

**Notes**: Does not perform sign extension; use `fs_sext32` for that.

---

### fs_resize64
```c
inline FourState64 fs_resize64(FourState64 a, uint width)
```
**Purpose**: Resizes a four-state value to a new width (64-bit version).

---

### fs_sext32
```c
inline FourState32 fs_sext32(FourState32 a, uint src_width, uint target_width)
```
**Purpose**: Sign-extends a four-state value from source width to target width.

**Parameters**:
- `a`: Input four-state value
- `src_width`: Current width of the value
- `target_width`: Desired width after extension

**Returns**: Sign-extended four-state value. If the sign bit is X or Z, the extension bits are also X.

**Behavior**:
- If target ≤ source: truncates to target width
- If sign bit is known: extends with that bit value
- If sign bit is X/Z: extends with X

---

### fs_sext64
```c
inline FourState64 fs_sext64(FourState64 a, uint src_width, uint target_width)
```
**Purpose**: Sign-extends a four-state value (64-bit version).

---

## Merge Functions

### fs_merge32
```c
inline FourState32 fs_merge32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Merges two four-state values, producing X where they differ.

**Parameters**:
- `a`: First four-state value
- `b`: Second four-state value
- `width`: Effective width for the operation

**Returns**: Four-state value where:
- Bits that are the same in both inputs (and both known) retain their value
- Bits that differ or are unknown in either input become X

**Use Case**: Used in conditional expressions where the condition is unknown.

---

### fs_merge64
```c
inline FourState64 fs_merge64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Merges two four-state values (64-bit version).

---

## Bitwise Logical Operations

### fs_not32
```c
inline FourState32 fs_not32(FourState32 a, uint width)
```
**Purpose**: Bitwise NOT operation on four-state value.

**Parameters**:
- `a`: Input four-state value
- `width`: Effective width

**Returns**: Bitwise complement where:
- Known 0 → 1
- Known 1 → 0
- X/Z → X

---

### fs_not64
```c
inline FourState64 fs_not64(FourState64 a, uint width)
```
**Purpose**: Bitwise NOT (64-bit version).

---

### fs_and32
```c
inline FourState32 fs_and32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Bitwise AND operation with four-state logic.

**Parameters**:
- `a`, `b`: Input four-state values
- `width`: Effective width

**Returns**: Four-state result following the truth table:
- 0 & anything → 0
- 1 & 1 → 1
- 1 & 0 → 0
- X & 1 → X
- X & 0 → 0

---

### fs_and64
```c
inline FourState64 fs_and64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Bitwise AND (64-bit version).

---

### fs_or32
```c
inline FourState32 fs_or32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Bitwise OR operation with four-state logic.

**Returns**: Four-state result where:
- 1 | anything → 1
- 0 | 0 → 0
- 0 | 1 → 1
- X | 0 → X
- X | 1 → 1

---

### fs_or64
```c
inline FourState64 fs_or64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Bitwise OR (64-bit version).

---

### fs_xor32
```c
inline FourState32 fs_xor32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Bitwise XOR operation with four-state logic.

**Returns**: Four-state result where:
- If either input has X/Z: result is X
- Otherwise: standard XOR of known bits

---

### fs_xor64
```c
inline FourState64 fs_xor64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Bitwise XOR (64-bit version).

---

## Arithmetic Operations

### fs_add32
```c
inline FourState32 fs_add32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Addition of four-state values.

**Returns**:
- If either operand contains X/Z bits: returns all X
- Otherwise: performs standard addition and masks to width

**Notes**: Conservative approach - any unknown propagates to entire result.

---

### fs_add64
```c
inline FourState64 fs_add64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Addition (64-bit version).

---

### fs_sub32
```c
inline FourState32 fs_sub32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Subtraction of four-state values.

**Returns**:
- If either operand contains X/Z: returns all X
- Otherwise: performs standard subtraction and masks to width

---

### fs_sub64
```c
inline FourState64 fs_sub64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Subtraction (64-bit version).

---

### fs_mul32
```c
inline FourState32 fs_mul32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Multiplication of four-state values.

**Returns**:
- If either operand contains X/Z: returns all X
- Otherwise: performs standard multiplication and masks to width

---

### fs_mul64
```c
inline FourState64 fs_mul64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Multiplication (64-bit version).

---

### fs_pow32
```c
inline FourState32 fs_pow32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Power operation (a ** b) for unsigned values.

**Parameters**:
- `a`: Base value
- `b`: Exponent value
- `width`: Result width

**Returns**:
- If either operand contains X/Z: returns all X
- Otherwise: computes a^b using exponentiation by squaring

**Algorithm**: Uses efficient binary exponentiation.

---

### fs_pow64
```c
inline FourState64 fs_pow64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Power operation (64-bit version).

---

### fs_spow32
```c
inline FourState32 fs_spow32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed power operation where exponent is treated as signed.

**Returns**:
- If either operand contains X/Z: returns all X
- If exponent is negative: returns 0
- Otherwise: computes a^b

**Notes**: The 's' prefix indicates signed exponent interpretation.

---

### fs_spow64
```c
inline FourState64 fs_spow64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed power operation (64-bit version).

---

### fs_div32
```c
inline FourState32 fs_div32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned division of four-state values.

**Returns**:
- If either operand contains X/Z: returns all X
- If divisor is 0: returns all X
- Otherwise: performs unsigned division

---

### fs_div64
```c
inline FourState64 fs_div64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned division (64-bit version).

---

### fs_mod32
```c
inline FourState32 fs_mod32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned modulo operation.

**Returns**:
- If either operand contains X/Z: returns all X
- If divisor is 0: returns all X
- Otherwise: performs unsigned modulo

---

### fs_mod64
```c
inline FourState64 fs_mod64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned modulo (64-bit version).

---

## Comparison Operations

### fs_cmp32
```c
inline FourState32 fs_cmp32(uint value, bool pred)
```
**Purpose**: Creates a 1-bit four-state value from a boolean predicate.

**Parameters**:
- `value`: Unused parameter (kept for API consistency)
- `pred`: Boolean result of comparison

**Returns**: FourState32 with value=1 if pred is true, value=0 if false, xz=0 (known).

---

### fs_cmp64
```c
inline FourState64 fs_cmp64(ulong value, bool pred)
```
**Purpose**: Creates a 1-bit four-state value from boolean (64-bit version).

---

### fs_eq32
```c
inline FourState32 fs_eq32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Equality comparison (==).

**Returns**: 1-bit four-state value:
- If either operand has X/Z: returns X
- If values are equal: returns 1
- If values differ: returns 0

---

### fs_eq64
```c
inline FourState64 fs_eq64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Equality comparison (64-bit version).

---

### fs_ne32
```c
inline FourState32 fs_ne32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Inequality comparison (!=).

**Returns**: 1-bit four-state value (opposite of fs_eq32).

---

### fs_ne64
```c
inline FourState64 fs_ne64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Inequality comparison (64-bit version).

---

### fs_lt32
```c
inline FourState32 fs_lt32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned less-than comparison (<).

**Returns**: 1-bit four-state value:
- If either operand has X/Z: returns X
- Otherwise: returns 1 if a < b (unsigned), 0 otherwise

---

### fs_lt64
```c
inline FourState64 fs_lt64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned less-than (64-bit version).

---

### fs_gt32
```c
inline FourState32 fs_gt32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned greater-than comparison (>).

---

### fs_gt64
```c
inline FourState64 fs_gt64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned greater-than (64-bit version).

---

### fs_le32
```c
inline FourState32 fs_le32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned less-than-or-equal comparison (<=).

---

### fs_le64
```c
inline FourState64 fs_le64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned less-than-or-equal (64-bit version).

---

### fs_ge32
```c
inline FourState32 fs_ge32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Unsigned greater-than-or-equal comparison (>=).

---

### fs_ge64
```c
inline FourState64 fs_ge64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Unsigned greater-than-or-equal (64-bit version).

---

## Shift Operations

### fs_shl32
```c
inline FourState32 fs_shl32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Logical left shift (<<).

**Parameters**:
- `a`: Value to shift
- `b`: Shift amount
- `width`: Result width

**Returns**:
- If shift amount contains X/Z: returns all X
- If shift >= width: returns 0
- Otherwise: shifts both val and xz fields left

---

### fs_shl64
```c
inline FourState64 fs_shl64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Logical left shift (64-bit version).

---

### fs_shr32
```c
inline FourState32 fs_shr32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Logical right shift (>>).

**Returns**:
- If shift amount contains X/Z: returns all X
- If shift >= width: returns 0
- Otherwise: shifts both val and xz fields right (zero-fill)

---

### fs_shr64
```c
inline FourState64 fs_shr64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Logical right shift (64-bit version).

---

### fs_sar32
```c
inline FourState32 fs_sar32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Arithmetic right shift (>>>), sign-extends.

**Parameters**:
- `a`: Value to shift
- `b`: Shift amount
- `width`: Bit width for sign determination

**Returns**:
- If shift amount contains X/Z: returns all X
- If sign bit is X/Z: returns all X
- Otherwise: shifts right filling with sign bit

**Notes**: Preserves sign by replicating the MSB.

---

### fs_sar64
```c
inline FourState64 fs_sar64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Arithmetic right shift (64-bit version).

---

## Multiplexer

### fs_mux32
```c
inline FourState32 fs_mux32(FourState32 cond, FourState32 t, FourState32 f, uint width)
```
**Purpose**: Four-state multiplexer (ternary operator).

**Parameters**:
- `cond`: Condition value
- `t`: True branch value
- `f`: False branch value
- `width`: Result width

**Returns**:
- If condition contains X/Z: returns merge of t and f
- If condition is non-zero: returns t
- If condition is zero: returns f

**Use Case**: Implements Verilog's `cond ? t : f` expression.

---

### fs_mux64
```c
inline FourState64 fs_mux64(FourState64 cond, FourState64 t, FourState64 f, uint width)
```
**Purpose**: Four-state multiplexer (64-bit version).

---

## Reduction Operations

### fs_red_and32
```c
inline FourState32 fs_red_and32(FourState32 a, uint width)
```
**Purpose**: Reduction AND - ANDs all bits together.

**Returns**: 1-bit four-state value:
- If any bit is known 0: returns 0
- If all bits are known 1: returns 1
- If any bit is X/Z but no known 0s: returns X

**Use Case**: Verilog's `&signal` operator.

---

### fs_red_and64
```c
inline FourState64 fs_red_and64(FourState64 a, uint width)
```
**Purpose**: Reduction AND (64-bit version).

---

### fs_red_or32
```c
inline FourState32 fs_red_or32(FourState32 a, uint width)
```
**Purpose**: Reduction OR - ORs all bits together.

**Returns**: 1-bit four-state value:
- If any bit is known 1: returns 1
- If all bits are known 0: returns 0
- If any bit is X/Z but no known 1s: returns X

**Use Case**: Verilog's `|signal` operator.

---

### fs_red_or64
```c
inline FourState64 fs_red_or64(FourState64 a, uint width)
```
**Purpose**: Reduction OR (64-bit version).

---

### fs_red_xor32
```c
inline FourState32 fs_red_xor32(FourState32 a, uint width)
```
**Purpose**: Reduction XOR - computes parity of all bits.

**Returns**: 1-bit four-state value:
- If any bit is X/Z: returns X
- Otherwise: returns 1 if odd number of 1s, 0 if even

**Use Case**: Verilog's `^signal` operator (parity check).

---

### fs_red_xor64
```c
inline FourState64 fs_red_xor64(FourState64 a, uint width)
```
**Purpose**: Reduction XOR (64-bit version).

**Algorithm**: Computes popcount of low and high 32 bits separately then combines.

---

## Signed Arithmetic Support

### fs_sign32
```c
inline int fs_sign32(uint val, uint width)
```
**Purpose**: Sign-extends an unsigned value to signed int.

**Parameters**:
- `val`: Unsigned value
- `width`: Actual bit width of the value (1-32)

**Returns**: Sign-extended signed int.

**Use Case**: Helper for signed operations on arbitrary-width values.

---

### fs_sign64
```c
inline long fs_sign64(ulong val, uint width)
```
**Purpose**: Sign-extends an unsigned value to signed long (64-bit version).

---

### fs_slt32
```c
inline FourState32 fs_slt32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed less-than comparison.

**Returns**: 1-bit four-state value:
- If either operand has X/Z: returns X
- Otherwise: treats values as signed and returns 1 if a < b

---

### fs_slt64
```c
inline FourState64 fs_slt64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed less-than (64-bit version).

---

### fs_sle32
```c
inline FourState32 fs_sle32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed less-than-or-equal comparison.

---

### fs_sle64
```c
inline FourState64 fs_sle64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed less-than-or-equal (64-bit version).

---

### fs_sgt32
```c
inline FourState32 fs_sgt32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed greater-than comparison.

---

### fs_sgt64
```c
inline FourState64 fs_sgt64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed greater-than (64-bit version).

---

### fs_sge32
```c
inline FourState32 fs_sge32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed greater-than-or-equal comparison.

---

### fs_sge64
```c
inline FourState64 fs_sge64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed greater-than-or-equal (64-bit version).

---

### fs_sdiv32
```c
inline FourState32 fs_sdiv32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed division.

**Returns**:
- If either operand contains X/Z: returns all X
- If divisor is 0: returns all X
- Otherwise: performs signed division

---

### fs_sdiv64
```c
inline FourState64 fs_sdiv64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed division (64-bit version).

---

### fs_smod32
```c
inline FourState32 fs_smod32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Signed modulo operation.

**Returns**:
- If either operand contains X/Z: returns all X
- If divisor is 0: returns all X
- Otherwise: performs signed modulo

---

### fs_smod64
```c
inline FourState64 fs_smod64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Signed modulo (64-bit version).

---

## Logical Operations (Short-Circuit Emulation)

### fs_log_not32
```c
inline FourState32 fs_log_not32(FourState32 a, uint width)
```
**Purpose**: Logical NOT operation (!a).

**Returns**: 1-bit four-state value:
- If any bit is known 1: returns 0 (value is true)
- If all bits are known 0: returns 1 (value is false)
- If any bit is X/Z and no known 1s: returns X

**Difference from fs_not32**: This is logical (returns 1-bit), not bitwise.

---

### fs_log_not64
```c
inline FourState64 fs_log_not64(FourState64 a, uint width)
```
**Purpose**: Logical NOT (64-bit version).

---

### fs_log_and32
```c
inline FourState32 fs_log_and32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Logical AND operation (a && b).

**Returns**: 1-bit four-state value:
- If either operand is definitely false (all known 0s): returns 0
- If both operands are definitely true (any known 1): returns 1
- Otherwise: returns X

**Use Case**: Verilog's `&&` operator.

---

### fs_log_and64
```c
inline FourState64 fs_log_and64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Logical AND (64-bit version).

---

### fs_log_or32
```c
inline FourState32 fs_log_or32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Logical OR operation (a || b).

**Returns**: 1-bit four-state value:
- If either operand is definitely true: returns 1
- If both operands are definitely false: returns 0
- Otherwise: returns X

**Use Case**: Verilog's `||` operator.

---

### fs_log_or64
```c
inline FourState64 fs_log_or64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Logical OR (64-bit version).

---

## Case Equality Operations

### fs_case_eq32
```c
inline bool fs_case_eq32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Case equality (===) - compares including X and Z values.

**Returns**: true if values match exactly (including X/Z positions), false otherwise.

**Difference from fs_eq32**: This compares X and Z states exactly, doesn't return X.

**Use Case**: Verilog's `===` operator.

---

### fs_case_eq64
```c
inline bool fs_case_eq64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Case equality (64-bit version).

---

### fs_casez32
```c
inline bool fs_casez32(FourState32 a, FourState32 b, uint ignore_mask, uint width)
```
**Purpose**: Case equality with Z-masking (casez statement).

**Parameters**:
- `a`: Pattern value
- `b`: Value to match against
- `ignore_mask`: Bits to ignore (treat as don't-care)
- `width`: Effective width

**Returns**: true if values match in non-ignored positions.

**Use Case**: Verilog's `casez` statement where ? and Z are wildcards.

---

### fs_casez64
```c
inline bool fs_casez64(FourState64 a, FourState64 b, ulong ignore_mask, uint width)
```
**Purpose**: Case equality with Z-masking (64-bit version).

---

### fs_casex32
```c
inline bool fs_casex32(FourState32 a, FourState32 b, uint width)
```
**Purpose**: Case equality with X and Z masking (casex statement).

**Returns**: true if values match in positions where neither has X or Z.

**Use Case**: Verilog's `casex` statement where ?, X, and Z are wildcards.

---

### fs_casex64
```c
inline bool fs_casex64(FourState64 a, FourState64 b, uint width)
```
**Purpose**: Case equality with X and Z masking (64-bit version).

---

## Usage Examples

### Creating Values
```c
// Create known value 5 (binary: 101) with 3-bit width
FourState32 five = fs_make32(5, 0, 3);

// Create value with X in bit 1 (value: 1X1)
FourState32 has_x = fs_make32(0b101, 0b010, 3);

// Create all-unknown 8-bit value
FourState32 unknown = fs_allx32(8);
```

### Performing Operations
```c
FourState32 a = fs_make32(3, 0, 4);  // 4'b0011
FourState32 b = fs_make32(5, 0, 4);  // 4'b0101
FourState32 result;

// Bitwise AND
result = fs_and32(a, b, 4);  // 4'b0001

// Addition
result = fs_add32(a, b, 4);  // 4'b1000 (wraps to 4 bits)

// Comparison
result = fs_lt32(a, b, 4);   // 1'b1 (true)
```

### Handling Unknown Values
```c
FourState32 known = fs_make32(7, 0, 3);
FourState32 unknown = fs_allx32(3);

// Arithmetic with unknown propagates X
FourState32 sum = fs_add32(known, unknown, 3);  // All X

// Logical operations can sometimes resolve
FourState32 x_val = fs_make32(0, 1, 1);  // 1'bX
FourState32 zero = fs_make32(0, 0, 1);   // 1'b0
FourState32 and_result = fs_and32(x_val, zero, 1);  // 1'b0 (0 & X = 0)
```

## Implementation Notes

1. **X/Z Representation**: The `xz` field marks unknown bits. When `xz[i]=1`, the value of bit `i` is unknown or high-impedance.

2. **Conservative Arithmetic**: Most arithmetic operations return all-X if any input bit is unknown, following Verilog semantics.

3. **Width Management**: All operations respect the specified width parameter, masking results appropriately.

4. **Metal Compatibility**: The header works in both Metal (GPU) and standard C++ (CPU) environments through conditional compilation.

5. **Performance**: All functions are inline for zero overhead in both compilation targets.

## See Also

- Verilog IEEE 1364-2005 Standard (for four-state semantics)
- Metal Shading Language Specification
- MetalFPGA documentation
