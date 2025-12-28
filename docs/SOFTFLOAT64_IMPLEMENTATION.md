# Software Double-Precision Float Implementation for Metal

## Problem Statement

Metal Shading Language does not support the `double` type (64-bit floating-point). Apple GPUs lack native double-precision FPU hardware. However, Verilog-2005's `real` type is defined as IEEE 754 double-precision (64-bit), making software emulation **mandatory** for full language compliance.

### Current Issue

The existing code uses `double` directly, which fails compilation:

```metal
// BROKEN: Metal doesn't support 'double'
inline double gpga_bits_to_real(ulong bits) {
  return as_type<double>(bits);
}
```

**Error**: `'double' is not supported in Metal`

### Impact

Without software double support, metalfpga cannot execute Verilog designs that use:
- Real number arithmetic (DSP, filters, FFTs)
- Analog/mixed-signal modeling
- Mathematical computations
- System-level modeling with floating-point

This blocks **full Verilog-2005 compliance**.

---

## Solution: Software Double Emulation Using `ulong` address space

1. **Single 64-bit register** - Use `ulong` (unsigned 64-bit integer) to store IEEE 754 double bits directly. **No Structs needed**.
2. **Full IEEE 754 compliance** - Implement exact semantics including NaN, Infinity, denormals, and proper rounding.
3. **GPU-optimized algorithms** - Use Metal 4 intrinsics exclusively. No CPU-style algorithms.
4. **Branchless operations** - Minimize thread divergence using bitwise operations and `select()`.
5. **No approximations** - Division, square root, and transcendentals must be mathematically correct.

### IEEE 754 Double-Precision Format

```
Bit layout in ulong:
┌─┬──────────┬────────────────────────────────────────────────────┐
│S│Exponent  │                    Mantissa                        │
└─┴──────────┴────────────────────────────────────────────────────┘
63  62    52  51                                                  0

- Bit 63:      Sign (1 bit)
- Bits 62-52:  Exponent (11 bits, biased by 1023)
- Bits 51-0:   Mantissa (52 bits, implicit leading 1 for normalized)
```

**Special values**:
- Zero: `exp=0, mantissa=0`
- Infinity: `exp=2047 (0x7FF), mantissa=0`
- NaN: `exp=2047 (0x7FF), mantissa≠0`
- Denormals: `exp=0, mantissa≠0`

---

## Claude's **Suggested** Implementation Roadmap (ref metal 4 compendium for any hardware functionality exploitable for speedup patterns)

### Phase 1: Core Infrastructure

#### Type Definition
```metal
// Store IEEE 754 double bits in 64-bit unsigned integer
typedef ulong gpga_double;
```

#### Bit Manipulation Helpers
```metal
// Extract components (inline for zero overhead)
inline uint  gpga_double_sign(gpga_double d)     { return (d >> 63) & 1; }
inline uint  gpga_double_exp(gpga_double d)      { return (d >> 52) & 0x7FF; }
inline ulong gpga_double_mantissa(gpga_double d) { return d & 0x000FFFFFFFFFFFFF; }

// Pack components into double
inline gpga_double gpga_double_pack(uint sign, uint exp, ulong mantissa) {
  return ((ulong)sign << 63) | ((ulong)exp << 52) | (mantissa & 0x000FFFFFFFFFFFFF);
}

// Special value constructors
inline gpga_double gpga_double_zero(uint sign)     { return (ulong)sign << 63; }
inline gpga_double gpga_double_inf(uint sign)      { return gpga_double_pack(sign, 0x7FF, 0); }
inline gpga_double gpga_double_nan()               { return 0x7FF8000000000000UL; }

// Special value detection (branchless)
inline bool gpga_double_is_zero(gpga_double d)    { return (d & 0x7FFFFFFFFFFFFFFF) == 0; }
inline bool gpga_double_is_inf(gpga_double d)     { return (d & 0x7FFFFFFFFFFFFFFF) == 0x7FF0000000000000; }
inline bool gpga_double_is_nan(gpga_double d)     { return ((d >> 52) & 0x7FF) == 0x7FF && (d & 0x000FFFFFFFFFFFFF) != 0; }
inline bool gpga_double_is_denorm(gpga_double d)  { return ((d >> 52) & 0x7FF) == 0 && (d & 0x000FFFFFFFFFFFFF) != 0; }
```

#### Conversion Functions
```metal
// Replace broken as_type<double>() calls
inline gpga_double gpga_bits_to_real(ulong bits) {
  return bits;  // Already in IEEE 754 format
}

inline ulong gpga_real_to_bits(gpga_double value) {
  return value;  // Already in IEEE 754 format
}

// Convert integer to double
inline gpga_double gpga_int_to_double(long value) {
  if (value == 0) return 0;

  uint sign = (value < 0) ? 1 : 0;
  ulong abs_val = (value < 0) ? -value : value;

  // Find leading bit position using clz (count leading zeros)
  uint leading_zeros = clz(abs_val);
  uint exp = 1023 + (63 - leading_zeros);

  // Shift mantissa to position (remove implicit leading 1)
  ulong mantissa = (abs_val << (leading_zeros + 1)) >> 12;

  return gpga_double_pack(sign, exp, mantissa);
}

// Convert double to integer (truncate toward zero)
inline long gpga_double_to_int(gpga_double d) {
  uint sign = gpga_double_sign(d);
  uint exp = gpga_double_exp(d);
  ulong mantissa = gpga_double_mantissa(d);

  if (exp == 0) return 0;           // Zero or denormal
  if (exp == 0x7FF) return 0;       // Inf/NaN

  int shift = (int)exp - 1023 - 52;
  if (shift < -52) return 0;
  if (shift > 10) return 0;

  // Add implicit leading 1
  ulong result = mantissa | (1UL << 52);

  // Shift to integer position
  result = (shift >= 0) ? (result << shift) : (result >> (-shift));

  return sign ? -(long)result : (long)result;
}

// Convert float to double (upcast - lossless)
inline gpga_double gpga_float_to_double(float f) {
  uint fbits = as_type<uint>(f);
  uint sign = (fbits >> 31) & 1;
  uint exp_f = (fbits >> 23) & 0xFF;
  uint mant_f = fbits & 0x7FFFFF;

  // Handle special cases
  if (exp_f == 0) {
    if (mant_f == 0) return gpga_double_zero(sign);
    // Denormal float - normalize for double
    uint shift = clz(mant_f) - 8;
    return gpga_double_pack(sign, 1023 - 126 - shift, (ulong)mant_f << (29 + shift));
  }
  if (exp_f == 0xFF) {
    return (mant_f == 0) ? gpga_double_inf(sign) : gpga_double_nan();
  }

  // Normal float: adjust exponent bias (127 → 1023) and shift mantissa
  uint exp_d = exp_f - 127 + 1023;
  ulong mant_d = (ulong)mant_f << 29;
  return gpga_double_pack(sign, exp_d, mant_d);
}

// Convert double to float (downcast - lossy)
inline float gpga_double_to_float(gpga_double d) {
  uint sign = gpga_double_sign(d);
  uint exp_d = gpga_double_exp(d);
  ulong mant_d = gpga_double_mantissa(d);

  // Handle special cases
  if (exp_d == 0) {
    if (mant_d == 0) return sign ? -0.0f : 0.0f;
    // Denormal double might still be normal float
    // Simplified: flush to zero
    return sign ? -0.0f : 0.0f;
  }
  if (exp_d == 0x7FF) {
    return (mant_d == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
  }

  // Adjust exponent bias (1023 → 127)
  int exp_f = (int)exp_d - 1023 + 127;
  if (exp_f <= 0) return sign ? -0.0f : 0.0f;      // Underflow
  if (exp_f >= 0xFF) return sign ? -INFINITY : INFINITY;  // Overflow

  // Shift mantissa and round
  uint mant_f = (uint)(mant_d >> 29);
  uint round_bit = (mant_d >> 28) & 1;
  mant_f += round_bit;  // Round to nearest

  uint fbits = (sign << 31) | ((uint)exp_f << 23) | (mant_f & 0x7FFFFF);
  return as_type<float>(fbits);
}
```

### Phase 2: Comparison Operations

```metal
// Less-than comparison
inline bool gpga_double_lt(gpga_double a, gpga_double b) {
  // Handle NaN (always false)
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) return false;

  uint sign_a = gpga_double_sign(a);
  uint sign_b = gpga_double_sign(b);

  // Different signs: negative < positive (unless both zero)
  if (sign_a != sign_b) {
    bool both_zero = gpga_double_is_zero(a) && gpga_double_is_zero(b);
    return select(sign_a > sign_b, false, both_zero);
  }

  // Same sign: compare as unsigned (flip for negative)
  ulong bits_a = a & 0x7FFFFFFFFFFFFFFF;
  ulong bits_b = b & 0x7FFFFFFFFFFFFFFF;
  return sign_a ? (bits_a > bits_b) : (bits_a < bits_b);
}

inline bool gpga_double_le(gpga_double a, gpga_double b) {
  return gpga_double_lt(a, b) || gpga_double_eq(a, b);
}

inline bool gpga_double_gt(gpga_double a, gpga_double b) {
  return gpga_double_lt(b, a);
}

inline bool gpga_double_ge(gpga_double a, gpga_double b) {
  return gpga_double_le(b, a);
}

inline bool gpga_double_eq(gpga_double a, gpga_double b) {
  // NaN never equals anything
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) return false;

  // Bitwise equality (handles +0 == -0)
  return (a == b) || ((a & 0x7FFFFFFFFFFFFFFF) == 0 && (b & 0x7FFFFFFFFFFFFFFF) == 0);
}

inline bool gpga_double_ne(gpga_double a, gpga_double b) {
  return !gpga_double_eq(a, b);
}
```

### Phase 3: Arithmetic Operations

#### Addition and Subtraction

**Critical requirement**: Full IEEE 754 rounding (round-to-nearest-even).

```metal
inline gpga_double gpga_double_add(gpga_double a, gpga_double b) {
  // Special cases
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) return gpga_double_nan();
  if (gpga_double_is_zero(a)) return b;
  if (gpga_double_is_zero(b)) return a;
  if (gpga_double_is_inf(a)) {
    if (gpga_double_is_inf(b) && gpga_double_sign(a) != gpga_double_sign(b))
      return gpga_double_nan();  // inf - inf
    return a;
  }
  if (gpga_double_is_inf(b)) return b;

  // Extract components
  uint sign_a = gpga_double_sign(a);
  uint sign_b = gpga_double_sign(b);
  uint exp_a = gpga_double_exp(a);
  uint exp_b = gpga_double_exp(b);
  ulong mant_a = gpga_double_mantissa(a) | (1UL << 52);  // Add implicit 1
  ulong mant_b = gpga_double_mantissa(b) | (1UL << 52);

  // Align exponents (shift smaller mantissa right)
  // Add 3 guard bits for rounding
  mant_a <<= 3;
  mant_b <<= 3;

  int exp_diff = (int)exp_a - (int)exp_b;
  uint result_exp;

  if (exp_diff > 0) {
    if (exp_diff < 64) mant_b >>= exp_diff;
    else mant_b = 0;
    result_exp = exp_a;
  } else {
    if (-exp_diff < 64) mant_a >>= -exp_diff;
    else mant_a = 0;
    result_exp = exp_b;
  }

  // Add or subtract based on signs
  ulong result_mant;
  uint result_sign;

  if (sign_a == sign_b) {
    // Same sign: add mantissas
    result_mant = mant_a + mant_b;
    result_sign = sign_a;

    // Normalize if overflow
    if (result_mant & (1UL << 56)) {
      result_mant >>= 1;
      result_exp++;
    }
  } else {
    // Different signs: subtract larger from smaller
    if (mant_a >= mant_b) {
      result_mant = mant_a - mant_b;
      result_sign = sign_a;
    } else {
      result_mant = mant_b - mant_a;
      result_sign = sign_b;
    }

    if (result_mant == 0) return gpga_double_zero(0);

    // Normalize: shift left until bit 55 is set
    uint shift = clz(result_mant) - 8;
    result_mant <<= shift;
    result_exp -= shift;
  }

  // Round to nearest even (check guard bits)
  uint guard_bits = result_mant & 7;
  result_mant >>= 3;

  if (guard_bits > 4 || (guard_bits == 4 && (result_mant & 1))) {
    result_mant++;
    if (result_mant & (1UL << 53)) {
      result_mant >>= 1;
      result_exp++;
    }
  }

  // Remove implicit 1 and pack
  result_mant &= 0x000FFFFFFFFFFFFF;
  return gpga_double_pack(result_sign, result_exp, result_mant);
}

inline gpga_double gpga_double_sub(gpga_double a, gpga_double b) {
  return gpga_double_add(a, b ^ (1UL << 63));  // Flip sign of b
}

inline gpga_double gpga_double_neg(gpga_double d) {
  return d ^ (1UL << 63);
}

inline gpga_double gpga_double_abs(gpga_double d) {
  return d & 0x7FFFFFFFFFFFFFFF;
}
```

#### Multiplication

**Critical requirement**: Full 106-bit intermediate precision for mantissa multiply.

```metal
inline gpga_double gpga_double_mul(gpga_double a, gpga_double b) {
  // Special cases
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) return gpga_double_nan();
  if (gpga_double_is_zero(a) || gpga_double_is_zero(b))
    return gpga_double_zero(gpga_double_sign(a) ^ gpga_double_sign(b));
  if (gpga_double_is_inf(a) || gpga_double_is_inf(b))
    return gpga_double_inf(gpga_double_sign(a) ^ gpga_double_sign(b));

  uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
  uint exp_a = gpga_double_exp(a);
  uint exp_b = gpga_double_exp(b);
  ulong mant_a = gpga_double_mantissa(a) | (1UL << 52);
  ulong mant_b = gpga_double_mantissa(b) | (1UL << 52);

  // Multiply 53×53 bit mantissas → 106 bits
  // Split into 32-bit chunks for precise multiplication
  uint a_hi = mant_a >> 32;
  uint a_lo = mant_a & 0xFFFFFFFF;
  uint b_hi = mant_b >> 32;
  uint b_lo = mant_b & 0xFFFFFFFF;

  ulong p0 = (ulong)a_lo * b_lo;
  ulong p1 = (ulong)a_lo * b_hi;
  ulong p2 = (ulong)a_hi * b_lo;
  ulong p3 = (ulong)a_hi * b_hi;

  // Combine partial products with carry handling
  ulong mid = (p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF);
  ulong result_hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
  ulong result_lo = (mid << 32) | (p0 & 0xFFFFFFFF);

  // Result exponent
  uint result_exp = exp_a + exp_b - 1023;

  // Normalize (result is in bit 105 or 104)
  if (result_hi & (1UL << 53)) {
    // Shift right, preserve LSB for rounding
    uint round_bit = result_hi & 1;
    result_hi = (result_hi >> 1) | ((result_lo >> 63) << 0);
    result_lo = (result_lo >> 1) | ((ulong)round_bit << 63);
    result_exp++;
  }

  // Round to nearest even
  uint guard = (result_hi & 1);
  result_hi >>= 1;
  if (guard && ((result_hi & 1) || result_lo)) {
    result_hi++;
  }

  ulong result_mant = result_hi & 0x000FFFFFFFFFFFFF;
  return gpga_double_pack(sign, result_exp, result_mant);
}
```

#### Division

**Critical requirement**: Use Gold-Schmidtt division or Newton-Raphson with full precision. approximations leading to IEEE un-trueness is not allowed.

```metal
// Division using Newton-Raphson reciprocal
inline gpga_double gpga_double_div(gpga_double a, gpga_double b) {
  // Special cases
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) return gpga_double_nan();
  if (gpga_double_is_zero(b)) {
    if (gpga_double_is_zero(a)) return gpga_double_nan();  // 0/0
    return gpga_double_inf(gpga_double_sign(a) ^ gpga_double_sign(b));
  }
  if (gpga_double_is_zero(a)) return gpga_double_zero(gpga_double_sign(a) ^ gpga_double_sign(b));
  if (gpga_double_is_inf(a)) {
    if (gpga_double_is_inf(b)) return gpga_double_nan();  // inf/inf
    return gpga_double_inf(gpga_double_sign(a) ^ gpga_double_sign(b));
  }
  if (gpga_double_is_inf(b)) return gpga_double_zero(gpga_double_sign(a) ^ gpga_double_sign(b));

  uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
  uint exp_a = gpga_double_exp(a);
  uint exp_b = gpga_double_exp(b);
  ulong mant_a = gpga_double_mantissa(a) | (1UL << 52);
  ulong mant_b = gpga_double_mantissa(b) | (1UL << 52);

  // Compute 1/b using Newton-Raphson: x_{n+1} = x_n * (2 - b * x_n)
  // Initial approximation from float reciprocal
  float b_float = gpga_double_to_float(b & 0x7FFFFFFFFFFFFFFF);
  float recip_approx = 1.0f / b_float;
  gpga_double x = gpga_float_to_double(recip_approx);

  // Refine with 3 Newton-Raphson iterations for full precision
  for (int i = 0; i < 3; i++) {
    gpga_double bx = gpga_double_mul(b, x);
    gpga_double two = gpga_int_to_double(2);
    gpga_double diff = gpga_double_sub(two, bx);
    x = gpga_double_mul(x, diff);
  }

  // Multiply a * (1/b)
  return gpga_double_mul(a, x);
}
```

#### Square Root

**Critical requirement**: Use Newton-Raphson with full precision convergence.

```metal
inline gpga_double gpga_double_sqrt(gpga_double d) {
  if (gpga_double_is_nan(d)) return d;
  if (gpga_double_sign(d)) return gpga_double_nan();  // sqrt(negative)
  if (gpga_double_is_zero(d)) return d;
  if (gpga_double_is_inf(d)) return d;

  // Initial approximation from float sqrt
  float d_float = gpga_double_to_float(d);
  float sqrt_approx = sqrt(d_float);
  gpga_double x = gpga_float_to_double(sqrt_approx);

  // Newton-Raphson: x_{n+1} = 0.5 * (x_n + d / x_n)
  gpga_double half = gpga_double_pack(0, 1022, 0);  // 0.5

  for (int i = 0; i < 4; i++) {
    gpga_double d_div_x = gpga_double_div(d, x);
    gpga_double sum = gpga_double_add(x, d_div_x);
    x = gpga_double_mul(half, sum);
  }

  return x;
}
```

### Phase 4: Transcendental Functions

#### Sine and Cosine

**Critical requirement**: Use CORDIC or polynomial minimax approximations with range reduction.

```metal
// TODO: Implement using CORDIC algorithm or Chebyshev polynomials
// Requires range reduction (x mod 2π) and lookup tables
inline gpga_double gpga_double_sin(gpga_double x) {
  // PLACEHOLDER - implement CORDIC or polynomial approximation
  return gpga_double_nan();
}

inline gpga_double gpga_double_cos(gpga_double x) {
  // PLACEHOLDER - implement CORDIC or polynomial approximation
  return gpga_double_nan();
}
```

#### Logarithm and Exponential

**Critical requirement**: Use Taylor series or lookup tables with interpolation. Keyword: IEEE compliant, fast solutions

```metal
// TODO: Implement using log2 reduction + Taylor series
inline gpga_double gpga_double_log(gpga_double x) {
  // PLACEHOLDER - implement using log2(x) with polynomial
  return gpga_double_nan();
}

// TODO: Implement using exp2 + polynomial
inline gpga_double gpga_double_exp(gpga_double x) {
  // PLACEHOLDER - implement using exp2(x) with polynomial
  return gpga_double_nan();
}
```

---

## Metal 4 Intrinsics (Mandatory Usage)

Use these Metal-specific functions for optimal GPU execution:

- **`clz(x)`** - Count leading zeros (fast normalization)
- **`ctz(x)`** - Count trailing zeros
- **`popcount(x)`** - Count set bits (useful for rounding)
- **`select(a, b, cond)`** - Conditional move (avoid branches)
- **`mul_hi(a, b)`** - High bits of multiply (for extended precision)
- **`mad_hi(a, b, c)`** - Fused multiply-add on high bits

---

## Testing Requirements

### Unit Tests

All operations must pass bit-exact validation against IEEE 754 reference:

```verilog
module test_softfloat_exact;
  real a, b, result, expected;

  initial begin
    // Test addition
    a = 1.5;
    b = 2.25;
    result = a + b;
    expected = 3.75;
    if (result !== expected) $display("FAIL: add");

    // Test edge cases
    result = 1.0 / 0.0;  // Must produce +inf
    if (result !== 1.0/0.0) $display("FAIL: inf");

    result = 0.0 / 0.0;  // Must produce NaN
    if (result === result) $display("FAIL: nan");  // NaN != NaN

    $finish;
  end
endmodule
```

### Edge Cases (All Must Pass)

- Zero (positive and negative)
- Infinity (positive and negative)
- NaN propagation
- Denormal numbers
- Subnormal results
- Overflow to infinity
- Underflow to zero
- Round-to-nearest-even
- Exact halfway cases

---

## Success Criteria

✅ All `test_real_*.v` tests pass with bit-exact IEEE 754 results
✅ VCD output matches reference simulator exactly
✅ No Metal compilation errors
✅ Full IEEE 754 compliance (no approximations without explicit justification)
✅ Branchless implementations where possible

## Potential Optimization to Consider

IF profiling shows real number operations are a bottleneck, THEN consider:
  - Using SIMD instructions (ulong2, ulong4) to process multiple gpga_double values in parallel
