#ifndef GPGA_REAL_H
#define GPGA_REAL_H

#if defined(__METAL_VERSION__)
#include <metal_stdlib>
using namespace metal;
#else
#include <cstdint>
// Fallback typedefs for editors/non-Metal tooling.
typedef uint32_t uint;
typedef uint64_t ulong;
#ifndef thread
#define thread
#endif
#ifndef constant
#define constant
#endif
#endif

#if defined(__METAL_VERSION__)
#define GPGA_CONST constant
#else
#define GPGA_CONST static const
#endif

// IEEE-754 binary64 helpers and CRlibm-compatible API (MSL).
typedef ulong gpga_double;
inline uint gpga_u64_hi(ulong value);
inline uint gpga_u64_lo(ulong value);
inline gpga_double gpga_u64_from_words(uint hi, uint lo);
inline uint gpga_double_sign(gpga_double d) {
  return (uint)((d >> 63) & 1ul);
}
inline uint gpga_double_exp(gpga_double d) {
  return (uint)((d >> 52) & 0x7FFu);
}
inline ulong gpga_double_mantissa(gpga_double d) {
  return d & 0x000FFFFFFFFFFFFFul;
}
inline int gpga_double_exponent(gpga_double d) {
  uint exp_bits = gpga_double_exp(d);
  if (exp_bits == 0u) {
    ulong mant = gpga_double_mantissa(d);
    if (mant == 0ul) {
      return -1023;
    }
    int e = -1022;
    while ((mant & (1ul << 52)) == 0ul) {
      mant <<= 1;
      e -= 1;
    }
    return e;
  }
  return (int)exp_bits - 1023;
}
inline gpga_double gpga_double_pack(uint sign, uint exp, ulong mantissa) {
  return ((ulong)sign << 63) | ((ulong)exp << 52) |
         (mantissa & 0x000FFFFFFFFFFFFFul);
}
inline gpga_double gpga_double_zero(uint sign) {
  return ((ulong)sign << 63);
}
inline gpga_double gpga_double_inf(uint sign) {
  return gpga_double_pack(sign, 0x7FFu, 0ul);
}
inline gpga_double gpga_double_nan() {
  return 0x7FF8000000000000ul;
}
inline bool gpga_double_is_zero(gpga_double d) {
  return (d & 0x7FFFFFFFFFFFFFFFul) == 0ul;
}
inline bool gpga_double_is_inf(gpga_double d) {
  return (d & 0x7FFFFFFFFFFFFFFFul) == 0x7FF0000000000000ul;
}
inline bool gpga_double_is_nan(gpga_double d) {
  return (gpga_double_exp(d) == 0x7FFu) &&
         (gpga_double_mantissa(d) != 0ul);
}
inline gpga_double gpga_bits_to_real(ulong bits) {
  return bits;
}
inline ulong gpga_real_to_bits(gpga_double value) {
  return value;
}
inline gpga_double gpga_double_abs(gpga_double x) {
  return x & 0x7FFFFFFFFFFFFFFFul;
}
inline gpga_double gpga_double_next_up(gpga_double x) {
  if (gpga_double_is_nan(x) ||
      (gpga_double_is_inf(x) && gpga_double_sign(x) == 0u)) {
    return x;
  }
  if (gpga_double_is_zero(x)) {
    return 0x0000000000000001ul;
  }
  return gpga_double_sign(x) ? (x - 1ul) : (x + 1ul);
}
inline gpga_double gpga_double_next_down(gpga_double x) {
  if (gpga_double_is_nan(x) ||
      (gpga_double_is_inf(x) && gpga_double_sign(x) != 0u)) {
    return x;
  }
  if (gpga_double_is_zero(x)) {
    return 0x8000000000000001ul;
  }
  return gpga_double_sign(x) ? (x + 1ul) : (x - 1ul);
}
inline gpga_double gpga_double_const_one() {
  return gpga_bits_to_real(0x3ff0000000000000ul);
}
inline gpga_double gpga_double_const_two() {
  return gpga_bits_to_real(0x4000000000000000ul);
}
inline gpga_double gpga_double_const_minus_one() {
  return gpga_bits_to_real(0xbff0000000000000ul);
}
inline gpga_double gpga_double_const_ln2() {
  return gpga_bits_to_real(0x3fe62e42fefa39eful);
}
inline gpga_double gpga_double_const_ln10() {
  return gpga_bits_to_real(0x40026bb1bbb55516ul);
}
inline gpga_double gpga_double_const_log10e() {
  return gpga_bits_to_real(0x3fdbcb7b1526e50eul);
}
inline gpga_double gpga_double_const_pi() {
  return gpga_bits_to_real(0x400921fb54442d18ul);
}
inline gpga_double gpga_double_const_two_pi() {
  return gpga_bits_to_real(0x401921fb54442d18ul);
}
inline gpga_double gpga_double_const_half_pi() {
  return gpga_bits_to_real(0x3ff921fb54442d18ul);
}
inline gpga_double gpga_double_const_quarter_pi() {
  return gpga_bits_to_real(0x3fe921fb54442d18ul);
}
inline gpga_double gpga_double_const_inv_ln2() {
  return gpga_bits_to_real(0x3ff71547652b82feul);
}
inline gpga_double gpga_double_const_inv_two_pi() {
  return gpga_bits_to_real(0x3fc45f306dc9c883ul);
}
inline gpga_double gpga_double_const_inv2() {
  return gpga_bits_to_real(0x3fe0000000000000ul);
}
inline gpga_double gpga_double_const_inv3() {
  return gpga_bits_to_real(0x3fd5555555555555ul);
}
inline gpga_double gpga_double_const_inv5() {
  return gpga_bits_to_real(0x3fc999999999999aul);
}
inline gpga_double gpga_double_const_inv7() {
  return gpga_bits_to_real(0x3fc2492492492492ul);
}
inline gpga_double gpga_double_const_inv9() {
  return gpga_bits_to_real(0x3fbc71c71c71c71cul);
}
inline gpga_double gpga_double_const_inv11() {
  return gpga_bits_to_real(0x3fb745d1745d1746ul);
}
inline gpga_double gpga_double_const_inv13() {
  return gpga_bits_to_real(0x3fb3b13b13b13b14ul);
}
inline gpga_double gpga_double_const_inv15() {
  return gpga_bits_to_real(0x3fb1111111111111ul);
}
inline gpga_double gpga_double_const_inv6() {
  return gpga_bits_to_real(0x3fc5555555555555ul);
}
inline gpga_double gpga_double_const_inv24() {
  return gpga_bits_to_real(0x3fa5555555555555ul);
}
inline gpga_double gpga_double_const_inv120() {
  return gpga_bits_to_real(0x3f81111111111111ul);
}
inline gpga_double gpga_double_const_inv720() {
  return gpga_bits_to_real(0x3f56c16c16c16c17ul);
}
inline gpga_double gpga_double_const_inv5040() {
  return gpga_bits_to_real(0x3f2a01a01a01a01aul);
}
inline gpga_double gpga_double_const_inv40320() {
  return gpga_bits_to_real(0x3efa01a01a01a01aul);
}
inline gpga_double gpga_double_const_inv362880() {
  return gpga_bits_to_real(0x3ec71de3a556c734ul);
}
inline gpga_double gpga_double_const_inv3628800() {
  return gpga_bits_to_real(0x3e927e4fb7789f5cul);
}
inline gpga_double gpga_double_const_inv39916800() {
  return gpga_bits_to_real(0x3e5ae64567f544e4ul);
}
inline gpga_double gpga_double_const_inv479001600() {
  return gpga_bits_to_real(0x3e21eed8eff8d898ul);
}
inline gpga_double gpga_double_const_inv6227020800() {
  return gpga_bits_to_real(0x3de6124613a86d09ul);
}
GPGA_CONST gpga_double SQRTPOLYC0 = 0x400407e3bff0ce0ful;
GPGA_CONST gpga_double SQRTPOLYC1 = 0xc00a618de0a521aful;
GPGA_CONST gpga_double SQRTPOLYC2 = 0x40060edebae5c173ul;
GPGA_CONST gpga_double SQRTPOLYC3 = 0xbff26ff93141fe48ul;
GPGA_CONST gpga_double SQRTPOLYC4 = 0x3fc7ec5765014684ul;
GPGA_CONST gpga_double SQRTTWO52 = 0x4330000000000000ul;
inline uint gpga_clz64(ulong value) {
  if (value == 0ul) {
    return 64u;
  }
  uint count = 0u;
  ulong mask = 1ul << 63;
  while ((value & mask) == 0ul) {
    count += 1u;
    mask >>= 1;
  }
  return count;
}
inline ulong gpga_shift_right_sticky(ulong value, uint shift) {
  if (shift == 0u) {
    return value;
  }
  if (shift >= 64u) {
    return (value != 0ul) ? 1ul : 0ul;
  }
  ulong mask = (1ul << shift) - 1ul;
  ulong sticky = (value & mask) ? 1ul : 0ul;
  ulong shifted = value >> shift;
  return shifted | sticky;
}
inline void gpga_mul_64(ulong a, ulong b, thread ulong* hi,
                         thread ulong* lo) {
  ulong a_lo = a & 0xFFFFFFFFul;
  ulong a_hi = a >> 32;
  ulong b_lo = b & 0xFFFFFFFFul;
  ulong b_hi = b >> 32;
  ulong p0 = a_lo * b_lo;
  ulong p1 = a_lo * b_hi;
  ulong p2 = a_hi * b_lo;
  ulong p3 = a_hi * b_hi;
  ulong mid = (p0 >> 32) + (p1 & 0xFFFFFFFFul) + (p2 & 0xFFFFFFFFul);
  *lo = (p0 & 0xFFFFFFFFul) | (mid << 32);
  *hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
}
inline ulong gpga_shift_right_sticky_128(ulong hi, ulong lo,
                                         uint shift) {
  if (shift == 0u) {
    return lo;
  }
  if (shift >= 128u) {
    return (hi | lo) ? 1ul : 0ul;
  }
  if (shift >= 64u) {
    uint s = shift - 64u;
    ulong shifted = (s >= 64u) ? 0ul : (hi >> s);
    ulong lost = lo;
    if (s < 64u) {
      ulong mask = (s == 0u) ? 0ul : ((1ul << s) - 1ul);
      lost |= (hi & mask);
    }
    if (lost != 0ul) {
      shifted |= 1ul;
    }
    return shifted;
  }
  ulong shifted = (hi << (64u - shift)) | (lo >> shift);
  ulong mask = (shift == 0u) ? 0ul : ((1ul << shift) - 1ul);
  ulong lost = lo & mask;
  if (lost != 0ul) {
    shifted |= 1ul;
  }
  return shifted;
}
inline gpga_double gpga_double_round_pack(uint sign, int exp,
                                          ulong mant_ext) {
  ulong sig = mant_ext >> 3;
  uint guard = (uint)((mant_ext >> 2) & 1ul);
  uint round = (uint)((mant_ext >> 1) & 1ul);
  uint sticky = (uint)(mant_ext & 1ul);
  if (guard != 0u && (round != 0u || sticky != 0u || (sig & 1ul))) {
    sig += 1ul;
  }
  if ((sig & (1ul << 53)) != 0ul) {
    sig >>= 1;
    exp += 1;
  }
  if (exp >= 1024) {
    return gpga_double_inf(sign);
  }
  if (exp < -1022) {
    int shift = -1022 - exp;
    if (shift >= 64) {
      return gpga_double_zero(sign);
    }
    ulong den_ext = gpga_shift_right_sticky(sig << 3, (uint)shift);
    ulong den_sig = den_ext >> 3;
    uint g = (uint)((den_ext >> 2) & 1ul);
    uint r = (uint)((den_ext >> 1) & 1ul);
    uint s = (uint)(den_ext & 1ul);
    if (g != 0u && (r != 0u || s != 0u || (den_sig & 1ul))) {
      den_sig += 1ul;
    }
    if (den_sig >= (1ul << 52)) {
      return gpga_double_pack(sign, 1u, 0ul);
    }
    return gpga_double_pack(sign, 0u,
                            den_sig & 0x000FFFFFFFFFFFFFul);
  }
  uint exp_bits = (uint)(exp + 1023);
  return gpga_double_pack(sign, exp_bits,
                          sig & 0x000FFFFFFFFFFFFFul);
}
inline gpga_double gpga_double_ldexp(gpga_double x, int exp) {
  if (gpga_double_is_zero(x) || gpga_double_is_inf(x) ||
      gpga_double_is_nan(x)) {
    return x;
  }
  uint sign = gpga_double_sign(x);
  uint exp_bits = gpga_double_exp(x);
  ulong mant = gpga_double_mantissa(x);
  int e = 0;
  if (exp_bits == 0u) {
    if (mant == 0ul) {
      return gpga_double_zero(sign);
    }
    e = -1022;
    while ((mant & (1ul << 52)) == 0ul) {
      mant <<= 1;
      e -= 1;
    }
    mant &= 0x000FFFFFFFFFFFFFul;
  } else {
    e = (int)exp_bits - 1023;
  }
  e += exp;
  if (e >= 1024) {
    return gpga_double_inf(sign);
  }
  if (e <= -1023) {
    int shift = -1022 - e;
    if (shift >= 64) {
      return gpga_double_zero(sign);
    }
    ulong sig = mant | (1ul << 52);
    ulong ext = gpga_shift_right_sticky(sig << 3, (uint)shift);
    return gpga_double_round_pack(sign, -1022, ext);
  }
  return gpga_double_pack(sign, (uint)(e + 1023), mant);
}
inline gpga_double gpga_double_frexp(gpga_double x, thread int* exp_out) {
  if (gpga_double_is_zero(x) || gpga_double_is_inf(x) ||
      gpga_double_is_nan(x)) {
    if (exp_out) {
      *exp_out = 0;
    }
    return x;
  }
  uint sign = gpga_double_sign(x);
  uint exp_bits = gpga_double_exp(x);
  ulong mant = gpga_double_mantissa(x);
  int e = 0;
  if (exp_bits == 0u) {
    e = -1022;
    while ((mant & (1ul << 52)) == 0ul) {
      mant <<= 1;
      e -= 1;
    }
    mant &= 0x000FFFFFFFFFFFFFul;
  } else {
    e = (int)exp_bits - 1023;
  }
  if (exp_out) {
    *exp_out = e + 1;
  }
  return gpga_double_pack(sign, 1022u, mant);
}
inline gpga_double gpga_double_from_u64(ulong value) {
  if (value == 0ul) {
    return gpga_double_zero(0u);
  }
  uint lz = gpga_clz64(value);
  uint msb = 63u - lz;
  int exp = (int)msb;
  if (msb <= 52u) {
    ulong mant = (value << (52u - msb)) & 0x000FFFFFFFFFFFFFul;
    return gpga_double_pack(0u, (uint)(exp + 1023), mant);
  }
  uint shift = msb - 52u;
  ulong mant_full = value >> shift;
  ulong mant = mant_full & 0x000FFFFFFFFFFFFFul;
  ulong lost = value & ((1ul << shift) - 1ul);
  if (shift != 0u) {
    ulong round_half = 1ul << (shift - 1u);
    if (lost > round_half || (lost == round_half && (mant & 1ul))) {
      mant += 1ul;
      if (mant == (1ul << 52)) {
        mant = 0ul;
        exp += 1;
      }
    }
  }
  if (exp >= 1024) {
    return gpga_double_inf(0u);
  }
  return gpga_double_pack(0u, (uint)(exp + 1023), mant);
}
inline gpga_double gpga_double_from_s64(long value) {
  if (value == 0l) {
    return gpga_double_zero(0u);
  }
  uint sign = (value < 0l) ? 1u : 0u;
  ulong abs_val = (value < 0l)
      ? (ulong)((~(ulong)value) + 1ul)
      : (ulong)value;
  gpga_double out = gpga_double_from_u64(abs_val);
  if (sign != 0u) {
    out ^= (1ul << 63);
  }
  return out;
}
inline gpga_double gpga_double_from_u32(uint value) {
  return gpga_double_from_u64((ulong)value);
}
inline gpga_double gpga_double_from_s32(int value) {
  return gpga_double_from_s64((long)value);
}
inline long gpga_double_to_s64(gpga_double d) {
  if (gpga_double_is_nan(d) || gpga_double_is_inf(d)) {
    return 0l;
  }
  uint sign = gpga_double_sign(d);
  uint exp_bits = gpga_double_exp(d);
  ulong mant = gpga_double_mantissa(d);
  if (exp_bits == 0u) {
    return 0l;
  }
  int exp = (int)exp_bits - 1023;
  if (exp < 0) {
    return 0l;
  }
  if (exp > 62) {
    return 0l;
  }
  ulong sig = mant | (1ul << 52);
  ulong val = (exp >= 52) ? (sig << (uint)(exp - 52))
                            : (sig >> (uint)(52 - exp));
  long out = (long)val;
  return sign ? -out : out;
}
inline int gpga_double_to_s32(gpga_double d) {
  return (int)gpga_double_to_s64(d);
}
inline long gpga_double_round_to_s64(gpga_double d) {
  if (gpga_double_is_nan(d) || gpga_double_is_inf(d)) {
    return 0l;
  }
  uint sign = gpga_double_sign(d);
  gpga_double abs = gpga_double_abs(d);
  uint exp_bits = gpga_double_exp(abs);
  if (exp_bits == 0u) {
    return 0l;
  }
  int exp = (int)exp_bits - 1023;
  ulong mant = gpga_double_mantissa(abs) | (1ull << 52);
  ulong int_part = 0ul;
  if (exp < 0) {
    if (exp == -1) {
      ulong half_val = 1ull << 52;
      if (mant > half_val) {
        int_part = 1ul;
      } else {
        int_part = 0ul;
      }
    } else {
      int_part = 0ul;
    }
  } else if (exp >= 52) {
    int_part = mant << (uint)(exp - 52);
  } else {
    uint shift = (uint)(52 - exp);
    int_part = mant >> shift;
    ulong rem = mant & ((1ull << shift) - 1ull);
    ulong half_val = 1ull << (shift - 1u);
    if (rem > half_val) {
      int_part += 1ull;
    } else if (rem == half_val) {
      if ((int_part & 1ull) != 0ull) {
        int_part += 1ull;
      }
    }
  }
  long out = (long)int_part;
  return sign ? -out : out;
}
inline gpga_double gpga_double_neg(gpga_double d) {
  return d ^ (1ul << 63);
}
inline gpga_double gpga_double_negate(gpga_double d) {
  return gpga_double_neg(d);
}
inline gpga_double gpga_double_add(gpga_double a, gpga_double b) {
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) {
    return gpga_double_nan();
  }
  if (gpga_double_is_inf(a)) {
    if (gpga_double_is_inf(b) &&
        (gpga_double_sign(a) != gpga_double_sign(b))) {
      return gpga_double_nan();
    }
    return a;
  }
  if (gpga_double_is_inf(b)) {
    return b;
  }
  if (gpga_double_is_zero(a)) {
    return b;
  }
  if (gpga_double_is_zero(b)) {
    return a;
  }
  uint sign_a = gpga_double_sign(a);
  uint sign_b = gpga_double_sign(b);
  uint exp_a_bits = gpga_double_exp(a);
  uint exp_b_bits = gpga_double_exp(b);
  long exp_a = (exp_a_bits == 0u) ? -1022 : (long)exp_a_bits - 1023;
  long exp_b = (exp_b_bits == 0u) ? -1022 : (long)exp_b_bits - 1023;
  ulong mant_a = gpga_double_mantissa(a);
  ulong mant_b = gpga_double_mantissa(b);
  if (exp_a_bits != 0u) {
    mant_a |= (1ul << 52);
  } else if (mant_a != 0ul) {
    while ((mant_a & (1ul << 52)) == 0ul) {
      mant_a <<= 1;
      exp_a -= 1;
    }
  }
  if (exp_b_bits != 0u) {
    mant_b |= (1ul << 52);
  } else if (mant_b != 0ul) {
    while ((mant_b & (1ul << 52)) == 0ul) {
      mant_b <<= 1;
      exp_b -= 1;
    }
  }
  if (exp_a < exp_b) {
    long tmp_exp = exp_a;
    exp_a = exp_b;
    exp_b = tmp_exp;
    uint tmp_sign = sign_a;
    sign_a = sign_b;
    sign_b = tmp_sign;
    ulong tmp_mant = mant_a;
    mant_a = mant_b;
    mant_b = tmp_mant;
  }
  uint diff = (uint)(exp_a - exp_b);
  ulong mant_a_ext = mant_a << 3;
  ulong mant_b_ext = gpga_shift_right_sticky(mant_b << 3, diff);
  ulong mant_ext = 0ul;
  uint sign = sign_a;
  if (sign_a == sign_b) {
    mant_ext = mant_a_ext + mant_b_ext;
    if ((mant_ext & (1ul << 56)) != 0ul) {
      mant_ext = gpga_shift_right_sticky(mant_ext, 1u);
      exp_a += 1;
    }
  } else {
    if (mant_a_ext >= mant_b_ext) {
      mant_ext = mant_a_ext - mant_b_ext;
      sign = sign_a;
    } else {
      mant_ext = mant_b_ext - mant_a_ext;
      sign = sign_b;
      exp_a = exp_b;
    }
    if (mant_ext == 0ul) {
      return gpga_double_zero(0u);
    }
    while ((mant_ext & (1ul << 55)) == 0ul && exp_a > -1022) {
      mant_ext <<= 1;
      exp_a -= 1;
    }
  }
  return gpga_double_round_pack(sign, (int)exp_a, mant_ext);
}
inline gpga_double gpga_double_sub(gpga_double a, gpga_double b) {
  return gpga_double_add(a, gpga_double_neg(b));
}
inline gpga_double gpga_double_mul(gpga_double a, gpga_double b) {
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) {
    return gpga_double_nan();
  }
  if (gpga_double_is_inf(a) || gpga_double_is_inf(b)) {
    if (gpga_double_is_zero(a) || gpga_double_is_zero(b)) {
      return gpga_double_nan();
    }
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_inf(sign);
  }
  if (gpga_double_is_zero(a) || gpga_double_is_zero(b)) {
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_zero(sign);
  }
  uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
  uint exp_a_bits = gpga_double_exp(a);
  uint exp_b_bits = gpga_double_exp(b);
  long exp_a = (exp_a_bits == 0u) ? -1022 : (long)exp_a_bits - 1023;
  long exp_b = (exp_b_bits == 0u) ? -1022 : (long)exp_b_bits - 1023;
  ulong mant_a = gpga_double_mantissa(a);
  ulong mant_b = gpga_double_mantissa(b);
  if (exp_a_bits != 0u) {
    mant_a |= (1ul << 52);
  } else if (mant_a != 0ul) {
    while ((mant_a & (1ul << 52)) == 0ul) {
      mant_a <<= 1;
      exp_a -= 1;
    }
  }
  if (exp_b_bits != 0u) {
    mant_b |= (1ul << 52);
  } else if (mant_b != 0ul) {
    while ((mant_b & (1ul << 52)) == 0ul) {
      mant_b <<= 1;
      exp_b -= 1;
    }
  }
  long exp = exp_a + exp_b;
  ulong hi = 0ul;
  ulong lo = 0ul;
  gpga_mul_64(mant_a, mant_b, &hi, &lo);
  bool msb = (hi & (1ul << 41)) != 0ul;
  if (msb) {
    exp += 1;
  }
  uint shift = msb ? 50u : 49u;
  ulong mant_ext = gpga_shift_right_sticky_128(hi, lo, shift);
  return gpga_double_round_pack(sign, (int)exp, mant_ext);
}
inline ulong gpga_div_mantissa(ulong num, ulong den) {
  if (den == 0ul) {
    return 0ul;
  }
  // num is a normalized 53-bit mantissa. We need 56 quotient bits from
  // (num << 55) / den. Run long division over all 108 numerator bits and
  // keep the lower 56 quotient bits.
  ulong hi = num >> 9;
  ulong lo = num << 55;
  ulong rem = 0ul;
  ulong quot = 0ul;
  for (uint i = 0u; i < 108u; ++i) {
    uint bit_index = 107u - i;
    ulong bit = 0ul;
    if (bit_index >= 64u) {
      bit = (hi >> (bit_index - 64u)) & 1ul;
    } else {
      bit = (lo >> bit_index) & 1ul;
    }
    rem = (rem << 1) | bit;
    ulong out_bit = 0ul;
    if (rem >= den) {
      rem -= den;
      out_bit = 1ul;
    }
    if (i >= 52u) {
      quot = (quot << 1) | out_bit;
    }
  }
  if (rem != 0ul) {
    quot |= 1ul;
  }
  return quot;
}
inline gpga_double gpga_double_div(gpga_double a, gpga_double b) {
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) {
    return gpga_double_nan();
  }
  if (gpga_double_is_zero(b)) {
    if (gpga_double_is_zero(a)) {
      return gpga_double_nan();
    }
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_inf(sign);
  }
  if (gpga_double_is_zero(a)) {
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_zero(sign);
  }
  if (gpga_double_is_inf(a)) {
    if (gpga_double_is_inf(b)) {
      return gpga_double_nan();
    }
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_inf(sign);
  }
  if (gpga_double_is_inf(b)) {
    uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
    return gpga_double_zero(sign);
  }
  uint sign = gpga_double_sign(a) ^ gpga_double_sign(b);
  uint exp_a_bits = gpga_double_exp(a);
  uint exp_b_bits = gpga_double_exp(b);
  long exp_a = (exp_a_bits == 0u) ? -1022 : (long)exp_a_bits - 1023;
  long exp_b = (exp_b_bits == 0u) ? -1022 : (long)exp_b_bits - 1023;
  ulong mant_a = gpga_double_mantissa(a);
  ulong mant_b = gpga_double_mantissa(b);
  if (exp_a_bits != 0u) {
    mant_a |= (1ul << 52);
  } else if (mant_a != 0ul) {
    while ((mant_a & (1ul << 52)) == 0ul) {
      mant_a <<= 1;
      exp_a -= 1;
    }
  }
  if (exp_b_bits != 0u) {
    mant_b |= (1ul << 52);
  } else if (mant_b != 0ul) {
    while ((mant_b & (1ul << 52)) == 0ul) {
      mant_b <<= 1;
      exp_b -= 1;
    }
  }
  long exp = exp_a - exp_b;
  ulong mant_ext = gpga_div_mantissa(mant_a, mant_b);
  if (mant_ext < (1ul << 55)) {
    mant_ext <<= 1;
    exp -= 1;
  }
  return gpga_double_round_pack(sign, (int)exp, mant_ext);
}
inline bool gpga_double_eq(gpga_double a, gpga_double b) {
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) {
    return false;
  }
  if (gpga_double_is_zero(a) && gpga_double_is_zero(b)) {
    return true;
  }
  return a == b;
}
inline bool gpga_double_lt(gpga_double a, gpga_double b) {
  if (gpga_double_is_nan(a) || gpga_double_is_nan(b)) {
    return false;
  }
  if (gpga_double_is_zero(a) && gpga_double_is_zero(b)) {
    return false;
  }
  uint sign_a = gpga_double_sign(a);
  uint sign_b = gpga_double_sign(b);
  if (sign_a != sign_b) {
    return sign_a > sign_b;
  }
  ulong mag_a = a & 0x7FFFFFFFFFFFFFFFul;
  ulong mag_b = b & 0x7FFFFFFFFFFFFFFFul;
  if (sign_a != 0u) {
    return mag_a > mag_b;
  }
  return mag_a < mag_b;
}
inline bool gpga_double_le(gpga_double a, gpga_double b) {
  return gpga_double_lt(a, b) || gpga_double_eq(a, b);
}
inline bool gpga_double_gt(gpga_double a, gpga_double b) {
  return gpga_double_lt(b, a);
}
inline bool gpga_double_ge(gpga_double a, gpga_double b) {
  return gpga_double_gt(a, b) || gpga_double_eq(a, b);
}
inline gpga_double gpga_double_floor(gpga_double x) {
  if (gpga_double_is_nan(x) || gpga_double_is_inf(x) ||
      gpga_double_is_zero(x)) {
    return x;
  }
  long t = gpga_double_to_s64(x);
  gpga_double t_real = gpga_double_from_s64(t);
  if (gpga_double_eq(x, t_real)) {
    return x;
  }
  if (gpga_double_sign(x)) {
    return gpga_double_from_s64(t - 1l);
  }
  return t_real;
}
inline gpga_double gpga_double_ceil(gpga_double x) {
  if (gpga_double_is_nan(x) || gpga_double_is_inf(x) ||
      gpga_double_is_zero(x)) {
    return x;
  }
  long t = gpga_double_to_s64(x);
  gpga_double t_real = gpga_double_from_s64(t);
  if (gpga_double_eq(x, t_real)) {
    return x;
  }
  if (gpga_double_sign(x) == 0u) {
    return gpga_double_from_s64(t + 1l);
  }
  return t_real;
}
inline gpga_double gpga_log_rn(gpga_double x);
inline gpga_double gpga_log2_rn(gpga_double x);
inline gpga_double gpga_log10_rn(gpga_double x);
inline gpga_double gpga_double_ln(gpga_double x) {
  return gpga_log_rn(x);
}
inline gpga_double gpga_double_log2(gpga_double x) {
  return gpga_log2_rn(x);
}
inline gpga_double gpga_double_log10(gpga_double x) {
  return gpga_log10_rn(x);
}

inline gpga_double gpga_exp_rn(gpga_double x);
inline gpga_double gpga_exp_rd(gpga_double x);
inline gpga_double gpga_exp_ru(gpga_double x);
inline gpga_double gpga_exp2_rn(gpga_double x);
inline gpga_double gpga_exp2_rd(gpga_double x);
inline gpga_double gpga_exp2_ru(gpga_double x);
inline gpga_double gpga_exp2_rz(gpga_double x);
inline void gpga_pow_exp2_120_core(thread int* H, thread gpga_double* resh,
                                   thread gpga_double* resm,
                                   thread gpga_double* resl, gpga_double xh,
                                   gpga_double xm, gpga_double xl);
inline void gpga_sqrt13(thread gpga_double* resh, thread gpga_double* resm,
                        thread gpga_double* resl, gpga_double x);
inline gpga_double ReturnRoundToNearest3(gpga_double xh, gpga_double xm,
                                         gpga_double xl);
inline gpga_double gpga_double_exp_real(gpga_double x) {
  return gpga_exp_rn(x);
}
inline gpga_double gpga_expm1_rn(gpga_double x);
inline gpga_double gpga_double_expm1(gpga_double x) {
  return gpga_expm1_rn(x);
}
inline gpga_double gpga_double_sqrt(gpga_double x) {
  if (gpga_double_is_nan(x)) {
    return x;
  }
  if (gpga_double_sign(x)) {
    return gpga_double_nan();
  }
  if (gpga_double_is_zero(x) || gpga_double_is_inf(x)) {
    return x;
  }
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_sqrt13(&rh, &rm, &rl, x);
  return ReturnRoundToNearest3(rh, rm, rl);
}

inline gpga_double scs_sin_rn(gpga_double x);
inline gpga_double scs_cos_rn(gpga_double x);
inline gpga_double scs_tan_rn(gpga_double x);
inline gpga_double gpga_sin_rn(gpga_double x);
inline gpga_double gpga_cos_rn(gpga_double x);
inline gpga_double gpga_tan_rn(gpga_double x);

inline gpga_double gpga_double_sin(gpga_double x) {
  return gpga_sin_rn(x);
}
inline gpga_double gpga_double_cos(gpga_double x) {
  return gpga_cos_rn(x);
}
inline gpga_double gpga_double_tan(gpga_double x) {
  return gpga_tan_rn(x);
}
inline gpga_double gpga_sinh_rn(gpga_double x);
inline gpga_double gpga_cosh_rn(gpga_double x);
inline gpga_double gpga_double_sinh(gpga_double x) {
  return gpga_sinh_rn(x);
}
inline gpga_double gpga_double_cosh(gpga_double x) {
  return gpga_cosh_rn(x);
}
inline gpga_double gpga_double_atan_series(gpga_double x) {
  gpga_double x2 = gpga_double_mul(x, x);
  gpga_double term = x;
  gpga_double sum = term;
  term = gpga_double_mul(term, x2);
  sum = gpga_double_sub(sum, gpga_double_mul(term, gpga_double_const_inv3()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_add(sum, gpga_double_mul(term, gpga_double_const_inv5()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_sub(sum, gpga_double_mul(term, gpga_double_const_inv7()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_add(sum, gpga_double_mul(term, gpga_double_const_inv9()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_sub(sum, gpga_double_mul(term, gpga_double_const_inv11()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_add(sum, gpga_double_mul(term, gpga_double_const_inv13()));
  term = gpga_double_mul(term, x2);
  sum = gpga_double_sub(sum, gpga_double_mul(term, gpga_double_const_inv15()));
  return sum;
}

inline gpga_double gpga_atan_rn(gpga_double x);
inline gpga_double gpga_atan_rd(gpga_double x);
inline gpga_double gpga_atan_ru(gpga_double x);
inline gpga_double gpga_atan_rz(gpga_double x);
inline gpga_double gpga_atanpi_rn(gpga_double x);
inline gpga_double gpga_atanpi_rd(gpga_double x);
inline gpga_double gpga_atanpi_ru(gpga_double x);
inline gpga_double gpga_atanpi_rz(gpga_double x);

inline gpga_double gpga_double_atan(gpga_double x) {
  return gpga_atan_rn(x);
}
inline gpga_double gpga_double_asin(gpga_double x) {
  if (gpga_double_is_nan(x)) {
    return x;
  }
  gpga_double one = gpga_double_from_u32(1u);
  if (gpga_double_gt(gpga_double_abs(x), one)) {
    return gpga_double_nan();
  }
  gpga_double x2 = gpga_double_mul(x, x);
  gpga_double denom = gpga_double_sqrt(gpga_double_sub(one, x2));
  gpga_double ratio = gpga_double_div(x, denom);
  return gpga_double_atan(ratio);
}
inline gpga_double gpga_double_acos(gpga_double x) {
  gpga_double half_pi = gpga_double_const_half_pi();
  return gpga_double_sub(half_pi, gpga_double_asin(x));
}
inline gpga_double gpga_double_pow_int(gpga_double base, long exp) {
  if (exp == 0l) {
    return gpga_double_from_u32(1u);
  }
  bool neg = (exp < 0l);
  if (neg) {
    exp = -exp;
  }
  gpga_double result = gpga_double_from_u32(1u);
  gpga_double factor = base;
  while (exp > 0l) {
    if ((exp & 1l) != 0l) {
      result = gpga_double_mul(result, factor);
    }
    factor = gpga_double_mul(factor, factor);
    exp >>= 1l;
  }
  if (neg) {
    result = gpga_double_div(gpga_double_from_u32(1u), result);
  }
  return result;
}
inline gpga_double gpga_pow_rn(gpga_double x, gpga_double y);
inline gpga_double gpga_double_pow(gpga_double base, gpga_double exp) {
  return gpga_pow_rn(base, exp);
}

// ---------------------------------------------------------------------------
// CRlibm internal helpers (double-double / triple-double scaffolding).

struct GpgaDd {
  gpga_double hi;
  gpga_double lo;
};

struct GpgaTd {
  gpga_double hi;
  gpga_double mid;
  gpga_double lo;
};

inline gpga_double gpga_double_dekker_const() {
  // 2^27 + 1, used for Dekker splitting.
  return gpga_double_from_u32(134217729u);
}

inline void Add12(thread gpga_double* s, thread gpga_double* r,
                  gpga_double a, gpga_double b) {
  gpga_double sum = gpga_double_add(a, b);
  gpga_double z = gpga_double_sub(sum, a);
  *s = sum;
  *r = gpga_double_sub(b, z);
}

inline void Add12Cond(thread gpga_double* s, thread gpga_double* r,
                      gpga_double a, gpga_double b) {
  gpga_double sum = gpga_double_add(a, b);
  gpga_double u1 = gpga_double_sub(sum, a);
  gpga_double u2 = gpga_double_sub(sum, u1);
  gpga_double u3 = gpga_double_sub(b, u1);
  gpga_double u4 = gpga_double_sub(a, u2);
  *s = sum;
  *r = gpga_double_add(u4, u3);
}

inline void Add22(thread gpga_double* zh, thread gpga_double* zl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl) {
  gpga_double r = gpga_double_add(xh, yh);
  gpga_double s = gpga_double_add(
      gpga_double_add(gpga_double_add(gpga_double_sub(xh, r), yh), yl), xl);
  gpga_double z = gpga_double_add(r, s);
  *zh = z;
  *zl = gpga_double_add(gpga_double_sub(r, z), s);
}

inline void Add22Cond(thread gpga_double* zh, thread gpga_double* zl,
                      gpga_double xh, gpga_double xl,
                      gpga_double yh, gpga_double yl) {
  gpga_double v1 = gpga_double_zero(0u);
  gpga_double v2 = gpga_double_zero(0u);
  Add12Cond(&v1, &v2, xh, yh);
  gpga_double v3 = gpga_double_add(xl, yl);
  gpga_double v4 = gpga_double_add(v2, v3);
  Add12(zh, zl, v1, v4);
}

inline void Mul12(thread gpga_double* rh, thread gpga_double* rl,
                  gpga_double u, gpga_double v) {
  gpga_double c = gpga_double_dekker_const();
  gpga_double up = gpga_double_mul(u, c);
  gpga_double vp = gpga_double_mul(v, c);
  gpga_double u1 = gpga_double_add(gpga_double_sub(u, up), up);
  gpga_double v1 = gpga_double_add(gpga_double_sub(v, vp), vp);
  gpga_double u2 = gpga_double_sub(u, u1);
  gpga_double v2 = gpga_double_sub(v, v1);
  gpga_double prod = gpga_double_mul(u, v);
  gpga_double err = gpga_double_add(
      gpga_double_add(gpga_double_add(
                          gpga_double_sub(gpga_double_mul(u1, v1), prod),
                          gpga_double_mul(u1, v2)),
                      gpga_double_mul(u2, v1)),
      gpga_double_mul(u2, v2));
  *rh = prod;
  *rl = err;
}

inline gpga_double gpga_double_fma(gpga_double a, gpga_double b,
                                   gpga_double c) {
  gpga_double prod_hi = gpga_double_zero(0u);
  gpga_double prod_lo = gpga_double_zero(0u);
  Mul12(&prod_hi, &prod_lo, a, b);
  gpga_double sum_hi = gpga_double_zero(0u);
  gpga_double sum_lo = gpga_double_zero(0u);
  Add22(&sum_hi, &sum_lo, prod_hi, prod_lo, c, gpga_double_zero(0u));
  return sum_hi;
}

inline void Mul12Cond(thread gpga_double* rh, thread gpga_double* rl,
                      gpga_double a, gpga_double b) {
  Mul12(rh, rl, a, b);
}

inline void Mul22(thread gpga_double* zh, thread gpga_double* zl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl) {
  gpga_double mh = gpga_double_zero(0u);
  gpga_double ml = gpga_double_zero(0u);
  Mul12(&mh, &ml, xh, yh);
  ml = gpga_double_add(ml,
                       gpga_double_add(gpga_double_mul(xh, yl),
                                       gpga_double_mul(xl, yh)));
  Add12(zh, zl, mh, ml);
}

inline void Mul22Cond(thread gpga_double* zh, thread gpga_double* zl,
                      gpga_double xh, gpga_double xl,
                      gpga_double yh, gpga_double yl) {
  Mul22(zh, zl, xh, xl, yh, yl);
}

inline void Fast2Sum(thread gpga_double* s, thread gpga_double* r,
                     gpga_double a, gpga_double b) {
  gpga_double sum = gpga_double_add(a, b);
  gpga_double z = gpga_double_sub(sum, a);
  *s = sum;
  *r = gpga_double_sub(b, z);
}

inline void Fast3Sum(thread gpga_double* r1, thread gpga_double* r2,
                     thread gpga_double* r3, gpga_double a, gpga_double b,
                     gpga_double c) {
  gpga_double u = gpga_double_zero(0u);
  gpga_double v = gpga_double_zero(0u);
  gpga_double w = gpga_double_zero(0u);
  Fast2Sum(&u, &v, b, c);
  Fast2Sum(r1, &w, a, u);
  Fast2Sum(r2, r3, w, v);
}

inline void Mul122(thread gpga_double* resh, thread gpga_double* resl,
                   gpga_double a, gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Mul12(&t1, &t2, a, bh);
  gpga_double t3 = gpga_double_mul(a, bl);
  gpga_double t4 = gpga_double_add(t2, t3);
  Add12(resh, resl, t1, t4);
}

inline void MulAdd212(thread gpga_double* resh, thread gpga_double* resl,
                      gpga_double ch, gpga_double cl, gpga_double a,
                      gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  Mul12(&t1, &t2, a, bh);
  Add12(&t3, &t4, ch, t1);
  t5 = gpga_double_mul(bl, a);
  t6 = gpga_double_add(cl, t2);
  t7 = gpga_double_add(t5, t6);
  t8 = gpga_double_add(t7, t4);
  Add12(resh, resl, t3, t8);
}

inline void MulAdd22(thread gpga_double* resh, thread gpga_double* resl,
                     gpga_double ch, gpga_double cl,
                     gpga_double ah, gpga_double al,
                     gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  Mul12(&t1, &t2, ah, bh);
  Add12(&t3, &t4, ch, t1);
  gpga_double t5 = gpga_double_mul(ah, bl);
  gpga_double t6 = gpga_double_mul(al, bh);
  gpga_double t7 = gpga_double_add(t2, cl);
  gpga_double t8 = gpga_double_add(t4, t7);
  gpga_double t9 = gpga_double_add(t5, t6);
  gpga_double t10 = gpga_double_add(t8, t9);
  Add12(resh, resl, t3, t10);
}

inline void Add122(thread gpga_double* resh, thread gpga_double* resl,
                   gpga_double a, gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, a, bh);
  gpga_double t3 = gpga_double_add(t2, bl);
  Add12(resh, resl, t1, t3);
}

inline void Add122Cond(thread gpga_double* resh, thread gpga_double* resl,
                       gpga_double a, gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&t1, &t2, a, bh);
  gpga_double t3 = gpga_double_add(t2, bl);
  Add12(resh, resl, t1, t3);
}

inline void Add212(thread gpga_double* resh, thread gpga_double* resl,
                   gpga_double ah, gpga_double al, gpga_double b) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, ah, b);
  gpga_double t3 = gpga_double_add(t2, al);
  Add12(resh, resl, t1, t3);
}

inline void Div22(thread gpga_double* pzh, thread gpga_double* pzl,
                  gpga_double xh, gpga_double xl,
                  gpga_double yh, gpga_double yl) {
  gpga_double ch = gpga_double_div(xh, yh);
  gpga_double uh = gpga_double_zero(0u);
  gpga_double ul = gpga_double_zero(0u);
  Mul12(&uh, &ul, ch, yh);
  gpga_double cl = gpga_double_sub(xh, uh);
  cl = gpga_double_sub(cl, ul);
  cl = gpga_double_add(cl, xl);
  cl = gpga_double_sub(cl, gpga_double_mul(ch, yl));
  cl = gpga_double_div(cl, yh);
  gpga_double z = gpga_double_add(ch, cl);
  *pzh = z;
  *pzl = gpga_double_add(gpga_double_sub(ch, z), cl);
}

inline void Renormalize3(thread gpga_double* resh, thread gpga_double* resm,
                         thread gpga_double* resl, gpga_double ah,
                         gpga_double am, gpga_double al) {
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  Add12(&t1h, &t1l, am, al);
  Add12(resh, &t2l, ah, t1h);
  Add12(resm, resl, t2l, t1l);
}

inline void Mul23(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl, gpga_double ah, gpga_double al,
                  gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  gpga_double t9 = gpga_double_zero(0u);
  gpga_double t10 = gpga_double_zero(0u);
  Mul12(resh, &t1, ah, bh);
  Mul12(&t2, &t3, ah, bl);
  Mul12(&t4, &t5, al, bh);
  t6 = gpga_double_mul(al, bl);
  Add22Cond(&t7, &t8, t2, t3, t4, t5);
  Add12(&t9, &t10, t1, t6);
  Add22Cond(resm, resl, t7, t8, t9, t10);
}

inline void Mul233(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double ah, gpga_double al,
                   gpga_double bh, gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  gpga_double t9 = gpga_double_zero(0u);
  gpga_double t10 = gpga_double_zero(0u);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  gpga_double t13 = gpga_double_zero(0u);
  gpga_double t14 = gpga_double_zero(0u);
  gpga_double t15 = gpga_double_zero(0u);
  gpga_double t16 = gpga_double_zero(0u);
  gpga_double t17 = gpga_double_zero(0u);
  gpga_double t18 = gpga_double_zero(0u);
  Mul12(resh, &t1, ah, bh);
  Mul12(&t2, &t3, ah, bm);
  Mul12(&t4, &t5, ah, bl);
  Mul12(&t6, &t7, al, bh);
  Mul12(&t8, &t9, al, bm);
  t10 = gpga_double_mul(al, bl);
  Add22Cond(&t11, &t12, t2, t3, t4, t5);
  Add22Cond(&t13, &t14, t6, t7, t8, t9);
  Add22Cond(&t15, &t16, t11, t12, t13, t14);
  Add12Cond(&t17, &t18, t1, t10);
  Add22Cond(resm, resl, t17, t18, t15, t16);
}

inline void Add33(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl, gpga_double ah, gpga_double am,
                  gpga_double al, gpga_double bh, gpga_double bm,
                  gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  Add12(resh, &t1, ah, bh);
  Add12Cond(&t2, &t3, am, bm);
  t6 = gpga_double_add(al, bl);
  Add12Cond(&t7, &t4, t1, t2);
  t5 = gpga_double_add(t3, t4);
  t8 = gpga_double_add(t5, t6);
  Add12Cond(resm, resl, t7, t8);
}

inline void Add33Cond(thread gpga_double* resh, thread gpga_double* resm,
                      thread gpga_double* resl, gpga_double ah,
                      gpga_double am, gpga_double al, gpga_double bh,
                      gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  Add12Cond(resh, &t1, ah, bh);
  Add12Cond(&t2, &t3, am, bm);
  t6 = gpga_double_add(al, bl);
  Add12Cond(&t7, &t4, t1, t2);
  t5 = gpga_double_add(t3, t4);
  t8 = gpga_double_add(t5, t6);
  Add12Cond(resm, resl, t7, t8);
}

inline void Add233(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double ah, gpga_double al,
                   gpga_double bh, gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  Add12(resh, &t1, ah, bh);
  Add12Cond(&t2, &t3, al, bm);
  Add12Cond(&t4, &t5, t1, t2);
  t6 = gpga_double_add(t3, bl);
  t7 = gpga_double_add(t6, t5);
  Add12Cond(resm, resl, t4, t7);
}

inline void Add233Cond(thread gpga_double* resh, thread gpga_double* resm,
                       thread gpga_double* resl, gpga_double ah, gpga_double al,
                       gpga_double bh, gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  Add12Cond(resh, &t1, ah, bh);
  Add12Cond(&t2, &t3, al, bm);
  Add12Cond(&t4, &t5, t1, t2);
  gpga_double t6 = gpga_double_add(t3, bl);
  gpga_double t7 = gpga_double_add(t6, t5);
  Add12Cond(resm, resl, t4, t7);
}

inline void Add123(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double a, gpga_double bh,
                   gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  Add12(resh, &t1, a, bh);
  Add12(resm, resl, t1, bl);
}

inline void Add133(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double a, gpga_double bh,
                   gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  Add12(resh, &t1, a, bh);
  Add12Cond(&t2, &t3, t1, bm);
  gpga_double t4 = gpga_double_add(t3, bl);
  Add12Cond(resm, resl, t2, t4);
}

inline void Add133Cond(thread gpga_double* resh, thread gpga_double* resm,
                       thread gpga_double* resl, gpga_double a, gpga_double bh,
                       gpga_double bm, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  Add12Cond(resh, &t1, a, bh);
  Add12Cond(&t2, &t3, t1, bm);
  gpga_double t4 = gpga_double_add(t3, bl);
  Add12Cond(resm, resl, t2, t4);
}

inline void Add213(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double ah, gpga_double al,
                   gpga_double b) {
  gpga_double t1 = gpga_double_zero(0u);
  Add12(resh, &t1, ah, b);
  Add12Cond(resm, resl, al, b);
}

inline void Add23(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl, gpga_double ah, gpga_double al,
                  gpga_double bh, gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  Add12(resh, &t1, ah, bh);
  Add12Cond(&t2, &t3, al, bl);
  Add12Cond(&t4, &t5, t1, t2);
  t6 = gpga_double_add(t3, t5);
  Add12Cond(resm, resl, t4, t6);
}

inline void Mul33(thread gpga_double* resh, thread gpga_double* resm,
                  thread gpga_double* resl, gpga_double ah, gpga_double am,
                  gpga_double al, gpga_double bh, gpga_double bm,
                  gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  gpga_double t9 = gpga_double_zero(0u);
  gpga_double t10 = gpga_double_zero(0u);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  gpga_double t13 = gpga_double_zero(0u);
  gpga_double t14 = gpga_double_zero(0u);
  gpga_double t15 = gpga_double_zero(0u);
  gpga_double t16 = gpga_double_zero(0u);
  gpga_double t17 = gpga_double_zero(0u);
  gpga_double t18 = gpga_double_zero(0u);
  gpga_double t19 = gpga_double_zero(0u);
  gpga_double t20 = gpga_double_zero(0u);
  gpga_double t21 = gpga_double_zero(0u);
  gpga_double t22 = gpga_double_zero(0u);
  Mul12(resh, &t1, ah, bh);
  Mul12(&t2, &t3, ah, bm);
  Mul12(&t4, &t5, am, bh);
  Mul12(&t6, &t7, am, bm);
  t8 = gpga_double_mul(ah, bl);
  t9 = gpga_double_mul(al, bh);
  t10 = gpga_double_mul(am, bl);
  t11 = gpga_double_mul(al, bm);
  t12 = gpga_double_add(t8, t9);
  t13 = gpga_double_add(t10, t11);
  Add12Cond(&t14, &t15, t1, t6);
  t16 = gpga_double_add(t7, t15);
  t17 = gpga_double_add(t12, t13);
  t18 = gpga_double_add(t16, t17);
  Add12Cond(&t19, &t20, t14, t18);
  Add22Cond(&t21, &t22, t2, t3, t4, t5);
  Add22Cond(resm, resl, t21, t22, t19, t20);
}

inline void Mul133(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double a, gpga_double bh,
                   gpga_double bm, gpga_double bl) {
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t7 = gpga_double_zero(0u);
  gpga_double t8 = gpga_double_zero(0u);
  gpga_double t9 = gpga_double_zero(0u);
  gpga_double t10 = gpga_double_zero(0u);
  Mul12(resh, &t2, a, bh);
  Mul12(&t3, &t4, a, bm);
  t5 = gpga_double_mul(a, bl);
  Add12Cond(&t9, &t7, t2, t3);
  t8 = gpga_double_add(t4, t5);
  t10 = gpga_double_add(t7, t8);
  Add12Cond(resm, resl, t9, t10);
}

inline void Mul123(thread gpga_double* resh, thread gpga_double* resm,
                   thread gpga_double* resl, gpga_double a, gpga_double bh,
                   gpga_double bl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  Mul12(resh, &t1, a, bh);
  Mul12(&t2, &t3, a, bl);
  Add12Cond(&t5, &t4, t1, t2);
  t6 = gpga_double_add(t3, t4);
  Add12Cond(resm, resl, t5, t6);
}

inline void Recpr33(thread gpga_double* resh, thread gpga_double* resm,
                    thread gpga_double* resl, gpga_double dh, gpga_double dm,
                    gpga_double dl) {
  gpga_double rec_r1 = gpga_double_div(gpga_double_from_u32(1u), dh);
  gpga_double rec_t1 = gpga_double_zero(0u);
  gpga_double rec_t2 = gpga_double_zero(0u);
  Mul12(&rec_t1, &rec_t2, rec_r1, dh);
  gpga_double rec_t3 = gpga_double_sub(rec_t1, gpga_double_from_u32(1u));
  gpga_double rec_t4 = gpga_double_zero(0u);
  gpga_double rec_t5 = gpga_double_zero(0u);
  Add12Cond(&rec_t4, &rec_t5, rec_t3, rec_t2);
  gpga_double rec_t6 = gpga_double_zero(0u);
  gpga_double rec_t7 = gpga_double_zero(0u);
  Mul12(&rec_t6, &rec_t7, rec_r1, dm);
  gpga_double rec_t8 = gpga_double_zero(0u);
  gpga_double rec_t9 = gpga_double_zero(0u);
  Add12(&rec_t8, &rec_t9, gpga_double_from_s32(-1), rec_t6);
  gpga_double rec_t10 = gpga_double_add(rec_t9, rec_t7);
  gpga_double rec_t11 = gpga_double_zero(0u);
  gpga_double rec_t12 = gpga_double_zero(0u);
  Add12(&rec_t11, &rec_t12, rec_t8, rec_t10);
  rec_r1 = gpga_double_neg(rec_r1);
  gpga_double rec_t13 = gpga_double_zero(0u);
  gpga_double rec_t14 = gpga_double_zero(0u);
  Add22Cond(&rec_t13, &rec_t14, rec_t4, rec_t5, rec_t11, rec_t12);
  gpga_double rec_r2h = gpga_double_zero(0u);
  gpga_double rec_r2l = gpga_double_zero(0u);
  Mul122(&rec_r2h, &rec_r2l, rec_r1, rec_t13, rec_t14);
  gpga_double rec_t15 = gpga_double_zero(0u);
  gpga_double rec_t16 = gpga_double_zero(0u);
  gpga_double rec_t17 = gpga_double_zero(0u);
  Mul233(&rec_t15, &rec_t16, &rec_t17, rec_r2h, rec_r2l, dh, dm, dl);
  gpga_double rec_t18 = gpga_double_zero(0u);
  gpga_double rec_t19 = gpga_double_zero(0u);
  gpga_double rec_t20 = gpga_double_zero(0u);
  Renormalize3(&rec_t18, &rec_t19, &rec_t20, rec_t15, rec_t16, rec_t17);
  rec_t18 = gpga_double_from_s32(-1);
  gpga_double rec_t21 = gpga_double_zero(0u);
  gpga_double rec_t22 = gpga_double_zero(0u);
  gpga_double rec_t23 = gpga_double_zero(0u);
  Mul233(&rec_t21, &rec_t22, &rec_t23, rec_r2h, rec_r2l, rec_t18, rec_t19,
         rec_t20);
  rec_t21 = gpga_double_neg(rec_t21);
  rec_t22 = gpga_double_neg(rec_t22);
  rec_t23 = gpga_double_neg(rec_t23);
  Renormalize3(resh, resm, resl, rec_t21, rec_t22, rec_t23);
}

inline void gpga_sqrt12_64_unfiltered(thread gpga_double* resh,
                                      thread gpga_double* resl,
                                      gpga_double x) {
  gpga_double xdb = x;
  int E = (int)(gpga_u64_hi(xdb) >> 20) - 1023;
  uint hi = gpga_u64_hi(xdb);
  hi = (hi & 0x000fffffu) | 0x3ff00000u;
  gpga_double m = gpga_u64_from_words(hi, gpga_u64_lo(xdb));
  gpga_double half_val = gpga_double_const_inv2();
  if (E & 1) {
    E += 1;
    m = gpga_double_mul(m, half_val);
  }
  uint sqrt_hi = (uint)(((E / 2) + 1023) << 20);
  gpga_double scale = gpga_u64_from_words(sqrt_hi, 0u);

  gpga_double r0 = gpga_double_add(
      SQRTPOLYC0,
      gpga_double_mul(
          m, gpga_double_add(
                 SQRTPOLYC1,
                 gpga_double_mul(
                     m, gpga_double_add(
                            SQRTPOLYC2,
                            gpga_double_mul(
                                m, gpga_double_add(SQRTPOLYC3,
                                                   gpga_double_mul(m,
                                                                   SQRTPOLYC4))))))));
  gpga_double three = gpga_double_from_u32(3u);
  gpga_double r1 =
      gpga_double_mul(half_val, gpga_double_mul(
                                r0, gpga_double_sub(
                                        three,
                                        gpga_double_mul(m,
                                                        gpga_double_mul(r0, r0)))));
  gpga_double r2 =
      gpga_double_mul(half_val, gpga_double_mul(
                                r1, gpga_double_sub(
                                        three,
                                        gpga_double_mul(m,
                                                        gpga_double_mul(r1, r1)))));

  gpga_double r2Sqh = gpga_double_zero(0u);
  gpga_double r2Sql = gpga_double_zero(0u);
  Mul12(&r2Sqh, &r2Sql, r2, r2);
  gpga_double r2PHr2h = gpga_double_zero(0u);
  gpga_double r2PHr2l = gpga_double_zero(0u);
  Add12(&r2PHr2h, &r2PHr2l, r2, gpga_double_mul(half_val, r2));
  gpga_double mMr2h = gpga_double_zero(0u);
  gpga_double mMr2l = gpga_double_zero(0u);
  Mul12(&mMr2h, &mMr2l, m, r2);
  gpga_double mMr2Ch = gpga_double_zero(0u);
  gpga_double mMr2Cl = gpga_double_zero(0u);
  Mul22(&mMr2Ch, &mMr2Cl, mMr2h, mMr2l, r2Sqh, r2Sql);
  gpga_double MHmMr2Ch = gpga_double_neg(gpga_double_mul(half_val, mMr2Ch));
  gpga_double MHmMr2Cl = gpga_double_neg(gpga_double_mul(half_val, mMr2Cl));
  gpga_double r3h = gpga_double_zero(0u);
  gpga_double r3l = gpga_double_zero(0u);
  Add22(&r3h, &r3l, r2PHr2h, r2PHr2l, MHmMr2Ch, MHmMr2Cl);

  gpga_double srtmh = gpga_double_zero(0u);
  gpga_double srtml = gpga_double_zero(0u);
  Mul22(&srtmh, &srtml, m, gpga_double_zero(0u), r3h, r3l);

  *resh = gpga_double_mul(scale, srtmh);
  *resl = gpga_double_mul(scale, srtml);
}

inline void gpga_sqrt13(thread gpga_double* resh, thread gpga_double* resm,
                        thread gpga_double* resl, gpga_double x) {
  if (gpga_double_is_zero(x)) {
    *resh = x;
    *resm = gpga_double_zero(0u);
    *resl = gpga_double_zero(0u);
    return;
  }

  int E = 0;
  gpga_double xdb = x;
  uint hi = gpga_u64_hi(xdb);
  if (hi < 0x00100000u) {
    E = -52;
    xdb = gpga_double_mul(xdb, SQRTTWO52);
    hi = gpga_u64_hi(xdb);
  }
  E += (int)(hi >> 20) - 1023;
  hi = (hi & 0x000fffffu) | 0x3ff00000u;
  gpga_double m = gpga_u64_from_words(hi, gpga_u64_lo(xdb));

  gpga_double half_val = gpga_double_const_inv2();
  if (E & 1) {
    E += 1;
    m = gpga_double_mul(m, half_val);
  }

  uint sqrt_hi = (uint)(((E / 2) + 1023) << 20);
  gpga_double scale = gpga_u64_from_words(sqrt_hi, 0u);

  gpga_double r0 = gpga_double_add(
      SQRTPOLYC0,
      gpga_double_mul(
          m, gpga_double_add(
                 SQRTPOLYC1,
                 gpga_double_mul(
                     m, gpga_double_add(
                            SQRTPOLYC2,
                            gpga_double_mul(
                                m, gpga_double_add(SQRTPOLYC3,
                                                   gpga_double_mul(m,
                                                                   SQRTPOLYC4))))))));
  gpga_double three = gpga_double_from_u32(3u);
  gpga_double r1 =
      gpga_double_mul(half_val, gpga_double_mul(
                                r0, gpga_double_sub(
                                        three,
                                        gpga_double_mul(m,
                                                        gpga_double_mul(r0, r0)))));
  gpga_double r2 =
      gpga_double_mul(half_val, gpga_double_mul(
                                r1, gpga_double_sub(
                                        three,
                                        gpga_double_mul(m,
                                                        gpga_double_mul(r1, r1)))));

  gpga_double r2Sqh = gpga_double_zero(0u);
  gpga_double r2Sql = gpga_double_zero(0u);
  Mul12(&r2Sqh, &r2Sql, r2, r2);
  gpga_double r2PHr2h = gpga_double_zero(0u);
  gpga_double r2PHr2l = gpga_double_zero(0u);
  Add12(&r2PHr2h, &r2PHr2l, r2, gpga_double_mul(half_val, r2));
  gpga_double mMr2h = gpga_double_zero(0u);
  gpga_double mMr2l = gpga_double_zero(0u);
  Mul12(&mMr2h, &mMr2l, m, r2);
  gpga_double mMr2Ch = gpga_double_zero(0u);
  gpga_double mMr2Cl = gpga_double_zero(0u);
  Mul22(&mMr2Ch, &mMr2Cl, mMr2h, mMr2l, r2Sqh, r2Sql);

  gpga_double MHmMr2Ch = gpga_double_neg(gpga_double_mul(half_val, mMr2Ch));
  gpga_double MHmMr2Cl = gpga_double_neg(gpga_double_mul(half_val, mMr2Cl));
  gpga_double r3h = gpga_double_zero(0u);
  gpga_double r3l = gpga_double_zero(0u);
  Add22(&r3h, &r3l, r2PHr2h, r2PHr2l, MHmMr2Ch, MHmMr2Cl);

  gpga_double r3Sqh = gpga_double_zero(0u);
  gpga_double r3Sql = gpga_double_zero(0u);
  Mul22(&r3Sqh, &r3Sql, r3h, r3l, r3h, r3l);
  gpga_double mMr3Sqh = gpga_double_zero(0u);
  gpga_double mMr3Sql = gpga_double_zero(0u);
  Mul22(&mMr3Sqh, &mMr3Sql, m, gpga_double_zero(0u), r3Sqh, r3Sql);

  gpga_double r4h = gpga_double_zero(0u);
  gpga_double r4l = gpga_double_zero(0u);
  gpga_double neg_half_mMr3Sql =
      gpga_double_neg(gpga_double_mul(half_val, mMr3Sql));
  Mul22(&r4h, &r4l, r3h, r3l, gpga_double_from_u32(1u), neg_half_mMr3Sql);

  gpga_double r4Sqh = gpga_double_zero(0u);
  gpga_double r4Sqm = gpga_double_zero(0u);
  gpga_double r4Sql = gpga_double_zero(0u);
  Mul23(&r4Sqh, &r4Sqm, &r4Sql, r4h, r4l, r4h, r4l);
  gpga_double mMr4Sqhover = gpga_double_zero(0u);
  gpga_double mMr4Sqmover = gpga_double_zero(0u);
  gpga_double mMr4Sqlover = gpga_double_zero(0u);
  Mul133(&mMr4Sqhover, &mMr4Sqmover, &mMr4Sqlover, m, r4Sqh, r4Sqm,
         r4Sql);
  gpga_double mMr4Sqh = gpga_double_zero(0u);
  gpga_double mMr4Sqm = gpga_double_zero(0u);
  gpga_double mMr4Sql = gpga_double_zero(0u);
  Renormalize3(&mMr4Sqh, &mMr4Sqm, &mMr4Sql, mMr4Sqhover, mMr4Sqmover,
               mMr4Sqlover);

  gpga_double HmMr4Sqm = gpga_double_neg(gpga_double_mul(half_val, mMr4Sqm));
  gpga_double HmMr4Sql = gpga_double_neg(gpga_double_mul(half_val, mMr4Sql));

  gpga_double r5h = gpga_double_zero(0u);
  gpga_double r5m = gpga_double_zero(0u);
  gpga_double r5l = gpga_double_zero(0u);
  Mul233(&r5h, &r5m, &r5l, r4h, r4l, gpga_double_from_u32(1u), HmMr4Sqm,
         HmMr4Sql);

  gpga_double srtmhover = gpga_double_zero(0u);
  gpga_double srtmmover = gpga_double_zero(0u);
  gpga_double srtmlover = gpga_double_zero(0u);
  Mul133(&srtmhover, &srtmmover, &srtmlover, m, r5h, r5m, r5l);

  gpga_double srtmh = gpga_double_zero(0u);
  gpga_double srtmm = gpga_double_zero(0u);
  gpga_double srtml = gpga_double_zero(0u);
  Renormalize3(&srtmh, &srtmm, &srtml, srtmhover, srtmmover, srtmlover);

  *resh = gpga_double_mul(scale, srtmh);
  *resm = gpga_double_mul(scale, srtmm);
  *resl = gpga_double_mul(scale, srtml);
}

inline gpga_double gpga_double_raw_inc(gpga_double x) {
  return x + 1ul;
}

inline gpga_double gpga_double_raw_dec(gpga_double x) {
  return x - 1ul;
}

inline gpga_double ReturnRoundToNearest3(gpga_double xh, gpga_double xm,
                                         gpga_double xl) {
  gpga_double t1 = gpga_double_raw_dec(xh);
  gpga_double t4 = gpga_double_raw_inc(xh);
  gpga_double t2 = gpga_double_sub(xh, t1);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double t3 = gpga_double_mul(t2, gpga_double_neg(half_val));
  gpga_double t5 = gpga_double_sub(t4, xh);
  gpga_double t6 = gpga_double_mul(t5, half_val);
  if (!gpga_double_eq(xm, t3) && !gpga_double_eq(xm, t6)) {
    return gpga_double_add(xh, xm);
  }
  gpga_double zero = gpga_double_zero(0u);
  if (gpga_double_gt(gpga_double_mul(xm, xl), zero)) {
    if (gpga_double_gt(gpga_double_mul(xh, xl), zero)) {
      return t4;
    }
    return t1;
  }
  return xh;
}

inline gpga_double ReturnRoundToNearest3Other(gpga_double xh, gpga_double xm,
                                              gpga_double xl) {
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  Add12(&t3, &t4, xm, xl);
  if (!gpga_double_is_zero(t4)) {
    if ((t3 & 1ul) == 0ul) {
      bool t4_pos = gpga_double_gt(t4, gpga_double_zero(0u));
      bool sign = gpga_double_sign(t3) != 0u;
      if (t4_pos != sign) {
        t3 = gpga_double_raw_inc(t3);
      } else {
        t3 = gpga_double_raw_dec(t3);
      }
    }
  }
  return gpga_double_add(xh, t3);
}

inline gpga_double ReturnRoundUpwards3(gpga_double xh, gpga_double xm,
                                       gpga_double xl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, xh, xm);
  gpga_double t3 = gpga_double_add(t2, xl);
  if (gpga_double_gt(t3, gpga_double_zero(0u))) {
    if (gpga_double_gt(t1, gpga_double_zero(0u))) {
      return gpga_double_raw_inc(t1);
    }
    return gpga_double_raw_dec(t1);
  }
  return t1;
}

inline gpga_double ReturnRoundDownwards3(gpga_double xh, gpga_double xm,
                                         gpga_double xl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, xh, xm);
  gpga_double t3 = gpga_double_add(t2, xl);
  if (gpga_double_lt(t3, gpga_double_zero(0u))) {
    if (gpga_double_gt(t1, gpga_double_zero(0u))) {
      return gpga_double_raw_dec(t1);
    }
    return gpga_double_raw_inc(t1);
  }
  return t1;
}

inline gpga_double ReturnRoundTowardsZero3(gpga_double xh, gpga_double xm,
                                           gpga_double xl) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, xh, xm);
  gpga_double t3 = gpga_double_add(t2, xl);
  if (gpga_double_gt(t1, gpga_double_zero(0u))) {
    if (gpga_double_lt(t3, gpga_double_zero(0u))) {
      return gpga_double_raw_dec(t1);
    }
    return t1;
  }
  if (gpga_double_gt(t3, gpga_double_zero(0u))) {
    return gpga_double_raw_dec(t1);
  }
  return t1;
}

inline gpga_double ReturnRoundUpwards3Unfiltered(gpga_double xh, gpga_double xm,
                                                 gpga_double xl,
                                                 gpga_double wca) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, xh, xm);
  gpga_double t3 = gpga_double_add(t2, xl);
  if (gpga_double_gt(t3, gpga_double_zero(0u))) {
    gpga_double scaled = gpga_double_mul(wca, t3);
    if (gpga_double_exp(gpga_double_abs(scaled)) <
        gpga_double_exp(gpga_double_abs(t1))) {
      return t1;
    }
    if (gpga_double_gt(t1, gpga_double_zero(0u))) {
      return gpga_double_raw_inc(t1);
    }
    return gpga_double_raw_dec(t1);
  }
  return t1;
}

inline gpga_double ReturnRoundDownwards3Unfiltered(
    gpga_double xh, gpga_double xm, gpga_double xl, gpga_double wca) {
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12(&t1, &t2, xh, xm);
  gpga_double t3 = gpga_double_add(t2, xl);
  if (gpga_double_lt(t3, gpga_double_zero(0u))) {
    gpga_double scaled = gpga_double_mul(wca, t3);
    if (gpga_double_exp(gpga_double_abs(scaled)) <
        gpga_double_exp(gpga_double_abs(t1))) {
      return t1;
    }
    if (gpga_double_gt(t1, gpga_double_zero(0u))) {
      return gpga_double_raw_dec(t1);
    }
    return gpga_double_raw_inc(t1);
  }
  return t1;
}

inline gpga_double ReturnRoundTowardsZero3Unfiltered(
    gpga_double xh, gpga_double xm, gpga_double xl, gpga_double wca) {
  if (gpga_double_gt(xh, gpga_double_zero(0u))) {
    return ReturnRoundDownwards3Unfiltered(xh, xm, xl, wca);
  }
  return ReturnRoundUpwards3Unfiltered(xh, xm, xl, wca);
}

inline void RoundToNearest3(thread gpga_double* res, gpga_double xh,
                            gpga_double xm, gpga_double xl) {
  *res = ReturnRoundToNearest3(xh, xm, xl);
}

inline void RoundUpwards3(thread gpga_double* res, gpga_double xh,
                          gpga_double xm, gpga_double xl) {
  *res = ReturnRoundUpwards3(xh, xm, xl);
}

inline void RoundDownwards3(thread gpga_double* res, gpga_double xh,
                            gpga_double xm, gpga_double xl) {
  *res = ReturnRoundDownwards3(xh, xm, xl);
}

inline void RoundTowardsZero3(thread gpga_double* res, gpga_double xh,
                              gpga_double xm, gpga_double xl) {
  *res = ReturnRoundTowardsZero3(xh, xm, xl);
}

inline bool gpga_test_and_return_ru(gpga_double yh, gpga_double yl,
                                    gpga_double eps,
                                    thread gpga_double* out) {
  ulong abs_yh_bits = yh & 0x7fffffffffffffffULL;
  gpga_double abs_yl = gpga_double_abs(yl);
  ulong u53_bits =
      (abs_yh_bits & 0x7ff0000000000000ULL) + 0x0010000000000000ULL;
  gpga_double u53 = gpga_double_from_u64(u53_bits);
  if (gpga_double_gt(abs_yl, gpga_double_mul(eps, u53))) {
    uint yh_neg = gpga_double_sign(yh);
    uint yl_neg = gpga_double_sign(yl);
    if (yl_neg == 0u) {
      ulong bits = yh;
      bits = yh_neg ? (bits - 1ull) : (bits + 1ull);
      *out = bits;
      return true;
    }
    *out = yh;
    return true;
  }
  return false;
}

inline bool gpga_test_and_return_rd(gpga_double yh, gpga_double yl,
                                    gpga_double eps,
                                    thread gpga_double* out) {
  ulong abs_yh_bits = yh & 0x7fffffffffffffffULL;
  gpga_double abs_yl = gpga_double_abs(yl);
  ulong u53_bits =
      (abs_yh_bits & 0x7ff0000000000000ULL) + 0x0010000000000000ULL;
  gpga_double u53 = gpga_double_from_u64(u53_bits);
  if (gpga_double_gt(abs_yl, gpga_double_mul(eps, u53))) {
    uint yh_neg = gpga_double_sign(yh);
    uint yl_neg = gpga_double_sign(yl);
    if (yl_neg != 0u) {
      ulong bits = yh;
      bits = yh_neg ? (bits + 1ull) : (bits - 1ull);
      *out = bits;
      return true;
    }
    *out = yh;
    return true;
  }
  return false;
}

inline bool gpga_test_and_return_rz(gpga_double yh, gpga_double yl,
                                    gpga_double eps,
                                    thread gpga_double* out) {
  ulong abs_yh_bits = yh & 0x7fffffffffffffffULL;
  gpga_double abs_yl = gpga_double_abs(yl);
  ulong u53_bits =
      (abs_yh_bits & 0x7ff0000000000000ULL) + 0x0010000000000000ULL;
  gpga_double u53 = gpga_double_from_u64(u53_bits);
  if (gpga_double_gt(abs_yl, gpga_double_mul(eps, u53))) {
    uint yh_neg = gpga_double_sign(yh);
    uint yl_neg = gpga_double_sign(yl);
    if (yl_neg != yh_neg) {
      *out = yh - 1ull;
      return true;
    }
    *out = yh;
    return true;
  }
  return false;
}



// ---------------------------------------------------------------------------
// SCS multi-precision (base 2^30) support for CRlibm.
#define SCS_NB_WORDS 8
#define SCS_NB_BITS 30
#define SCS_RADIX ((uint)(1u << SCS_NB_BITS))
#define SCS_MASK_RADIX (SCS_RADIX - 1u)
#define SCS_MAX_RANGE 32

struct scs {
  uint h_word[SCS_NB_WORDS];
  gpga_double exception;
  int index;
  int sign;
};

typedef struct scs scs;
typedef thread scs* scs_ptr;
typedef constant scs* scs_const_ptr;
typedef struct scs scs_t[1];

#define R_HW result->h_word
#define R_SGN result->sign
#define R_IND result->index
#define R_EXP result->exception

#define X_HW x->h_word
#define X_SGN x->sign
#define X_IND x->index
#define X_EXP x->exception

#define Y_HW y->h_word
#define Y_SGN y->sign
#define Y_IND y->index
#define Y_EXP y->exception

#define Z_HW z->h_word
#define Z_SGN z->sign
#define Z_IND z->index
#define Z_EXP z->exception

#define SCS_CARRY_PROPAGATE(r1, r0, tmp) \
  {                                     \
    tmp = r1 >> SCS_NB_BITS;            \
    r0 += tmp;                          \
    r1 -= (tmp << SCS_NB_BITS);         \
  }

inline void scs_set_const(thread scs* result, scs_const_ptr src);
inline void scs_set(scs_ptr result, scs_ptr x);
inline void scs_set_si(scs_ptr result, int x);
inline void scs_set_d(scs_ptr result, gpga_double x);
inline void scs_get_d(thread gpga_double* result, scs_ptr x);
inline void scs_get_d_minf(thread gpga_double* result, scs_ptr x);
inline void scs_get_d_pinf(thread gpga_double* result, scs_ptr x);
inline void scs_get_d_zero(thread gpga_double* result, scs_ptr x);
inline void scs_add(scs_ptr result, scs_ptr x, scs_ptr y);
inline void scs_add(scs_ptr result, scs_const_ptr x, scs_ptr y);
inline void scs_add(scs_ptr result, scs_ptr x, scs_const_ptr y);
inline void scs_add(scs_ptr result, scs_const_ptr x, scs_const_ptr y);
inline void scs_sub(scs_ptr result, scs_ptr x, scs_ptr y);
inline void scs_sub(scs_ptr result, scs_const_ptr x, scs_ptr y);
inline void scs_sub(scs_ptr result, scs_ptr x, scs_const_ptr y);
inline void scs_sub(scs_ptr result, scs_const_ptr x, scs_const_ptr y);
inline void scs_mul(scs_ptr result, scs_ptr x, scs_ptr y);
inline void scs_mul(scs_ptr result, scs_const_ptr x, scs_ptr y);
inline void scs_mul(scs_ptr result, scs_ptr x, scs_const_ptr y);
inline void scs_mul(scs_ptr result, scs_const_ptr x, scs_const_ptr y);
inline void scs_square(scs_ptr result, scs_ptr x);
inline void scs_mul_ui(scs_ptr x, uint val_int);
inline void scs_div(scs_ptr result, scs_ptr x, scs_ptr y);
inline void scs_inv(scs_ptr result, scs_ptr y);

GPGA_CONST gpga_double gpga_db_one =
    gpga_bits_to_real(0x3ff0000000000000ul);

#define GPGA_DBL_FROM_WORDS(hi, lo) \
  (((ulong)(hi) << 32) | (ulong)(lo))

GPGA_CONST scs scs_zer = {{
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                           },
                           gpga_double_zero(0u),
                           0,
                           1};

GPGA_CONST scs scs_half = {{
                                0x20000000u,
                                0x00000000u,
                                0x00000000u,
                                0x00000000u,
                                0x00000000u,
                                0x00000000u,
                                0x00000000u,
                                0x00000000u,
                            },
                            gpga_db_one,
                            -1,
                            1};

GPGA_CONST scs scs_one = {{
                               0x00000001u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                           },
                           gpga_db_one,
                           0,
                           1};

GPGA_CONST scs scs_two = {{
                               0x00000002u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                               0x00000000u,
                           },
                           gpga_db_one,
                           0,
                           1};

GPGA_CONST scs scs_sixinv = {{
                                  0x0aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                                  0x2aaaaaaau,
                              },
                              gpga_db_one,
                              -1,
                              1};

#define SCS_ZERO ((scs_const_ptr)&scs_zer)
#define SCS_HALF ((scs_const_ptr)&scs_half)
#define SCS_ONE ((scs_const_ptr)&scs_one)
#define SCS_TWO ((scs_const_ptr)&scs_two)
#define SCS_SIXINV ((scs_const_ptr)&scs_sixinv)

inline void scs_set_const(thread scs* result, scs_const_ptr src) {
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    result->h_word[i] = src->h_word[i];
  }
  result->exception = src->exception;
  result->index = src->index;
  result->sign = src->sign;
}

GPGA_CONST uint scs_two_over_pi[] = {
    0x28be60dbu, 0x24e44152u, 0x27f09d5fu, 0x11f534ddu, 0x3036d8a5u,
    0x1993c439u, 0x0107f945u, 0x23abdebbu, 0x31586dc9u, 0x06e3a424u,
    0x374b8019u, 0x092eea09u, 0x3464873fu, 0x21deb1cbu, 0x04a69cfbu,
    0x288235f5u, 0x0baed121u, 0x0e99c702u, 0x1ad17df9u, 0x013991d6u,
    0x0e60d4ceu, 0x1f49c845u, 0x3e2ef7e4u, 0x283b1ff8u, 0x25fff781u,
    0x1980fef2u, 0x3c462d68u, 0x0a6d1f6du, 0x0d9fb3c9u, 0x3cb09b74u,
    0x3d18fd9au, 0x1e5fea2du, 0x1d49eeb1u, 0x3ebe5f17u, 0x2cf41ce7u,
    0x378a5292u, 0x3a9afed7u, 0x3b11f8d5u, 0x3421580cu, 0x3046fc7bu,
    0x1aeafc33u, 0x3bc209afu, 0x10d876a7u, 0x2391615eu, 0x3986c219u,
    0x199855f1u, 0x1281a102u, 0x0dffd880u,
};

GPGA_CONST scs scs_pio2 = {{
                                 0x00000001u,
                                 0x2487ed51u,
                                 0x042d1846u,
                                 0x26263314u,
                                 0x1701b839u,
                                 0x28948127u,
                                 0x01114cf9u,
                                 0x23a0105du,
                             },
                             gpga_db_one,
                             0,
                             1};

#define Pio2_ptr ((scs_const_ptr)&scs_pio2)

GPGA_CONST scs scs_pi = {{
                               0x00000003u,
                               0x090fdaa2u,
                               0x085a308du,
                               0x0c4c6628u,
                               0x2e037073u,
                               0x1129024eu,
                               0x022299f3u,
                               0x074020bbu,
                           },
                           gpga_db_one,
                           0,
                           1};

#define PiSCS_ptr ((scs_const_ptr)&scs_pi)

GPGA_CONST scs sc_ln2 = {{
                               0x2c5c85fdu,
                               0x3d1cf79au,
                               0x2f278eceu,
                               0x1803f2f6u,
                               0x2bd03cd0u,
                               0x3267298bu,
                               0x18b62834u,
                               0x175b8baau,
                           },
                           gpga_db_one,
                           -1,
                           1};

#define sc_ln2_ptr ((scs_const_ptr)&sc_ln2)

GPGA_CONST scs table_ti[13] = {
    {{0x17fafa3bu, 0x360546fbu, 0x1e6fdb53u, 0x0b1225e6u,
      0x15f38987u, 0x26664702u, 0x3cb1bf6du, 0x118a64f9u},
     gpga_db_one,
     -1,
     -1},
    {{0x12696211u, 0x0d36e49eu, 0x03beb767u, 0x1b02aa70u,
      0x2a30f490u, 0x3732bb37u, 0x2425c6dau, 0x1fc53d0eu},
     gpga_db_one,
     -1,
     -1},
    {{0x0d49f69eu, 0x115b3c6du, 0x395f53bdu, 0x0b901b99u,
      0x2e77188au, 0x3e3d1ab5u, 0x1147dedeu, 0x05483ae4u},
     gpga_db_one,
     -1,
     -1},
    {{0x088bc741u, 0x04fc8f7bu, 0x319c5a0fu, 0x38e5bd03u,
      0x31dda8feu, 0x30f08645u, 0x2fa1d5c5u, 0x02c6529du},
     gpga_db_one,
     -1,
     -1},
    {{0x0421662du, 0x19e3a068u, 0x228ff66fu, 0x3503372cu,
      0x04bf1b16u, 0x0ff1b85cu, 0x006c21b2u, 0x21a9efd6u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_double_zero(0u),
     0,
     1},
    {{0x03e14618u, 0x008b1533u, 0x02f992e2u, 0x37759978u,
      0x2634d1d3u, 0x13375edbu, 0x2e4634eau, 0x1dcf0aefu},
     gpga_db_one,
     -1,
     1},
    {{0x0789c1dbu, 0x22af2e5eu, 0x27aa1fffu, 0x21fe9e15u,
      0x176e53afu, 0x04015c6bu, 0x021a0541u, 0x006df1d7u},
     gpga_db_one,
     -1,
     1},
    {{0x0aff9838u, 0x14f27a79u, 0x039f1050u, 0x0e424775u,
      0x3f35571cu, 0x355ff008u, 0x1ca13efcu, 0x3c2c8490u},
     gpga_db_one,
     -1,
     1},
    {{0x0e47fbe3u, 0x33534435u, 0x212ec0f7u, 0x25ff7344u,
      0x2571d97au, 0x274129e2u, 0x12b111dbu, 0x2c051568u},
     gpga_db_one,
     -1,
     1},
    {{0x11675cabu, 0x2ae98380u, 0x39cc7d57u, 0x041b8b82u,
      0x0fc19f41u, 0x0a43c91du, 0x1523ef69u, 0x164b69f6u},
     gpga_db_one,
     -1,
     1},
    {{0x14618bc2u, 0x0717b09fu, 0x10b7b37bu, 0x0cf1cd10u,
      0x15dcb349u, 0x0c00c397u, 0x2c39cc9bu, 0x274c94a8u},
     gpga_db_one,
     -1,
     1},
    {{0x1739d7f6u, 0x2ef401a7u, 0x0e24c53fu, 0x2b4fbde5u,
      0x2ab77843u, 0x1cea5975u, 0x1eeef249u, 0x384d2344u},
     gpga_db_one,
     -1,
     1},
};

#define table_ti_ptr ((scs_const_ptr)&table_ti)

GPGA_CONST scs table_inv_wi[13] = {
    {{0x00000001u, 0x1d1745d1u, 0x1d1745d1u, 0x1d1745d1u,
      0x1d1745d1u, 0x1d1745d1u, 0x1d183e2au, 0x36835582u},
     gpga_db_one,
     0,
     1},
    {{0x00000001u, 0x15555555u, 0x15555555u, 0x15555555u,
      0x15555555u, 0x15555555u, 0x15549b7eu, 0x1a416c6bu},
     gpga_db_one,
     0,
     1},
    {{0x00000001u, 0x0ec4ec4eu, 0x313b13b1u, 0x0ec4ec4eu,
      0x313b13b1u, 0x0ec4ec4eu, 0x313a6825u, 0x3ab28b77u},
     gpga_db_one,
     0,
     1},
    {{0x00000001u, 0x09249249u, 0x09249249u, 0x09249249u,
      0x09249249u, 0x09249249u, 0x09238b74u, 0x26f620a6u},
     gpga_db_one,
     0,
     1},
    {{0x00000001u, 0x04444444u, 0x11111111u, 0x04444444u,
      0x11111111u, 0x04444444u, 0x1111d60eu, 0x1f0c9d58u},
     gpga_db_one,
     0,
     1},
    {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_db_one,
     0,
     1},
    {{0x3c3c3c3cu, 0x0f0f0f0fu, 0x03c3c3c3u, 0x30f0f0f0u,
      0x3c3c3c3cu, 0x0f0f923du, 0x16e0e0a4u, 0x3a84202fu},
     gpga_db_one,
     -1,
     1},
    {{0x38e38e38u, 0x38e38e38u, 0x38e38e38u, 0x38e38e38u,
      0x38e38e38u, 0x38e3946au, 0x2e0ee2c9u, 0x0d6e0fbdu},
     gpga_db_one,
     -1,
     1},
    {{0x35e50d79u, 0x10d79435u, 0x39435e50u, 0x35e50d79u,
      0x10d79435u, 0x3943324du, 0x0637ea85u, 0x131a67bau},
     gpga_db_one,
     -1,
     1},
    {{0x33333333u, 0x0cccccccu, 0x33333333u, 0x0cccccccu,
      0x33333333u, 0x0cccccccu, 0x33333333u, 0x0cccccccu},
     gpga_db_one,
     -1,
     1},
    {{0x30c30c30u, 0x30c30c30u, 0x30c30c30u, 0x30c30c30u,
      0x30c30c30u, 0x30c2f1a4u, 0x160958a1u, 0x2b03bc88u},
     gpga_db_one,
     -1,
     1},
    {{0x2e8ba2e8u, 0x2e8ba2e8u, 0x2e8ba2e8u, 0x2e8ba2e8u,
      0x2e8ba2e8u, 0x2e8bcb74u, 0x2d78b525u, 0x00a1db67u},
     gpga_db_one,
     -1,
     1},
    {{0x2c8590b2u, 0x0590b216u, 0x10b21642u, 0x321642c8u,
      0x1642c859u, 0x02c8590bu, 0x08590b21u, 0x190b2164u},
     gpga_db_one,
     -1,
     1},
};

#define table_inv_wi_ptr ((scs_const_ptr)&table_inv_wi)

GPGA_CONST scs constant_poly[20] = {
    {{0x0337074bu, 0x275aac5cu, 0x2cf4a893u, 0x38013cc3u,
      0x149a3416u, 0x0e067307u, 0x12745608u, 0x1658e0d5u},
     gpga_db_one,
     -1,
     -1},
    {{0x03622298u, 0x252ff65cu, 0x03001550u, 0x2f457908u,
      0x32f78eccu, 0x17442a4eu, 0x1d806366u, 0x2c50350eu},
     gpga_db_one,
     -1,
     1},
    {{0x038e36bbu, 0x30665a9cu, 0x119434c7u, 0x3fdec8cbu,
      0x37dd3adbu, 0x2663cd45u, 0x230e43e9u, 0x32b9663cu},
     gpga_db_one,
     -1,
     -1},
    {{0x03c3c1bbu, 0x38c473aeu, 0x192b9c18u, 0x242b7c4eu,
      0x3da8edc8u, 0x04454ffeu, 0x2cf133c6u, 0x0c926fd0u},
     gpga_db_one,
     -1,
     1},
    {{0x04000000u, 0x2b72bb0au, 0x038f5efcu, 0x34665092u,
      0x2461b6c9u, 0x172f7050u, 0x1218b5c1u, 0x104862d7u},
     gpga_db_one,
     -1,
     -1},
    {{0x04444444u, 0x374f3324u, 0x1531bcf1u, 0x1d7d23fcu,
      0x26ff9670u, 0x38fc33aeu, 0x15bf1cfbu, 0x2c9f1c2du},
     gpga_db_one,
     -1,
     1},
    {{0x04924924u, 0x2489e5b6u, 0x288b19c5u, 0x2893519bu,
      0x2c3f35c0u, 0x0b8bfdceu, 0x3541ab49u, 0x1de415bcu},
     gpga_db_one,
     -1,
     -1},
    {{0x04ec4ec4u, 0x3b0ce4bdu, 0x14d14046u, 0x0243ade9u,
      0x083cc34fu, 0x393e6a5au, 0x2c1855f2u, 0x259d599fu},
     gpga_db_one,
     -1,
     1},
    {{0x05555555u, 0x1555565bu, 0x064b42afu, 0x13bc7961u,
      0x1396754bu, 0x33d85415u, 0x2ba548d4u, 0x039c4ff6u},
     gpga_db_one,
     -1,
     -1},
    {{0x05d1745du, 0x05d1751cu, 0x24facd05u, 0x07540f86u,
      0x014f2ec1u, 0x3bb3fa8bu, 0x02e1da4cu, 0x3304817cu},
     gpga_db_one,
     -1,
     1},
    {{0x06666666u, 0x19999999u, 0x21667ee1u, 0x0f5f75eau,
      0x353af37fu, 0x2578daa1u, 0x07c76f47u, 0x16541534u},
     gpga_db_one,
     -1,
     -1},
    {{0x071c71c7u, 0x071c71c7u, 0x03e7af88u, 0x2fca5d74u,
      0x0bb43f38u, 0x050edb70u, 0x3631b696u, 0x1fc3e0d3u},
     gpga_db_one,
     -1,
     1},
    {{0x08000000u, 0x00000000u, 0x00003ac6u, 0x36c11384u,
      0x2d596ab4u, 0x09257878u, 0x0597dc26u, 0x2d60813au},
     gpga_db_one,
     -1,
     -1},
    {{0x09249249u, 0x09249249u, 0x0924b1dbu, 0x0d002ac1u,
      0x0eafd708u, 0x2b4df21du, 0x0458da93u, 0x2d11460cu},
     gpga_db_one,
     -1,
     1},
    {{0x0aaaaaaau, 0x2aaaaaaau, 0x2aaaaaa9u, 0x0bb6630eu,
      0x2e44a5cfu, 0x39f32e04u, 0x105732b9u, 0x01a76208u},
     gpga_db_one,
     -1,
     -1},
    {{0x0cccccccu, 0x33333333u, 0x0cccccccu, 0x0bbbe6e8u,
      0x253269eau, 0x0ec2a630u, 0x10defc5cu, 0x238aef3bu},
     gpga_db_one,
     -1,
     1},
    {{0x10000000u, 0x00000000u, 0x00000000u, 0x0001195cu,
      0x3654cd5au, 0x16ca3471u, 0x343d2da0u, 0x235273f2u},
     gpga_db_one,
     -1,
     -1},
    {{0x15555555u, 0x15555555u, 0x15555555u, 0x1555a1e0u,
      0x2eb2094au, 0x07dde891u, 0x230e2bfau, 0x28aae6abu},
     gpga_db_one,
     -1,
     1},
    {{0x1fffffffu, 0x3fffffffu, 0x3fffffffu, 0x3fffffffu,
      0x029bd81bu, 0x360f63dfu, 0x28d28bd3u, 0x3c15f394u},
     gpga_db_one,
     -1,
     -1},
    {{0x3fffffffu, 0x3fffffffu, 0x3fffffffu, 0x3fffffffu,
      0x39e04b7eu, 0x08e4e337u, 0x1a1e2ed3u, 0x23e85705u},
     gpga_db_one,
     -1,
     1},
};

#define constant_poly_ptr ((scs_const_ptr)&constant_poly)

GPGA_CONST scs log2_table_ti[13] = {
    {{0x2298ac1fu, 0x33457c40u, 0x1c141e66u, 0x3eaaab29u,
      0x1030633du, 0x048bef17u, 0x1a91d6a1u, 0x22230522u},
     gpga_db_one,
     -1,
     -1},
    {{0x1a8ff971u, 0x20429786u, 0x017fd3b7u, 0x35f97452u,
      0x0bb6c306u, 0x15c5da64u, 0x3efe1069u, 0x2fb2da05u},
     gpga_db_one,
     -1,
     -1},
    {{0x132bfee3u, 0x1c3b9a19u, 0x1a24978du, 0x38d67caeu,
      0x3878c2dfu, 0x02d6ff98u, 0x24e1a2a9u, 0x1b4f917du},
     gpga_db_one,
     -1,
     -1},
    {{0x0c544c05u, 0x17f7a64cu, 0x3354dbf1u, 0x1bec1a57u,
      0x2e31ce56u, 0x2b7fe8e9u, 0x20d510a7u, 0x19e262f3u},
     gpga_db_one,
     -1,
     -1},
    {{0x05f58125u, 0x2cfbb4c6u, 0x1ced1447u, 0x38c2b4e4u,
      0x3edd56b0u, 0x1637ed79u, 0x2a14f4feu, 0x3db0ce67u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_double_zero(0u),
     0,
     1},
    {{0x0598fdbeu, 0x2c913167u, 0x33314e09u, 0x2144575au,
      0x30f2941fu, 0x0fab1d27u, 0x3e612491u, 0x0849fe51u},
     gpga_db_one,
     -1,
     1},
    {{0x0ae00d1cu, 0x3f7ad0f3u, 0x3d005890u, 0x140d175bu,
      0x289279f3u, 0x14744b36u, 0x0203df2cu, 0x209a4bf4u},
     gpga_db_one,
     -1,
     1},
    {{0x0fde0b5cu, 0x204d0144u, 0x1d46ccc5u, 0x0f09de6bu,
      0x39267ab7u, 0x1b5a9520u, 0x35aacfb1u, 0x311d7642u},
     gpga_db_one,
     -1,
     1},
    {{0x149a784bu, 0x3346e2bfu, 0x2492bf6fu, 0x3d36bf6du,
      0x0cd96c55u, 0x3f8decebu, 0x14e91b6au, 0x32020b9eu},
     gpga_db_one,
     -1,
     1},
    {{0x191bba89u, 0x07c5c22du, 0x0b2b5056u, 0x2e1a7156u,
      0x06176ea2u, 0x3eba3cb1u, 0x202cdeeeu, 0x366ac306u},
     gpga_db_one,
     -1,
     1},
    {{0x1d6753e0u, 0x0cba83bfu, 0x23ebe199u, 0x015554d6u,
      0x2fcf9cc2u, 0x3b7410e8u, 0x256e295eu, 0x1ddcfaddu},
     gpga_db_one,
     -1,
     1},
    {{0x21820a01u, 0x2b1d532cu, 0x104aea53u, 0x1b12ef0au,
      0x2a0fca1au, 0x1dd6be1du, 0x0730b711u, 0x35eaa979u},
     gpga_db_one,
     -1,
     1},
};

GPGA_CONST scs log2_constant_poly[20] = {
    {{0x04a3610eu, 0x3280f22fu, 0x1de04b83u, 0x13d0592cu,
      0x01c1f347u, 0x0e59a808u, 0x0bcf5cfau, 0x3009a167u},
     gpga_db_one,
     -1,
     -1},
    {{0x04e191a1u, 0x27127aeau, 0x2fb498c3u, 0x3f8e3721u,
      0x2688ed52u, 0x38503e4fu, 0x3b216e42u, 0x17d8666cu},
     gpga_db_one,
     -1,
     1},
    {{0x05212933u, 0x12c0276eu, 0x1534c74eu, 0x1ffc3802u,
      0x36935f91u, 0x25fe848du, 0x2bade416u, 0x110f0662u},
     gpga_db_one,
     -1,
     -1},
    {{0x056e6838u, 0x35aa4f4au, 0x2ae2f258u, 0x30768483u,
      0x2bc43d6eu, 0x176d2fe1u, 0x17488263u, 0x30f7670bu},
     gpga_db_one,
     -1,
     1},
    {{0x05c551dau, 0x1166e535u, 0x10625f98u, 0x08af81eeu,
      0x04feb59eu, 0x2906123eu, 0x0b31f878u, 0x0693beb1u},
     gpga_db_one,
     -1,
     -1},
    {{0x0627cec6u, 0x20792d0du, 0x181b90abu, 0x0c8e3405u,
      0x1fe3b53fu, 0x0f6d3b7au, 0x00eefd12u, 0x0808849bu},
     gpga_db_one,
     -1,
     1},
    {{0x06985d8au, 0x27a1d37du, 0x3fa3c0e9u, 0x37e7d679u,
      0x0379ed1du, 0x25a15e16u, 0x3ef8c491u, 0x2414ab4fu},
     gpga_db_one,
     -1,
     -1},
    {{0x071a3d5au, 0x0d27a702u, 0x138411a6u, 0x15701caeu,
      0x31ef415du, 0x1985227du, 0x31a4c54cu, 0x15d3a279u},
     gpga_db_one,
     -1,
     1},
    {{0x07b1c277u, 0x03a04150u, 0x3be8d05bu, 0x2f373cf4u,
      0x105c73c2u, 0x0ee0272bu, 0x0b2bf018u, 0x1ad7026fu},
     gpga_db_one,
     -1,
     -1},
    {{0x0864d424u, 0x328046b9u, 0x132f0fc6u, 0x2b14d554u,
      0x189d4b79u, 0x2ec40035u, 0x15083d44u, 0x33a13b8cu},
     gpga_db_one,
     -1,
     1},
    {{0x093bb628u, 0x1df37fcfu, 0x022eb69au, 0x0b325d1du,
      0x0625c904u, 0x1ccaa7aeu, 0x0ed7ce1du, 0x3202ff74u},
     gpga_db_one,
     -1,
     -1},
    {{0x0a42589eu, 0x2f80551eu, 0x3eb3ec54u, 0x1a1094cdu,
      0x23649796u, 0x328d4019u, 0x3f6371c6u, 0x37a359a1u},
     gpga_db_one,
     -1,
     1},
    {{0x0b8aa3b2u, 0x25705fc2u, 0x3bbedccau, 0x35a13588u,
      0x15c06cdcu, 0x39fbf52cu, 0x2b666451u, 0x350d8287u},
     gpga_db_one,
     -1,
     -1},
    {{0x0d30bb15u, 0x0f5bdb27u, 0x3b23121cu, 0x383c6bb0u,
      0x2f3b4fd2u, 0x2dba0ce0u, 0x0230a445u, 0x306b1136u},
     gpga_db_one,
     -1,
     1},
    {{0x0f6384eeu, 0x07407faeu, 0x24fe0aa6u, 0x3c7aefd2u,
      0x12953456u, 0x0d1d1991u, 0x2af86714u, 0x26b4c925u},
     gpga_db_one,
     -1,
     -1},
    {{0x12776c50u, 0x3be6ff9eu, 0x12ca7330u, 0x1a6d448bu,
      0x1ef18433u, 0x375e9366u, 0x21f2efcfu, 0x38ab1d93u},
     gpga_db_one,
     -1,
     1},
    {{0x17154765u, 0x0ae0bf85u, 0x377d0ffdu, 0x2836248au,
      0x3b8e6858u, 0x123f10aeu, 0x0c65387cu, 0x26048456u},
     gpga_db_one,
     -1,
     -1},
    {{0x1ec709dcu, 0x0e80ff5du, 0x09fc1552u, 0x0af12c97u,
      0x37bd6a2cu, 0x3ebc8f65u, 0x113a7c28u, 0x1663571au},
     gpga_db_one,
     -1,
     1},
    {{0x2e2a8ecau, 0x15c17f0bu, 0x2efa1ffbu, 0x10691d3du,
      0x09a93721u, 0x2d59e835u, 0x1baea424u, 0x28b120a0u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x1c551d94u, 0x2b82fe17u, 0x1df43ff6u,
      0x20d23a7cu, 0x3b9ff352u, 0x14d59cbfu, 0x00fe1bdbu},
     gpga_db_one,
     0,
     1},
};

#define log2_constant_poly_ptr ((scs_const_ptr)&log2_constant_poly)
#define log2_table_ti_ptr ((scs_const_ptr)&log2_table_ti)
#define log2_table_inv_wi_ptr ((scs_const_ptr)&table_inv_wi)

GPGA_CONST scs sin_scs_poly[13] = {
    {{0x0000004fu, 0x18f09e97u, 0x212a5b47u, 0x39f049a7u,
      0x3bd24b7bu, 0x23af8e4au, 0x34d618d1u, 0x013262b6u},
     gpga_db_one,
     -3,
     1},
    {{0x0000bb0cu, 0x3cb17c1eu, 0x37e81e1au, 0x37195774u,
      0x129a10eeu, 0x1f4e5bd3u, 0x14c6fde7u, 0x15d2d8fdu},
     gpga_db_one,
     -3,
     -1},
    {{0x0171b8eeu, 0x330c68b0u, 0x185bd7cdu, 0x085a8b04u,
      0x00c4a269u, 0x38b64e54u, 0x18d2c602u, 0x292ac5c1u},
     gpga_db_one,
     -3,
     1},
    {{0x00000009u, 0x1e9368cfu, 0x32371688u, 0x0021bd52u,
      0x19613de4u, 0x2e9e92f1u, 0x27c78771u, 0x3f5cc281u},
     gpga_db_one,
     -2,
     -1},
    {{0x00000ca9u, 0x18ee0615u, 0x210b66bbu, 0x339e7ef6u,
      0x2c29a154u, 0x1f2d1a6eu, 0x0dbea028u, 0x1214063fu},
     gpga_db_one,
     -2,
     1},
    {{0x000d73f9u, 0x3ce67703u, 0x36256279u, 0x0f1440e6u,
      0x24128080u, 0x2e0c47a6u, 0x27b32155u, 0x2f115571u},
     gpga_db_one,
     -2,
     -1},
    {{0x0b092309u, 0x350da12fu, 0x24b28056u, 0x31def2f9u,
      0x24044621u, 0x074f188cu, 0x3829c278u, 0x09324bddu},
     gpga_db_one,
     -2,
     1},
    {{0x0000001au, 0x399159fdu, 0x144e38feu, 0x1d141fdbu,
      0x1666d4dfu, 0x1bf80c34u, 0x17ad2747u, 0x0566e4f9u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000b8eu, 0x3c74aad8u, 0x399c7d56u, 0x03906123u,
      0x2a904384u, 0x14010dddu, 0x155495d5u, 0x0daca530u},
     gpga_db_one,
     -1,
     1},
    {{0x00034034u, 0x00d00d00u, 0x34034034u, 0x00d006ceu,
      0x348eca38u, 0x3f5dd48fu, 0x205a5108u, 0x07e84299u},
     gpga_db_one,
     -1,
     -1},
    {{0x00888888u, 0x22222222u, 0x08888888u, 0x2222220du,
      0x270976f8u, 0x08cd270cu, 0x22fa3253u, 0x22fd1924u},
     gpga_db_one,
     -1,
     1},
    {{0x0aaaaaaau, 0x2aaaaaaau, 0x2aaaaaaau, 0x2aaaaaaau,
      0x27da7c46u, 0x26422150u, 0x2f255a8cu, 0x055ff185u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_db_one,
     0,
     1},
};

GPGA_CONST scs cos_scs_poly[14] = {
    {{0x00000003u, 0x036a24afu, 0x0b960021u, 0x36ab92b4u,
      0x251cbcb3u, 0x24a97fbbu, 0x175c8edbu, 0x26ff1299u},
     gpga_db_one,
     -3,
     -1},
    {{0x000007cbu, 0x0d2a2e99u, 0x10f8748eu, 0x389f34e6u,
      0x2313bd8fu, 0x20be0984u, 0x333a6d8eu, 0x1b807806u},
     gpga_db_one,
     -3,
     1},
    {{0x0010ce39u, 0x1987726fu, 0x1eea7e37u, 0x1ddf4cfau,
      0x1ec5b5c4u, 0x120cb52eu, 0x37c988dcu, 0x0a3377bau},
     gpga_db_one,
     -3,
     -1},
    {{0x1e542ba3u, 0x3f3c9ecfu, 0x00267a43u, 0x134ef26eu,
      0x1ff9a381u, 0x26109691u, 0x3d30a560u, 0x223f772eu},
     gpga_db_one,
     -3,
     1},
    {{0x000000b4u, 0x04f0c772u, 0x3e3a3149u, 0x04b97d58u,
      0x13ea02e8u, 0x370088b3u, 0x212bb00bu, 0x06497545u},
     gpga_db_one,
     -2,
     -1},
    {{0x0000d73fu, 0x27ce6770u, 0x0f5c0984u, 0x0a909e14u,
      0x043eaabbu, 0x3925da80u, 0x0d2bb88bu, 0x1b8d9c50u},
     gpga_db_one,
     -2,
     1},
    {{0x00c9cba5u, 0x1180f93au, 0x1053f6e9u, 0x07a12245u,
      0x25d6d8feu, 0x0183a89fu, 0x11fe0eccu, 0x116ceaf8u},
     gpga_db_one,
     -2,
     -1},
    {{0x00000002u, 0x0f76c77fu, 0x31b12f6au, 0x226bf583u,
      0x38324c53u, 0x0bebff35u, 0x1d8aa929u, 0x0902e04au},
     gpga_db_one,
     -1,
     1},
    {{0x00000127u, 0x393edde2u, 0x1f5c72efu, 0x005b3093u,
      0x064ee888u, 0x10be8aa5u, 0x3b5ca412u, 0x36d45cbeu},
     gpga_db_one,
     -1,
     -1},
    {{0x00006806u, 0x201a01a0u, 0x06806806u, 0x2019fff2u,
      0x243ec0f6u, 0x3857f8fcu, 0x1e36a7c0u, 0x0c2c43d2u},
     gpga_db_one,
     -1,
     1},
    {{0x0016c16cu, 0x05b05b05u, 0x2c16c16cu, 0x05b05af9u,
      0x29545050u, 0x0f1975d1u, 0x0cc59466u, 0x3fbcae72u},
     gpga_db_one,
     -1,
     -1},
    {{0x02aaaaaau, 0x2aaaaaaau, 0x2aaaaaaau, 0x2aaaaaaau,
      0x222d48a2u, 0x0e242eacu, 0x1247b316u, 0x3b9fbfe1u},
     gpga_db_one,
     -1,
     1},
    {{0x1fffffffu, 0x3fffffffu, 0x3fffffffu, 0x3fffffffu,
      0x3ffc089fu, 0x2c68799bu, 0x0b48bbdeu, 0x2ea8aa51u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_db_one,
     0,
     1},
};

GPGA_CONST scs tan_scs_poly[35] = {
    {{0x0049c3c8u, 0x3614b771u, 0x24336d30u, 0x18260f52u,
      0x1c63a612u, 0x3c9708b1u, 0x2c030207u, 0x3b60a762u},
     gpga_db_one,
     -2,
     1},
    {{0x02505729u, 0x078bbff7u, 0x277b7292u, 0x2ae76500u,
      0x3326025du, 0x0785010au, 0x2e88ed52u, 0x23a7933au},
     gpga_db_one,
     -2,
     -1},
    {{0x0977a350u, 0x3804d188u, 0x2d3b121cu, 0x3b0228d8u,
      0x002a8c44u, 0x2d5dfc73u, 0x2507d88eu, 0x3cfff02bu},
     gpga_db_one,
     -2,
     1},
    {{0x19163b42u, 0x137d2c1bu, 0x19fd92a2u, 0x1d367da1u,
      0x28a08aa6u, 0x19489cc8u, 0x2fbf37c1u, 0x2a39fb53u},
     gpga_db_one,
     -2,
     -1},
    {{0x3099f85bu, 0x38f9459bu, 0x195e2a8du, 0x3797159cu,
      0x34148387u, 0x3b401948u, 0x1983fd31u, 0x184ad9fdu},
     gpga_db_one,
     -2,
     1},
    {{0x00000001u, 0x07d00484u, 0x1f25e4feu, 0x34768b93u,
      0x3b55eb65u, 0x12227622u, 0x04fedebcu, 0x2e961f5cu},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x15e6a9a6u, 0x23b954f6u, 0x248f5fbau,
      0x38fabeafu, 0x059e710au, 0x3e68659eu, 0x08f9d054u},
     gpga_db_one,
     -1,
     1},
    {{0x00000001u, 0x1111ac11u, 0x2b29b510u, 0x0408b38cu,
      0x29617027u, 0x289298c6u, 0x2fe4c001u, 0x34903af7u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x05c0e901u, 0x1495b536u, 0x3d0dceffu,
      0x11d27769u, 0x314f4319u, 0x2adb4907u, 0x04581607u},
     gpga_db_one,
     -1,
     1},
    {{0x23f4aaa3u, 0x3810e7e3u, 0x377f317cu, 0x118eb173u,
      0x271248c9u, 0x0dc34f76u, 0x38c69c1au, 0x1715bf81u},
     gpga_db_one,
     -2,
     -1},
    {{0x2f026372u, 0x048fabdau, 0x157f9376u, 0x1ee01201u,
      0x1d337305u, 0x3277dac2u, 0x0388ddbbu, 0x1e444286u},
     gpga_db_one,
     -2,
     1},
    {{0x28924a94u, 0x186459bdu, 0x07f03e85u, 0x3c711128u,
      0x3ae6e0c9u, 0x1e84f311u, 0x3d3d9fe2u, 0x186b0f23u},
     gpga_db_one,
     -2,
     1},
    {{0x00000002u, 0x07fc704fu, 0x2fe41d64u, 0x39e487cfu,
      0x184c49c2u, 0x0bee9906u, 0x1478fbedu, 0x3b189f6au},
     gpga_db_one,
     -1,
     1},
    {{0x00000005u, 0x00de33a3u, 0x25923c9cu, 0x2ca62cbau,
      0x20bdef99u, 0x128b5ff5u, 0x3fc900eau, 0x04ca7a66u},
     gpga_db_one,
     -1,
     1},
    {{0x0000000cu, 0x1cde875fu, 0x11fe6b7au, 0x1bf8f0eeu,
      0x1fdaf92du, 0x3db0bd62u, 0x15f329f2u, 0x1aaaa7deu},
     gpga_db_one,
     -1,
     1},
    {{0x0000001eu, 0x2ca1db60u, 0x16b02396u, 0x27782b2du,
      0x13c68ddeu, 0x28c97a48u, 0x1971af0du, 0x190791d2u},
     gpga_db_one,
     -1,
     1},
    {{0x0000004bu, 0x2ff1aba9u, 0x215c7e5fu, 0x0ec7c100u,
      0x29f9ff6eu, 0x0f5a5e50u, 0x1b028fafu, 0x24fd23e7u},
     gpga_db_one,
     -1,
     1},
    {{0x000000bau, 0x39bb1378u, 0x228e6aabu, 0x1cd5a3f6u,
      0x0df50eb5u, 0x2fe1339fu, 0x3bf44406u, 0x0623c42cu},
     gpga_db_one,
     -1,
     1},
    {{0x000001cdu, 0x0a67c230u, 0x15e26fdeu, 0x2d6f2ed2u,
      0x1e5d5eedu, 0x13306d09u, 0x2bc7fd07u, 0x3e19f2dbu},
     gpga_db_one,
     -1,
     1},
    {{0x00000471u, 0x37df8b71u, 0x36c6ae8bu, 0x247f572bu,
      0x11ae0684u, 0x2c9d50b0u, 0x3b8c1296u, 0x142cbe10u},
     gpga_db_one,
     -1,
     1},
    {{0x00000af7u, 0x25b48fd7u, 0x22f9b7b5u, 0x1bf47d23u,
      0x2d5e7cb4u, 0x29eec735u, 0x12e994fcu, 0x38f234bdu},
     gpga_db_one,
     -1,
     1},
    {{0x00001b0fu, 0x1cb4fb69u, 0x3117e917u, 0x2d08cb65u,
      0x3ed33963u, 0x11bb69f4u, 0x0b13ad8fu, 0x2ed16814u},
     gpga_db_one,
     -1,
     1},
    {{0x000042c4u, 0x32d39a62u, 0x3b08d8b5u, 0x1a0a769du,
      0x102ec25du, 0x27c1ccb1u, 0x2d7c3ac9u, 0x1484f03bu},
     gpga_db_one,
     -1,
     1},
    {{0x0000a4beu, 0x31dd44a4u, 0x21406decu, 0x00d8ae9bu,
      0x263ef3acu, 0x26d69803u, 0x1d7f349du, 0x2a7a1ff7u},
     gpga_db_one,
     -1,
     1},
    {{0x0001967eu, 0x062bf2beu, 0x2d29a81au, 0x3528530au,
      0x2be09523u, 0x150ffb17u, 0x1543b0eeu, 0x2e51e70cu},
     gpga_db_one,
     -1,
     1},
    {{0x0003eafau, 0x3b9a68b3u, 0x083c6556u, 0x3c8a24a1u,
      0x2485ea61u, 0x0e5b9b2cu, 0x2d16e9bau, 0x0700d411u},
     gpga_db_one,
     -1,
     1},
    {{0x0009aac1u, 0x09006ce8u, 0x229156e9u, 0x1010a671u,
      0x167b367eu, 0x02d471c6u, 0x33700cd8u, 0x180a5404u},
     gpga_db_one,
     -1,
     1},
    {{0x0017da36u, 0x114add78u, 0x2b6a1c11u, 0x39bc050bu,
      0x1fa62cdeu, 0x202525f2u, 0x06797d2eu, 0x188697d8u},
     gpga_db_one,
     -1,
     1},
    {{0x003ada7au, 0x070abeefu, 0x36bf9822u, 0x114b4de5u,
      0x29c19548u, 0x21cff3b2u, 0x1b67ac39u, 0x3afe7677u},
     gpga_db_one,
     -1,
     1},
    {{0x0091371au, 0x2bcd8479u, 0x07ada8e1u, 0x2d73a327u,
      0x39d10b36u, 0x2a1371abu, 0x10b30f6eu, 0x139bddaau},
     gpga_db_one,
     -1,
     1},
    {{0x01664f48u, 0x20b043e7u, 0x332d6bbau, 0x1281f45du,
      0x05c107b1u, 0x0b2e1a96u, 0x378ae51eu, 0x2c9d2be3u},
     gpga_db_one,
     -1,
     1},
    {{0x03743743u, 0x1d0dd0ddu, 0x03743743u, 0x1d0dc5f0u,
      0x20d05868u, 0x240b23beu, 0x38da29dfu, 0x3866f03cu},
     gpga_db_one,
     -1,
     1},
    {{0x08888888u, 0x22222222u, 0x08888888u, 0x22222226u,
      0x288230eeu, 0x27ff32dfu, 0x1b2f8f82u, 0x12608ad5u},
     gpga_db_one,
     -1,
     1},
    {{0x15555555u, 0x15555555u, 0x15555555u, 0x15555555u,
      0x154195d6u, 0x3f2e970au, 0x24a360e2u, 0x3f8caf8eu},
     gpga_db_one,
     -1,
     1},
    {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_db_one,
     0,
     1},
};

#define DEGREE_SIN_SCS 13
#define DEGREE_COS_SCS 14
#define DEGREE_TAN_SCS 35

#define sin_scs_poly_ptr ((scs_const_ptr)&sin_scs_poly)
#define cos_scs_poly_ptr ((scs_const_ptr)&cos_scs_poly)
#define tan_scs_poly_ptr ((scs_const_ptr)&tan_scs_poly)

GPGA_CONST scs atan_constant_poly[10] = {
    {{0x035e50d7u, 0x250d7943u, 0x179435e5u, 0x035e50d7u,
      0x250d7943u, 0x179435e5u, 0x035e50d7u, 0x250d7943u},
     gpga_db_one,
     -1,
     -1},
    {{0x03c3c3c3u, 0x30f0f0f0u, 0x3c3c3c3cu, 0x0f0f0f0fu,
      0x03c3c3c3u, 0x30f0f0f0u, 0x3c3c3c3cu, 0x0f0f0f0fu},
     gpga_db_one,
     -1,
     1},
    {{0x04444444u, 0x11111111u, 0x04444444u, 0x11111111u,
      0x04444444u, 0x11111111u, 0x04444444u, 0x11111111u},
     gpga_db_one,
     -1,
     -1},
    {{0x04ec4ec4u, 0x3b13b13bu, 0x04ec4ec4u, 0x3b13b13bu,
      0x04ec4ec4u, 0x3b13b13bu, 0x04ec4ec4u, 0x3b13b13bu},
     gpga_db_one,
     -1,
     1},
    {{0x05d1745du, 0x05d1745du, 0x05d1745du, 0x05d1745du,
      0x05d1745du, 0x05d1745du, 0x05d1745du, 0x05d1745du},
     gpga_db_one,
     -1,
     -1},
    {{0x071c71c7u, 0x071c71c7u, 0x071c71c7u, 0x071c71c7u,
      0x071c71c7u, 0x071c71c7u, 0x071c71c7u, 0x071c71c7u},
     gpga_db_one,
     -1,
     1},
    {{0x09249249u, 0x09249249u, 0x09249249u, 0x09249249u,
      0x09249249u, 0x09249249u, 0x09249249u, 0x09249249u},
     gpga_db_one,
     -1,
     -1},
    {{0x0cccccccu, 0x33333333u, 0x0cccccccu, 0x33333333u,
      0x0cccccccu, 0x33333333u, 0x0cccccccu, 0x33333333u},
     gpga_db_one,
     -1,
     1},
    {{0x15555555u, 0x15555555u, 0x15555555u, 0x15555555u,
      0x15555555u, 0x15555555u, 0x15555555u, 0x15555555u},
     gpga_db_one,
     -1,
     -1},
    {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
      0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     gpga_db_one,
     0,
     1},
};

#define atan_constant_poly_ptr ((scs_const_ptr)&atan_constant_poly)

GPGA_CONST scs atan_inv_pi_scs = {
    {0x145f306du, 0x327220a9u, 0x13f84eafu, 0x28fa9a6eu,
     0x381b6c52u, 0x2cc9e21cu, 0x2083fca2u, 0x31d5ef5du},
    gpga_db_one,
    -1,
    1};

#define atan_inv_pi_scs_ptr ((scs_const_ptr)&atan_inv_pi_scs)

GPGA_CONST gpga_double atan_blolo[62] = {
    0xb8d1c5f3a947cce9ul, 0xb8b059433c749846ul, 0x38fc2f2a267751fbul,
    0x38d44e61dda4249dul, 0xb906fbfaa5b245c8ul, 0xb8de1e9867cabe65ul,
    0x38bd7b8f45d13048ul, 0xb904943623ad369bul, 0xb8f98586f6bb1db7ul,
    0xb89bc985cb4d6219ul, 0xb8c4002c9c3884f4ul, 0x3910cbc4893058d9ul,
    0xb8f5408bff010bb5ul, 0xb908570fb5813578ul, 0x39155b897e967248ul,
    0xb8cd159a2031f115ul, 0xb8e8d9c60fc3c81cul, 0x38c0ac308219f20dul,
    0x38b3650632edcb95ul, 0xb8fd57b05932e7c2ul, 0x38c7382791a2d916ul,
    0xb92fe60d9ec5116cul, 0x3903aadf6a3b8ae2ul, 0xb88d6a0171b87c3cul,
    0xb8e17427ea5ac8e8ul, 0x38f4ed52f776b005ul, 0xb9265b2375d2b05cul,
    0xb8da45daafeab282ul, 0x3912ef75bd16d3d4ul, 0x390ab3d7e904b022ul,
    0xb8ee688b3f9f468bul, 0xb9225df671a98823ul, 0xb8cc82b0522be0f8ul,
    0xb912afe088bb6a0bul, 0x38d4dc998640fae5ul, 0xb9016fc6406a343bul,
    0x3909804c126fc74cul, 0x39051033e5b0956ful, 0xb910cadc203b114cul,
    0x38efcac7f97ed36bul, 0xb92464c0b51d77b4ul, 0x391157f32d4a03deul,
    0xb92bd929136a8ddeul, 0x390f1514035df6b3ul, 0xb92606ec19db7e3bul,
    0x393b104bcda1b51dul, 0x3935fb01f88a520cul, 0xb9189ed11940944dul,
    0xb914e8f238d4794bul, 0xb93df07a73581cf5ul, 0xb939c22f727ce10cul,
    0xb8ef37d226e1a810ul, 0x391a425145155daaul, 0xb9126d2fa42d0a9aul,
    0xb93caf8b3dfaffa5ul, 0x3919c170a9a35831ul, 0xb924c2b3ae51e4b3ul,
    0xb8defb7d0de98917ul, 0xb916ec391abe373bul, 0x391c5ba44135adbeul,
    0xb9112692d7179e60ul, 0x392f96b5bcc93753ul,
};

GPGA_CONST gpga_double atan_fast_halfpi = 0x3ff921fb54442d18ul;
GPGA_CONST gpga_double atan_fast_halfpi_plus_inf = 0x3ff921fb54442d19ul;
GPGA_CONST gpga_double atan_fast_min_reduction_needed = 0x3f89fdf8bcce533dul;
GPGA_CONST gpga_double atan_fast_invpih = 0x3fd45f306dc9c883ul;
GPGA_CONST gpga_double atan_fast_invpil = 0xbc76b01ec5417056ul;

GPGA_CONST gpga_double atan_fast_rncst[4] = {
    0x3ff0046e5629e4e3ul,
    0x3ff00036ddc523c4ul,
    0x3ff007447e47b9f2ul,
    0x3ff0000ae2d4671ful,
};

GPGA_CONST gpga_double atan_fast_epsilon[4] = {
    0x3bf1ac7dfb3fc0a6ul,
    0x3baaee76d65775a4ul,
    0x3bfcff7222585274ul,
    0x3b83c5950893a3b9ul,
};

GPGA_CONST gpga_double atan_fast_coef_poly[4] = {
    0x3fbc71c71c71c71cul,
    0xbfc2492492492492ul,
    0x3fc999999999999aul,
    0xbfd5555555555555ul,
};

GPGA_CONST gpga_double atan_fast_table[62][4] = {
    {0x3f89fdf8bcce533dul, 0x3f99ff0b27760007ul, 0x3f99fd9d4969f96cul,
     0xbc301997750685eaul},
    {0x3fa3809f90cebc31ul, 0x3faa0355e3547cc2ul, 0x3fa9fd9d4936262dul,
     0xbc4c1f963a2d0f59ul},
    {0x3fb0441968fba526ul, 0x3fb387e0abf7bfb9ul, 0x3fb37e35f6c1e06cul,
     0xbc5567108378e024ul},
    {0x3fb6cd46abcdfa25ul, 0x3fba1491e265b8abul, 0x3fb9fd9d48cf1996ul,
     0xbc4360dcb73bed3cul},
    {0x3fbd5e096d2ea546ul, 0x3fc054fa9124d46bul, 0x3fc03e824d618156ul,
     0x3c62604e49a86344ul},
    {0x3fc1fc4ed691e891ul, 0x3fc3a52651f57043ul, 0x3fc37e35f64ef83eul,
     0x3c5f2b13b4a55ea6ul},
    {0x3fc54fa6531f610bul, 0x3fc6fbf4b99f2601ul, 0x3fc6bde99f302425ul,
     0xbc23b62ac4d330ceul},
    {0x3fc8aa380550eaf1ul, 0x3fca5a9761d9d63dul, 0x3fc9fd9d48053fc8ul,
     0xbc67a12c98474af1ul},
    {0x3fcc0d3ab8975bd9ul, 0x3fcdc24abcbf6abcul, 0x3fcd3d50f0ce8dd9ul,
     0x3c6ef2573a31d353ul},
    {0x3fcf79f0fee46885ul, 0x3fd09a2bfcf66b85ul, 0x3fd03e824cc62c6bul,
     0x3c5da59495c6e482ul},
    {0x3fd178d5943274caul, 0x3fd2590b8943e603ul, 0x3fd1de5c211f7969ul,
     0x3c3c4e870b01ef44ul},
    {0x3fd33ae4b2cfb5f7ul, 0x3fd41e7881b0d1dful, 0x3fd37e35f5735aa1ul,
     0xbc772d058d437ae9ul},
    {0x3fd503df0dd40a5bul, 0x3fd5eb311eb6a659ul, 0x3fd51e0fc9c20060ul,
     0xbc6c0f5320ba7a7bul},
    {0x3fd6d4883998dd14ul, 0x3fd7bffeaaf865b1ul, 0x3fd6bde99e0b9e67ul,
     0x3c797792870d5e8eul},
    {0x3fd8adaf964abfa5ul, 0x3fd99db700a6f44eul, 0x3fd85dc372506bcaul,
     0x3c720bd143a1cba3ul},
    {0x3fda9031e241114eul, 0x3fdb853e327f0e90ul, 0x3fd9fd9d4690a2c8ul,
     0xbc6a048eefb73034ul},
    {0x3fdc7cfafb78b41dul, 0x3fdd778867be333aul, 0x3fdb9d771acc80a6ul,
     0x3c6a9bd7e8fb154dul},
    {0x3fde7507d82b9dc6ul, 0x3fdf759bf3b4aaccul, 0x3fdd3d50ef044589ul,
     0x3c57585709e8652eul},
    {0x3fe03cb45ff4b2abul, 0x3fe0c049d9952beful, 0x3fdedd2ac338344aul,
     0x3c23c78781513292ul},
    {0x3fe145a1e826e4eaul, 0x3fe1ccd0ddd96de0ul, 0x3fe03e824bb44923ul,
     0x3c8caf0149df0a58ul},
    {0x3fe255ebed462bacul, 0x3fe2e10935b0819cul, 0x3fe10e6f35cad39dul,
     0x3c45db330d1eb36bul},
    {0x3fe36e3fd4cdd9acul, 0x3fe3fda7f51235d9ul, 0x3fe1de5c1fdfde84ul,
     0x3c86fbefd3921ca6ul},
    {0x3fe48f5ae1fb2991ul, 0x3fe5237318d0ccb0ul, 0x3fe2ae4909f38fc9ul,
     0xbc84bbb5058084c6ul},
    {0x3fe5ba0c5fe86e27ul, 0x3fe65343da588d41ul, 0x3fe37e35f4060e3dul,
     0x3bf6235cae25b9b7ul},
    {0x3fe6ef3822c19a5dul, 0x3fe78e09629002bcul, 0x3fe44e22de17817ful,
     0xbc85f46e1a6d25e3ul},
    {0x3fe82fd970f967bdul, 0x3fe8d4cbee8b9555ul, 0x3fe51e0fc82811dcul,
     0xbc81ccf7fcb9720cul},
    {0x3fe97d0669351a0dul, 0x3fea28b07d0d616aul, 0x3fe5edfcb237e838ul,
     0xbc833ea8f8f29048ul},
    {0x3fead7f3fe730fcdul, 0x3feb8afd21335e3dul, 0x3fe6bde99c472df1ul,
     0x3c5e812780ded249ul},
    {0x3fec41faaa0a733eul, 0x3fecfd1e1d9a7669ul, 0x3fe78dd686560cc5ul,
     0xbc7e962f9396d511ul},
    {0x3fedbc9bfaeeeadful, 0x3fee80abf41419e7ul, 0x3fe85dc37064aeb4ul,
     0x3c77c0fc5225acd1ul},
    {0x3fef498933ac790aul, 0x3ff00bb950bb2d02ul, 0x3fe92db05a733de9ul,
     0xbc6598c9a318d041ul},
    {0x3ff075559ac922b4ul, 0x3ff0e1bd25f4bc57ul, 0x3fe9fd9d4481e499ul,
     0xbc82e89f9464a40dul},
    {0x3ff151160440e8d3ul, 0x3ff1c388dec8da1aul, 0x3feacd8a2e90cceaul,
     0x3c60b8dbfe4ac33bul},
    {0x3ff23941329d3dd8ul, 0x3ff2b26d8f9cf3adul, 0x3feb9d7718a020d8ul,
     0x3c714ea343ddc7eeul},
    {0x3ff32f3fe2db7094ul, 0x3ff3afedc485093aul, 0x3fec6d6402b00a17ul,
     0x3c5cf722c1e56981ul},
    {0x3ff434b0d38a35d7ul, 0x3ff4bdc716d89bc7ul, 0x3fed3d50ecc0b1f8ul,
     0x3c8ffd9ac689ae1bul},
    {0x3ff54b736f41f96dul, 0x3ff5ddfe11c53212ul, 0x3fee0d3dd6d24151ul,
     0xbc654f0fc830b564ul},
    {0x3ff675b5165ca5e1ul, 0x3ff712ed13170c5bul, 0x3feedd2ac0e4e05bul,
     0xbc8feac3bf676ffeul},
    {0x3ff7b601d0dea3c6ul, 0x3ff85f5711fbea40ul, 0x3fefad17aaf8b69eul,
     0x3c8b8da3e69f3af7ul},
    {0x3ff90f5979506f51ul, 0x3ff9c67f8af460dful, 0x3ff03e824a86f56bul,
     0x3c8c4641de1df40bul},
    {0x3ffa854ad74cf791ul, 0x3ffb4c494a696f14ul, 0x3ff0a678bf92516cul,
     0xbc9194ff6a0966aful},
    {0x3ffc1c16b3972246ul, 0x3ffcf55e80f0b83eul, 0x3ff10e6f349e81baul,
     0x3c9c7d123cd05c13ul},
    {0x3ffdd8ddc6db1831ul, 0x3ffec765927d039cul, 0x3ff17665a9ab9836ul,
     0xbc924dfa7b9c115bul},
    {0x3fffc1dda4f6d032ul, 0x400064a3cdec630cul, 0x3ff1de5c1eb9a624ul,
     0xbc7002f6595ec15dul},
    {0x4000ef6156aefaf2ul, 0x400181c802fa3f97ul, 0x3ff2465293c8bc28ul,
     0xbc8fe336c61e8575ul},
    {0x40021c8bfd9a80c1ul, 0x4002c078188015c0ul, 0x3ff2ae4908d8ea37ul,
     0xbc99c94356281657ul},
    {0x40036e717d67269cul, 0x4004277c17edbbf3ul, 0x3ff3163f7dea3f8cul,
     0x3c9ad32990eb721bul},
    {0x4004ecbff069f1e4ul, 0x4005bf8fa99113b9ul, 0x3ff37e35f2fccaa3ul,
     0xbc96c4d78b3674f4ul},
    {0x4006a170780169b7ul, 0x40079423eecfde39ul, 0x3ff3e62c68109926ul,
     0x3c9d395e46562d7dul},
    {0x400899b4319c3f02ul, 0x4009b48334491c9aul, 0x3ff44e22dd25b7f0ul,
     0xbc9ac40896299ac9ul},
    {0x400ae75e05b0834aul, 0x400c35956451b6e9ul, 0x3ff4b619523c32f8ul,
     0x3c98372d9a6cf82bul},
    {0x400da31d739bd0e3ul, 0x400f34b7088b2a13ul, 0x3ff51e0fc7541555ul,
     0xbc4da0e1d97cc177ul},
    {0x401078131886bc57ul, 0x40116e3d8d819944ul, 0x3ff586063c6d692dul,
     0xbc7dab9dd9d13c46ul},
    {0x4012813a8bce2241ul, 0x4013b6859f0e4ebful, 0x3ff5edfcb18837b4ul,
     0xbc858200bc854f71ul},
    {0x401515164acece78ul, 0x4016a5e9f3cd0189ul, 0x3ff655f326a48924ul,
     0xbc95dcd40ef07703ul},
    {0x401874ce9526fab9ul, 0x401a9194ba057809ul, 0x3ff6bde99bc264b7ul,
     0x3c839fb7555821b5ul},
    {0x401d11ebe094c913ul, 0x40200a31a01efacaul, 0x3ff725e010e1d0a5ul,
     0x3c86b63b56dadb03ul},
    {0x4021e2bc220dfa19ul, 0x40243445fba72898ul, 0x3ff78dd68602d21dul,
     0xbc9fd7d64d2ea6e0ul},
    {0x40273463d0337c49ul, 0x402b3dc74a4a5a1dul, 0x3ff7f5ccfb256d40ul,
     0x3c954e9ea28cf77cul},
    {0x40307b8e26350916ul, 0x4034dbd624d415bful, 0x3ff85dc37049a526ul,
     0xbc8424e5e9374500ul},
    {0x403c62cf497bf2f2ul, 0x404632086c3ec43cul, 0x3ff8c5b9e56f7bd2ul,
     0xbc81d57a2f7ec153ul},
    {0x40596cc3fa9e0ef4ul, 0x4054b2c47bff8329ul, 0x3ff8f082f333cbbaul,
     0x3c9525773909bc3aul},
};

// trigpi constants (binary64 bits)
GPGA_CONST gpga_double trigpi_twoto42 = 0x4290000000000000ul;
GPGA_CONST gpga_double trigpi_twoto52 = 0x4330000000000000ul;
GPGA_CONST gpga_double trigpi_inv128 = 0x3f80000000000000ul;
GPGA_CONST gpga_double trigpi_twoto5251 = 0x4338000000000000ul;
GPGA_CONST gpga_double trigpi_smallest = 0x0000000000000001ul;
GPGA_CONST gpga_double trigpi_pih = 0x400921fb54442d18ul;
GPGA_CONST gpga_double trigpi_pim = 0x3ca1a62633145c07ul;
GPGA_CONST gpga_double trigpi_pil = 0xb92f1976b7ed8fbcul;
GPGA_CONST gpga_double trigpi_pihh = 0x400921fb58000000ul;
GPGA_CONST gpga_double trigpi_pihm = 0xbe5dde9740000000ul;
GPGA_CONST gpga_double trigpi_pix_rncst_sin = 0x3ff0204081020409ul;
GPGA_CONST gpga_double trigpi_pix_eps_sin = 0x3c20000000000000ul;
GPGA_CONST gpga_double trigpi_pix_rncst_tan = 0x3ff0410410410411ul;
GPGA_CONST gpga_double trigpi_pix_eps_tan = 0x3c30000000000000ul;

struct GpgaTrigpiEntry {
  gpga_double sh;
  gpga_double ch;
  gpga_double sm;
  gpga_double cm;
  gpga_double sl;
  gpga_double cl;
};

GPGA_CONST GpgaTrigpiEntry gpga_trigpi_sincos_table[64] = {
    {0x0000000000000000ul, 0x3ff0000000000000ul, 0x0000000000000000ul,
     0x0000000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul},
    {0x3f992155f7a3667eul, 0x3feffd886084cd0dul, 0xbbfb1d63091a0130ul,
     0xbc81354d4556e4cbul, 0x3899e58994be786bul, 0xb923d19b52e092dbul},
    {0x3fa91f65f10dd814ul, 0x3feff621e3796d7eul, 0xbc2912bd0d569a90ul,
     0xbc6c57bc2e24aa15ul, 0xb8cd7476f4c4b019ul, 0x38f453dcf53e4baaul},
    {0x3fb2d52092ce19f6ul, 0x3fefe9cdad01883aul, 0xbc49a088a8bf6b2cul,
     0x3c6521ecd0c67e35ul, 0xb8de51df6b678492ul, 0x390c2c4c8e7c3174ul},
    {0x3fb917a6bc29b42cul, 0x3fefd88da3d12526ul, 0xbc3e2718d26ed688ul,
     0xbc887df6378811c7ul, 0xb8b18edefcf7ef57ul, 0x391ba7bd68b25db4ul},
    {0x3fbf564e56a9730eul, 0x3fefc26470e19fd3ul, 0x3c4a2704729ae56dul,
     0x3c81ec8668ecaceeul, 0x38ee28dc484e8ef5ul, 0xb923162266c5450ful},
    {0x3fc2c8106e8e613aul, 0x3fefa7557f08a517ul, 0x3c513000a89a11e0ul,
     0xbc87a0a8ca13571ful, 0x38ff07f9fe14048cul, 0xb902a212f347e949ul},
    {0x3fc5e214448b3fc6ul, 0x3fef8764fa714ba9ul, 0x3c6531ff779ddac6ul,
     0x3c7ab256778ffcb6ul, 0xb90c1de6e152ea39ul, 0xb90f44b6dc911d8dul},
    {0x3fc8f8b83c69a60bul, 0x3fef6297cff75cb0ul, 0xbc626d19b9ff8d82ul,
     0x3c7562172a361fd3ul, 0x3909b09f9ca72c69ul, 0xb9163744e82fc701ul},
    {0x3fcc0b826a7e4f63ul, 0x3fef38f3ac64e589ul, 0xbc1af1439e521935ul,
     0xbc7d7bafb51f72e6ul, 0xb8bf0cd3647fe397ul, 0xb91359fe192a6166ul},
    {0x3fcf19f97b215f1bul, 0x3fef0a7efb9230d7ul, 0xbc642deef11da2c4ul,
     0x3c752c7adc6b4989ul, 0xb90b4ce553ffbd03ul, 0xb91db915a9794d33ul},
    {0x3fd111d262b1f677ul, 0x3feed740e7684963ul, 0x3c7824c20ab7aa9aul,
     0x3c7e82c791f59cc2ul, 0xb91779f4232b3b53ul, 0xb88eea7cbd5ac167ul},
    {0x3fd294062ed59f06ul, 0x3fee9f4156c62ddaul, 0xbc75d28da2c4612dul,
     0x3c8760b1e2e3f81eul, 0x3917eea71c14d05cul, 0xb921ce7542369ecdul},
    {0x3fd4135c94176601ul, 0x3fee6288ec48e112ul, 0x3c70c97c4afa2518ul,
     0xbc616b56f2847754ul, 0x38fe6057b0a0a42ful, 0xb9054aec99b7a418ul},
    {0x3fd58f9a75ab1fddul, 0x3fee212104f686e5ul, 0xbc1efdc0d58cf620ul,
     0xbc8014c76c126527ul, 0xb88f072f54189325ul, 0x3920e62b13b565c2ul},
    {0x3fd7088530fa459ful, 0x3feddb13b6ccc23cul, 0xbc744b19e0864c5dul,
     0x3c883c37c6107db3ul, 0x391bc76fbdd51dfdul, 0x3912c06bf13eb37ful},
    {0x3fd87de2a6aea963ul, 0x3fed906bcf328d46ul, 0xbc672cedd3d5a610ul,
     0x3c7457e610231ac2ul, 0xb8f11e4420e0a4b5ul, 0xb904f3f87abe1619ul},
    {0x3fd9ef7943a8ed8aul, 0x3fed4134d14dc93aul, 0x3c66da81290bdbabul,
     0xbc84ef5295d25af2ul, 0xb8f4e8de9013a792ul, 0xb9242fb98551f41eul},
    {0x3fdb5d1009e15cc0ul, 0x3feced7af43cc773ul, 0x3c65b362cb974183ul,
     0xbc5e7b6bb5ab58aeul, 0xb8ecfcff7c31af0cul, 0x38e525e5e3766505ul},
    {0x3fdcc66e9931c45eul, 0x3fec954b213411f5ul, 0x3c56850e59c37f8ful,
     0xbc52fb761e946603ul, 0x38f68e65a5c94540ul, 0x38f515c8743f3fe2ul},
    {0x3fde2b5d3806f63bul, 0x3fec38b2f180bdb1ul, 0x3c5e0d891d3c6841ul,
     0xbc76e0b1757c8d07ul, 0x38f878ed68aad82aul, 0xb90d3f8010ae0079ul},
    {0x3fdf8ba4dbf89abaul, 0x3febd7c0ac6f952aul, 0xbc32ec1fc1b776b8ul,
     0xbc8825a732ac700aul, 0x38d71a2d56b84136ul, 0x390d7366a512bcb3ul},
    {0x3fe073879922ffeeul, 0x3feb728345196e3eul, 0xbc8a5a014347406cul,
     0xbc8bc69f324e6d61ul, 0x3920157dad78ffcbul, 0xb916f0112635b4d1ul},
    {0x3fe11eb3541b4b23ul, 0x3feb090a58150200ul, 0xbc8ef23b69abe4f1ul,
     0xbc8926da300ffcceul, 0xb91cdecf888dbf4ful, 0xb92516b845a7a95bul},
    {0x3fe1c73b39ae68c8ul, 0x3fea9b66290ea1a3ul, 0x3c8b25dd267f6600ul,
     0x3c39f630e8b6dac8ul, 0xb9256f3106b0516dul, 0x38cf345a348e97cdul},
    {0x3fe26d054cdd12dful, 0x3fea29a7a0462782ul, 0xbc85da743ef3770cul,
     0xbc7128bb015df175ul, 0xb92c7d2376953a04ul, 0xb91041b871e4d097ul},
    {0x3fe30ff7fce17035ul, 0x3fe9b3e047f38741ul, 0xbc6efcc626f74a6ful,
     0xbc830ee286712474ul, 0xb9196d598bf43c65ul, 0xb8d68e6523ac8297ul},
    {0x3fe3affa292050b9ul, 0x3fe93a22499263fbul, 0x3c7e3e25e3954964ul,
     0x3c83d419a920df0bul, 0xb91d4661e2f6dea9ul, 0x3922644a97f89b35ul},
    {0x3fe44cf325091dd6ul, 0x3fe8bc806b151741ul, 0x3c68076a2cfdc6b3ul,
     0xbc82c5e12ed1336dul, 0x39011a6e1c0b805ful, 0x391cc9ab51d0df4eul},
    {0x3fe4e6cabbe3e5e9ul, 0x3fe83b0e0bff976eul, 0x3c63c293edceb327ul,
     0xbc76f420f8ea3475ul, 0xb90cbaeb2aa7f85cul, 0x39005eb6bc2e067eul},
    {0x3fe57d69348ceca0ul, 0x3fe7b5df226aafaful, 0xbc875720992bfbb2ul,
     0xbc70f537acdf0ad7ul, 0x38fa94c2fd0f385aul, 0xb904951b1cc475b3ul},
    {0x3fe610b7551d2cdful, 0x3fe72d0837efff96ul, 0xbc7251b352ff2a37ul,
     0x3c80d4ef0f1d915cul, 0x3912f34699090e37ul, 0x3927e9b6876252feul},
    {0x3fe6a09e667f3bcdul, 0x3fe6a09e667f3bcdul, 0xbc8bdd3413b26456ul,
     0xbc8bdd3413b26456ul, 0x39257d3e3adec175ul, 0x39257d3e3adec175ul},
    {0x3fe72d0837efff96ul, 0x3fe610b7551d2cdful, 0x3c80d4ef0f1d915cul,
     0xbc7251b352ff2a37ul, 0x3927e9b6876252feul, 0x3912f34699090e37ul},
    {0x3fe7b5df226aafaful, 0x3fe57d69348ceca0ul, 0xbc70f537acdf0ad7ul,
     0xbc875720992bfbb2ul, 0xb904951b1cc475b3ul, 0x38fa94c2fd0f385aul},
    {0x3fe83b0e0bff976eul, 0x3fe4e6cabbe3e5e9ul, 0xbc76f420f8ea3475ul,
     0x3c63c293edceb327ul, 0x39005eb6bc2e067eul, 0xb90cbaeb2aa7f85cul},
    {0x3fe8bc806b151741ul, 0x3fe44cf325091dd6ul, 0xbc82c5e12ed1336dul,
     0x3c68076a2cfdc6b3ul, 0x391cc9ab51d0df4eul, 0x39011a6e1c0b805ful},
    {0x3fe93a22499263fbul, 0x3fe3affa292050b9ul, 0x3c83d419a920df0bul,
     0x3c7e3e25e3954964ul, 0x3922644a97f89b35ul, 0xb91d4661e2f6dea9ul},
    {0x3fe9b3e047f38741ul, 0x3fe30ff7fce17035ul, 0xbc830ee286712474ul,
     0xbc6efcc626f74a6ful, 0xb9196d598bf43c65ul, 0xb8d68e6523ac8297ul},
    {0x3fea29a7a0462782ul, 0x3fe26d054cdd12dful, 0xbc7128bb015df175ul,
     0xbc85da743ef3770cul, 0xb91041b871e4d097ul, 0xb92c7d2376953a04ul},
    {0x3fea9b66290ea1a3ul, 0x3fe1c73b39ae68c8ul, 0x3c39f630e8b6dac8ul,
     0x3c8b25dd267f6600ul, 0x38cf345a348e97cdul, 0xb9256f3106b0516dul},
    {0x3feb090a58150200ul, 0x3fe11eb3541b4b23ul, 0xbc8926da300ffcceul,
     0xbc8ef23b69abe4f1ul, 0xb92516b845a7a95bul, 0xb91cdecf888dbf4ful},
    {0x3feb728345196e3eul, 0x3fe073879922ffeeul, 0xbc8bc69f324e6d61ul,
     0xbc8a5a014347406cul, 0xb916f0112635b4d1ul, 0x3920157dad78ffcbul},
    {0x3febd7c0ac6f952aul, 0x3fdf8ba4dbf89abaul, 0xbc8825a732ac700aul,
     0xbc32ec1fc1b776b8ul, 0x390d7366a512bcb3ul, 0x38d71a2d56b84136ul},
    {0x3fec38b2f180bdb1ul, 0x3fde2b5d3806f63bul, 0xbc76e0b1757c8d07ul,
     0x3c5e0d891d3c6841ul, 0xb90d3f8010ae0079ul, 0x38f878ed68aad82aul},
    {0x3fec954b213411f5ul, 0x3fdcc66e9931c45eul, 0xbc52fb761e946603ul,
     0x3c56850e59c37f8ful, 0x38f515c8743f3fe2ul, 0x38f68e65a5c94540ul},
    {0x3feced7af43cc773ul, 0x3fdb5d1009e15cc0ul, 0xbc5e7b6bb5ab58aeul,
     0x3c65b362cb974183ul, 0x38e525e5e3766505ul, 0xb8ecfcff7c31af0cul},
    {0x3fed4134d14dc93aul, 0x3fd9ef7943a8ed8aul, 0xbc84ef5295d25af2ul,
     0x3c66da81290bdbabul, 0xb9242fb98551f41eul, 0xb8f4e8de9013a792ul},
    {0x3fed906bcf328d46ul, 0x3fd87de2a6aea963ul, 0x3c7457e610231ac2ul,
     0xbc672cedd3d5a610ul, 0xb904f3f87abe1619ul, 0xb8f11e4420e0a4b5ul},
    {0x3feddb13b6ccc23cul, 0x3fd7088530fa459ful, 0x3c883c37c6107db3ul,
     0xbc744b19e0864c5dul, 0x3912c06bf13eb37ful, 0x391bc76fbdd51dfdul},
    {0x3fee212104f686e5ul, 0x3fd58f9a75ab1fddul, 0xbc8014c76c126527ul,
     0xbc1efdc0d58cf620ul, 0x3920e62b13b565c2ul, 0xb88f072f54189325ul},
    {0x3fee6288ec48e112ul, 0x3fd4135c94176601ul, 0xbc616b56f2847754ul,
     0x3c70c97c4afa2518ul, 0xb9054aec99b7a418ul, 0x38fe6057b0a0a42ful},
    {0x3fee9f4156c62ddaul, 0x3fd294062ed59f06ul, 0x3c8760b1e2e3f81eul,
     0xbc75d28da2c4612dul, 0xb921ce7542369ecdul, 0x3917eea71c14d05cul},
    {0x3feed740e7684963ul, 0x3fd111d262b1f677ul, 0x3c7e82c791f59cc2ul,
     0x3c7824c20ab7aa9aul, 0xb88eea7cbd5ac167ul, 0xb91779f4232b3b53ul},
    {0x3fef0a7efb9230d7ul, 0x3fcf19f97b215f1bul, 0x3c752c7adc6b4989ul,
     0xbc642deef11da2c4ul, 0xb91db915a9794d33ul, 0xb90b4ce553ffbd03ul},
    {0x3fef38f3ac64e589ul, 0x3fcc0b826a7e4f63ul, 0xbc7d7bafb51f72e6ul,
     0xbc1af1439e521935ul, 0xb91359fe192a6166ul, 0xb8bf0cd3647fe397ul},
    {0x3fef6297cff75cb0ul, 0x3fc8f8b83c69a60bul, 0x3c7562172a361fd3ul,
     0xbc626d19b9ff8d82ul, 0xb9163744e82fc701ul, 0x3909b09f9ca72c69ul},
    {0x3fef8764fa714ba9ul, 0x3fc5e214448b3fc6ul, 0x3c7ab256778ffcb6ul,
     0x3c6531ff779ddac6ul, 0xb90f44b6dc911d8dul, 0xb90c1de6e152ea39ul},
    {0x3fefa7557f08a517ul, 0x3fc2c8106e8e613aul, 0xbc87a0a8ca13571ful,
     0x3c513000a89a11e0ul, 0xb902a212f347e949ul, 0x38ff07f9fe14048cul},
    {0x3fefc26470e19fd3ul, 0x3fbf564e56a9730eul, 0x3c81ec8668ecaceeul,
     0x3c4a2704729ae56dul, 0xb923162266c5450ful, 0x38ee28dc484e8ef5ul},
    {0x3fefd88da3d12526ul, 0x3fb917a6bc29b42cul, 0xbc887df6378811c7ul,
     0xbc3e2718d26ed688ul, 0x391ba7bd68b25db4ul, 0xb8b18edefcf7ef57ul},
    {0x3fefe9cdad01883aul, 0x3fb2d52092ce19f6ul, 0x3c6521ecd0c67e35ul,
     0xbc49a088a8bf6b2cul, 0x390c2c4c8e7c3174ul, 0xb8de51df6b678492ul},
    {0x3feff621e3796d7eul, 0x3fa91f65f10dd814ul, 0xbc6c57bc2e24aa15ul,
     0xbc2912bd0d569a90ul, 0x38f453dcf53e4baaul, 0xb8cd7476f4c4b019ul},
    {0x3feffd886084cd0dul, 0x3f992155f7a3667eul, 0xbc81354d4556e4cbul,
     0xbbfb1d63091a0130ul, 0xb923d19b52e092dbul, 0x3899e58994be786bul},
};

GPGA_CONST gpga_double trigpi_sinpiacc_coeff_1h = 0x400921fb54442d18ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_1m = 0x3ca1a62633145c07ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_1l = 0xb92de56bfc518aa0ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_3h = 0xc014abbce625be53ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_3m = 0x3cb05511c6845b60ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_5h = 0x400466bc6775aae2ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_5m = 0xbc96dc099509acb4ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_7h = 0xbfe32d2cce62bd86ul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_9h = 0x3fb507834881024eul;
GPGA_CONST gpga_double trigpi_sinpiacc_coeff_11h = 0xbf7e307ef392817eul;

GPGA_CONST gpga_double trigpi_cospiacc_coeff_0h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_2h = 0xc013bd3cc9be45deul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_2m = 0xbcb692b71366d2deul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_4h = 0x40103c1f081b5ac4ul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_4m = 0xbcb32b33c9f113a8ul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_6h = 0xbff55d3c7e3cbffaul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_8h = 0x3fce1f506891855aul;
GPGA_CONST gpga_double trigpi_cospiacc_coeff_10h = 0xbf9a6d1b3a75a55ful;

GPGA_CONST gpga_double trigpi_sinpiquick_coeff_1h = 0x400921fb54442d18ul;
GPGA_CONST gpga_double trigpi_sinpiquick_coeff_1m = 0x3ca1a628f488484aul;
GPGA_CONST gpga_double trigpi_sinpiquick_coeff_3h = 0xc014abbce625be53ul;
GPGA_CONST gpga_double trigpi_sinpiquick_coeff_5h = 0x400466bc67767178ul;
GPGA_CONST gpga_double trigpi_sinpiquick_coeff_7h = 0xbfe32d2b83a83690ul;
GPGA_CONST gpga_double trigpi_cospiquick_coeff_0h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double trigpi_cospiquick_coeff_2h = 0xc013bd3cc9be45deul;
GPGA_CONST gpga_double trigpi_cospiquick_coeff_4h = 0x40103c1f0819cac7ul;
GPGA_CONST gpga_double trigpi_cospiquick_coeff_6h = 0xbff55d33e38f046bul;

GPGA_CONST gpga_double trigpi_quick_rncst = 0x3ff0000a7c5ac472ul;
GPGA_CONST gpga_double trigpi_one_minus_ulp = 0x3feffffffffffffful;

inline void gpga_sincospiacc(thread gpga_double* sin_h,
                             thread gpga_double* sin_m,
                             thread gpga_double* sin_l,
                             thread gpga_double* cos_h,
                             thread gpga_double* cos_m,
                             thread gpga_double* cos_l, gpga_double x) {
  gpga_double x2h = gpga_double_zero(0u);
  gpga_double x2m = gpga_double_zero(0u);
  Mul12(&x2h, &x2m, x, x);

  gpga_double sin_t1 = trigpi_sinpiacc_coeff_11h;
  gpga_double sin_t2 = gpga_double_mul(sin_t1, x2h);
  gpga_double sin_t3 = gpga_double_add(trigpi_sinpiacc_coeff_9h, sin_t2);
  gpga_double sin_t4 = gpga_double_mul(sin_t3, x2h);
  gpga_double sin_t5h = gpga_double_zero(0u);
  gpga_double sin_t5m = gpga_double_zero(0u);
  Add12(&sin_t5h, &sin_t5m, trigpi_sinpiacc_coeff_7h, sin_t4);
  gpga_double sin_t6h = gpga_double_zero(0u);
  gpga_double sin_t6m = gpga_double_zero(0u);
  MulAdd22(&sin_t6h, &sin_t6m, trigpi_sinpiacc_coeff_5h,
           trigpi_sinpiacc_coeff_5m, x2h, x2m, sin_t5h, sin_t5m);
  gpga_double sin_t7h = gpga_double_zero(0u);
  gpga_double sin_t7m = gpga_double_zero(0u);
  MulAdd22(&sin_t7h, &sin_t7m, trigpi_sinpiacc_coeff_3h,
           trigpi_sinpiacc_coeff_3m, x2h, x2m, sin_t6h, sin_t6m);
  gpga_double sin_t8h = gpga_double_zero(0u);
  gpga_double sin_t8m = gpga_double_zero(0u);
  Mul22(&sin_t8h, &sin_t8m, sin_t7h, sin_t7m, x2h, x2m);
  gpga_double sin_t9h = gpga_double_zero(0u);
  gpga_double sin_t9m = gpga_double_zero(0u);
  gpga_double sin_t9l = gpga_double_zero(0u);
  Add233Cond(&sin_t9h, &sin_t9m, &sin_t9l, sin_t8h, sin_t8m,
             trigpi_sinpiacc_coeff_1h, trigpi_sinpiacc_coeff_1m,
             trigpi_sinpiacc_coeff_1l);
  gpga_double sin_t10h = gpga_double_zero(0u);
  gpga_double sin_t10m = gpga_double_zero(0u);
  gpga_double sin_t10l = gpga_double_zero(0u);
  Mul133(&sin_t10h, &sin_t10m, &sin_t10l, x, sin_t9h, sin_t9m, sin_t9l);
  Renormalize3(sin_h, sin_m, sin_l, sin_t10h, sin_t10m, sin_t10l);

  gpga_double cos_t1 = trigpi_cospiacc_coeff_10h;
  gpga_double cos_t2 = gpga_double_mul(cos_t1, x2h);
  gpga_double cos_t3 = gpga_double_add(trigpi_cospiacc_coeff_8h, cos_t2);
  gpga_double cos_t4 = gpga_double_mul(cos_t3, x2h);
  gpga_double cos_t5h = gpga_double_zero(0u);
  gpga_double cos_t5m = gpga_double_zero(0u);
  Add12(&cos_t5h, &cos_t5m, trigpi_cospiacc_coeff_6h, cos_t4);
  gpga_double cos_t6h = gpga_double_zero(0u);
  gpga_double cos_t6m = gpga_double_zero(0u);
  MulAdd22(&cos_t6h, &cos_t6m, trigpi_cospiacc_coeff_4h,
           trigpi_cospiacc_coeff_4m, x2h, x2m, cos_t5h, cos_t5m);
  gpga_double cos_t7h = gpga_double_zero(0u);
  gpga_double cos_t7m = gpga_double_zero(0u);
  MulAdd22(&cos_t7h, &cos_t7m, trigpi_cospiacc_coeff_2h,
           trigpi_cospiacc_coeff_2m, x2h, x2m, cos_t6h, cos_t6m);
  gpga_double cos_t8h = gpga_double_zero(0u);
  gpga_double cos_t8m = gpga_double_zero(0u);
  Mul22(&cos_t8h, &cos_t8m, cos_t7h, cos_t7m, x2h, x2m);
  gpga_double cos_t9h = gpga_double_zero(0u);
  gpga_double cos_t9m = gpga_double_zero(0u);
  gpga_double cos_t9l = gpga_double_zero(0u);
  Add123(&cos_t9h, &cos_t9m, &cos_t9l, trigpi_cospiacc_coeff_0h, cos_t8h,
         cos_t8m);
  *cos_h = cos_t9h;
  *cos_m = cos_t9m;
  *cos_l = cos_t9l;
}

inline void gpga_sinpi_accurate(thread gpga_double* rh, thread gpga_double* rm,
                                thread gpga_double* rl, gpga_double y,
                                int index, int quadrant) {
  gpga_double syh = gpga_double_zero(0u);
  gpga_double sym = gpga_double_zero(0u);
  gpga_double syl = gpga_double_zero(0u);
  gpga_double cyh = gpga_double_zero(0u);
  gpga_double cym = gpga_double_zero(0u);
  gpga_double cyl = gpga_double_zero(0u);
  gpga_sincospiacc(&syh, &sym, &syl, &cyh, &cym, &cyl, y);

  GpgaTrigpiEntry entry = gpga_trigpi_sincos_table[index];
  gpga_double sah = entry.sh;
  gpga_double cah = entry.ch;
  gpga_double sam = entry.sm;
  gpga_double cam = entry.cm;
  gpga_double sal = entry.sl;
  gpga_double cal = entry.cl;

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (quadrant == 0 || quadrant == 2) {
    Mul33(&t1h, &t1m, &t1l, syh, sym, syl, cah, cam, cal);
    Mul33(&t2h, &t2m, &t2l, sah, sam, sal, cyh, cym, cyl);
    Add33Cond(rh, rm, rl, t2h, t2m, t2l, t1h, t1m, t1l);
  } else {
    Mul33(&t1h, &t1m, &t1l, cyh, cym, cyl, cah, cam, cal);
    Mul33(&t2h, &t2m, &t2l, sah, sam, sal, syh, sym, syl);
    Add33Cond(rh, rm, rl, t1h, t1m, t1l, gpga_double_neg(t2h),
              gpga_double_neg(t2m), gpga_double_neg(t2l));
  }

  if (quadrant >= 2) {
    *rh = gpga_double_neg(*rh);
    *rm = gpga_double_neg(*rm);
    *rl = gpga_double_neg(*rl);
  }
}

inline void gpga_cospi_accurate(thread gpga_double* rh, thread gpga_double* rm,
                                thread gpga_double* rl, gpga_double y,
                                int index, int quadrant) {
  gpga_double syh = gpga_double_zero(0u);
  gpga_double sym = gpga_double_zero(0u);
  gpga_double syl = gpga_double_zero(0u);
  gpga_double cyh = gpga_double_zero(0u);
  gpga_double cym = gpga_double_zero(0u);
  gpga_double cyl = gpga_double_zero(0u);
  gpga_sincospiacc(&syh, &sym, &syl, &cyh, &cym, &cyl, y);

  GpgaTrigpiEntry entry = gpga_trigpi_sincos_table[index];
  gpga_double sah = entry.sh;
  gpga_double cah = entry.ch;
  gpga_double sam = entry.sm;
  gpga_double cam = entry.cm;
  gpga_double sal = entry.sl;
  gpga_double cal = entry.cl;

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (quadrant == 0 || quadrant == 2) {
    Mul33(&t1h, &t1m, &t1l, cyh, cym, cyl, cah, cam, cal);
    Mul33(&t2h, &t2m, &t2l, sah, sam, sal, syh, sym, syl);
    Add33Cond(rh, rm, rl, t1h, t1m, t1l, gpga_double_neg(t2h),
              gpga_double_neg(t2m), gpga_double_neg(t2l));
  } else {
    Mul33(&t1h, &t1m, &t1l, syh, sym, syl, cah, cam, cal);
    Mul33(&t2h, &t2m, &t2l, sah, sam, sal, cyh, cym, cyl);
    Add33Cond(rh, rm, rl, t2h, t2m, t2l, t1h, t1m, t1l);
  }

  if (quadrant == 1 || quadrant == 2) {
    *rh = gpga_double_neg(*rh);
    *rm = gpga_double_neg(*rm);
    *rl = gpga_double_neg(*rl);
  }
}

inline void gpga_sinpiquick(thread gpga_double* rh, thread gpga_double* rm,
                            gpga_double x, int index, int quadrant) {
  gpga_double x2h = gpga_double_zero(0u);
  gpga_double x2m = gpga_double_zero(0u);
  Mul12(&x2h, &x2m, x, x);

  GpgaTrigpiEntry entry = gpga_trigpi_sincos_table[index];
  gpga_double sah = entry.sh;
  gpga_double cah = entry.ch;
  gpga_double sam = entry.sm;
  gpga_double cam = entry.cm;

  gpga_double sin_t1 = trigpi_sinpiquick_coeff_7h;
  gpga_double sin_t2 = gpga_double_mul(sin_t1, x2h);
  gpga_double sin_t3 = gpga_double_add(trigpi_sinpiquick_coeff_5h, sin_t2);
  gpga_double sin_t4 = gpga_double_mul(sin_t3, x2h);
  gpga_double sin_t5h = gpga_double_zero(0u);
  gpga_double sin_t5m = gpga_double_zero(0u);
  Add12(&sin_t5h, &sin_t5m, trigpi_sinpiquick_coeff_3h, sin_t4);
  gpga_double sin_t6h = gpga_double_zero(0u);
  gpga_double sin_t6m = gpga_double_zero(0u);
  MulAdd22(&sin_t6h, &sin_t6m, trigpi_sinpiquick_coeff_1h,
           trigpi_sinpiquick_coeff_1m, x2h, x2m, sin_t5h, sin_t5m);
  gpga_double sin_t7h = gpga_double_zero(0u);
  gpga_double sin_t7m = gpga_double_zero(0u);
  Mul122(&sin_t7h, &sin_t7m, x, sin_t6h, sin_t6m);
  gpga_double syh = sin_t7h;
  gpga_double sym = sin_t7m;

  gpga_double cos_t1 = trigpi_cospiquick_coeff_6h;
  gpga_double cos_t2 = gpga_double_mul(cos_t1, x2h);
  gpga_double cos_t3 = gpga_double_add(trigpi_cospiquick_coeff_4h, cos_t2);
  gpga_double cos_t4 = gpga_double_mul(cos_t3, x2h);
  gpga_double cos_t5h = gpga_double_zero(0u);
  gpga_double cos_t5m = gpga_double_zero(0u);
  Add12(&cos_t5h, &cos_t5m, trigpi_cospiquick_coeff_2h, cos_t4);
  gpga_double cos_t6h = gpga_double_zero(0u);
  gpga_double cos_t6m = gpga_double_zero(0u);
  Mul22(&cos_t6h, &cos_t6m, cos_t5h, cos_t5m, x2h, x2m);
  gpga_double cos_t7h = gpga_double_zero(0u);
  gpga_double cos_t7m = gpga_double_zero(0u);
  Add122(&cos_t7h, &cos_t7m, trigpi_cospiquick_coeff_0h, cos_t6h, cos_t6m);
  gpga_double cyh = cos_t7h;
  gpga_double cym = cos_t7m;

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);

  if (quadrant == 0 || quadrant == 2) {
    Mul22(&t1h, &t1m, syh, sym, cah, cam);
    Mul22(&t2h, &t2m, sah, sam, cyh, cym);
    Add22Cond(rh, rm, t2h, t2m, t1h, t1m);
  } else {
    Mul22(&t1h, &t1m, cyh, cym, cah, cam);
    Mul22(&t2h, &t2m, sah, sam, syh, sym);
    Add22Cond(rh, rm, t1h, t1m, gpga_double_neg(t2h),
              gpga_double_neg(t2m));
  }

  if (quadrant >= 2) {
    *rh = gpga_double_neg(*rh);
    *rm = gpga_double_neg(*rm);
  }
}

inline gpga_double gpga_sinpi_rn(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double check =
        gpga_double_add(rh, gpga_double_mul(rl, trigpi_pix_rncst_sin));
    if (gpga_double_eq(rh, check)) {
      return rh;
    }
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_sinpiquick(&rh, &rm, y, (int)index, (int)quadrant);
  gpga_double quick_check =
      gpga_double_add(rh, gpga_double_mul(rm, trigpi_quick_rncst));
  if (gpga_double_eq(rh, quick_check)) {
    return rh;
  }
  gpga_sinpi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundToNearest3(rh, rm, rl);
}

inline gpga_double gpga_sinpi_rd(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_minf(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(rh, rl, trigpi_pix_eps_sin, &quick)) {
      return quick;
    }
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundDownwards3(rh, rm, rl);
}

inline gpga_double gpga_sinpi_ru(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_pinf(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(rh, rl, trigpi_pix_eps_sin, &quick)) {
      return quick;
    }
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundUpwards3(rh, rm, rl);
}

inline gpga_double gpga_sinpi_rz(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_zero(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(rh, rl, trigpi_pix_eps_sin, &quick)) {
      return quick;
    }
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundTowardsZero3(rh, rm, rl);
}

inline gpga_double gpga_cospi_rn(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  y = gpga_double_mul(y, trigpi_inv128);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43400000u) {
    return gpga_double_from_u32(1u);
  }

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 1u)) {
    return gpga_double_zero(0u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 0u) {
    return gpga_double_from_u32(1u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 2u) {
    return gpga_double_from_s32(-1);
  }

  if (absxhi < 0x3e26a09eu) {
    return gpga_double_from_u32(1u);
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_cospi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundToNearest3(rh, rm, rl);
}

inline gpga_double gpga_cospi_rd(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  y = gpga_double_mul(y, trigpi_inv128);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43400000u) {
    return gpga_double_from_u32(1u);
  }

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 1u)) {
    return gpga_double_zero(1u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 0u) {
    return gpga_double_from_u32(1u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 2u) {
    return gpga_double_from_s32(-1);
  }

  if (absxhi < 0x3e200000u) {
    return trigpi_one_minus_ulp;
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_cospi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundDownwards3(rh, rm, rl);
}

inline gpga_double gpga_cospi_ru(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  y = gpga_double_mul(y, trigpi_inv128);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43400000u) {
    return gpga_double_from_u32(1u);
  }

  if (index == 0u && gpga_double_is_zero(y) && quadrant == 0u) {
    return gpga_double_from_u32(1u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 2u) {
    return gpga_double_from_s32(-1);
  }
  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 1u)) {
    return gpga_double_zero(0u);
  }

  if (absxhi < 0x3e200000u) {
    return gpga_double_from_u32(1u);
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_cospi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundUpwards3(rh, rm, rl);
}

inline gpga_double gpga_cospi_rz(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  y = gpga_double_mul(y, trigpi_inv128);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43400000u) {
    return gpga_double_from_u32(1u);
  }

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 1u)) {
    return gpga_double_zero(0u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 0u) {
    return gpga_double_from_u32(1u);
  }
  if (index == 0u && gpga_double_is_zero(y) && quadrant == 2u) {
    return gpga_double_from_s32(-1);
  }

  if (absxhi < 0x3e200000u) {
    return trigpi_one_minus_ulp;
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_cospi_accurate(&rh, &rm, &rl, y, (int)index, (int)quadrant);
  return ReturnRoundTowardsZero3(rh, rm, rl);
}

inline gpga_double gpga_tanpi_rn(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double check =
        gpga_double_add(rh, gpga_double_mul(rl, trigpi_pix_rncst_tan));
    if (gpga_double_eq(rh, check)) {
      return rh;
    }
  }

  gpga_double ch = gpga_double_zero(0u);
  gpga_double cm = gpga_double_zero(0u);
  gpga_double cl = gpga_double_zero(0u);
  gpga_cospi_accurate(&ch, &cm, &cl, y, (int)index, (int)quadrant);
  gpga_double ich = gpga_double_zero(0u);
  gpga_double icm = gpga_double_zero(0u);
  gpga_double icl = gpga_double_zero(0u);
  Recpr33(&ich, &icm, &icl, ch, cm, cl);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sm = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&sh, &sm, &sl, y, (int)index, (int)quadrant);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Mul33(&rh, &rm, &rl, sh, sm, sl, ich, icm, icl);
  return ReturnRoundToNearest3(rh, rm, rl);
}

inline gpga_double gpga_tanpi_rd(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_minf(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(rh, rl, trigpi_pix_eps_sin, &quick)) {
      return quick;
    }
  }

  gpga_double ch = gpga_double_zero(0u);
  gpga_double cm = gpga_double_zero(0u);
  gpga_double cl = gpga_double_zero(0u);
  gpga_cospi_accurate(&ch, &cm, &cl, y, (int)index, (int)quadrant);
  gpga_double ich = gpga_double_zero(0u);
  gpga_double icm = gpga_double_zero(0u);
  gpga_double icl = gpga_double_zero(0u);
  Recpr33(&ich, &icm, &icl, ch, cm, cl);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sm = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&sh, &sm, &sl, y, (int)index, (int)quadrant);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Mul33(&rh, &rm, &rl, sh, sm, sl, ich, icm, icl);
  return ReturnRoundDownwards3(rh, rm, rl);
}

inline gpga_double gpga_tanpi_ru(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_pinf(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(rh, rl, trigpi_pix_eps_tan, &quick)) {
      return quick;
    }
  }

  gpga_double ch = gpga_double_zero(0u);
  gpga_double cm = gpga_double_zero(0u);
  gpga_double cl = gpga_double_zero(0u);
  gpga_cospi_accurate(&ch, &cm, &cl, y, (int)index, (int)quadrant);
  gpga_double ich = gpga_double_zero(0u);
  gpga_double icm = gpga_double_zero(0u);
  gpga_double icl = gpga_double_zero(0u);
  Recpr33(&ich, &icm, &icl, ch, cm, cl);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sm = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&sh, &sm, &sl, y, (int)index, (int)quadrant);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Mul33(&rh, &rm, &rl, sh, sm, sl, ich, icm, icl);
  return ReturnRoundUpwards3(rh, rm, rl);
}

inline gpga_double gpga_tanpi_rz(gpga_double x) {
  gpga_double absx = gpga_double_abs(x);
  gpga_double xs = gpga_double_mul(x, gpga_double_from_u32(128u));
  if (gpga_double_gt(absx, trigpi_twoto42)) {
    gpga_double tmask = xs & 0xffffffff00000000ULL;
    xs = gpga_double_sub(xs, tmask);
  }
  gpga_double t = gpga_double_add(trigpi_twoto5251, xs);
  gpga_double u = gpga_double_sub(t, trigpi_twoto5251);
  gpga_double y = gpga_double_sub(xs, u);
  uint index = gpga_u64_lo(t) & 0x3fu;
  uint quadrant = (gpga_u64_lo(t) & 0xffu) >> 6;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint sign = gpga_double_sign(x);

  if (index == 0u && gpga_double_is_zero(y) && ((quadrant & 1u) == 0u)) {
    return gpga_double_zero(sign);
  }

  y = gpga_double_mul(y, trigpi_inv128);

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }
  if (absxhi >= 0x43300000u) {
    return gpga_double_zero(sign);
  }

  if (absxhi <= 0x3e000000u) {
    if (absxhi < 0x01700000u) {
      scs_t result;
      scs_set_d(result, x);
      scs_mul(result, PiSCS_ptr, result);
      gpga_double rh = gpga_double_zero(0u);
      scs_get_d_zero(&rh, result);
      return rh;
    }
    gpga_double dekker = gpga_double_from_u32(134217729u);
    gpga_double tt = gpga_double_mul(x, dekker);
    gpga_double xh = gpga_double_add(gpga_double_sub(x, tt), tt);
    gpga_double xl = gpga_double_sub(x, xh);
    gpga_double rh = gpga_double_zero(0u);
    gpga_double rl = gpga_double_zero(0u);
    gpga_double term1 = gpga_double_mul(xh, trigpi_pihh);
    gpga_double term2 = gpga_double_add(gpga_double_mul(xl, trigpi_pihh),
                                        gpga_double_mul(xh, trigpi_pihm));
    gpga_double term3 = gpga_double_add(gpga_double_mul(xh, trigpi_pim),
                                        gpga_double_mul(xl, trigpi_pihm));
    Add12(&rh, &rl, term1, gpga_double_add(term2, term3));
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(rh, rl, trigpi_pix_eps_sin, &quick)) {
      return quick;
    }
  }

  gpga_double ch = gpga_double_zero(0u);
  gpga_double cm = gpga_double_zero(0u);
  gpga_double cl = gpga_double_zero(0u);
  gpga_cospi_accurate(&ch, &cm, &cl, y, (int)index, (int)quadrant);
  gpga_double ich = gpga_double_zero(0u);
  gpga_double icm = gpga_double_zero(0u);
  gpga_double icl = gpga_double_zero(0u);
  Recpr33(&ich, &icm, &icl, ch, cm, cl);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sm = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  gpga_sinpi_accurate(&sh, &sm, &sl, y, (int)index, (int)quadrant);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Mul33(&rh, &rm, &rl, sh, sm, sl, ich, icm, icl);
  return ReturnRoundTowardsZero3(rh, rm, rl);
}


// CRLIBM_ASINCOS_CONSTANTS
GPGA_CONST gpga_double RNROUNDCST = 0x3ff00b5baade1dbcul;
GPGA_CONST gpga_double RDROUNDCST = 0x3c06a09e667f3bcdul;
GPGA_CONST gpga_double RNROUNDCSTASINPI = 0x3ff000000406cacaul;
GPGA_CONST gpga_double RDROUNDCSTASINPI = 0x3af0000000000000ul;
GPGA_CONST uint ASINSIMPLEBOUND = 0x3e300000u;
GPGA_CONST uint ACOSSIMPLEBOUND = 0x3e400000u;
GPGA_CONST uint ASINPISIMPLEBOUND = 0x3c300000u;
GPGA_CONST uint ACOSPISIMPLEBOUND = 0x3c900000u;
GPGA_CONST uint ASINPINOSUBNORMALBOUND = 0xa5000000u;
GPGA_CONST uint EXTRABOUND = 0x3f500000u;
GPGA_CONST uint EXTRABOUND2 = 0x3f020000u;
GPGA_CONST gpga_double PIHALFH = 0x3ff921fb54442d18ul;
GPGA_CONST gpga_double PIHALFM = 0x3c91a62633145c07ul;
GPGA_CONST gpga_double PIHALFL = 0xb91f1976b7ed8fbcul;
GPGA_CONST gpga_double PIHALFRU = 0x3ff921fb54442d19ul;
GPGA_CONST gpga_double PIRU = 0x400921fb54442d19ul;
GPGA_CONST gpga_double PIH = 0x400921fb54442d18ul;
GPGA_CONST gpga_double PIM = 0x3ca1a62633145c07ul;
GPGA_CONST gpga_double PIL = 0xb92f1976b7ed8fbcul;
GPGA_CONST gpga_double RECPRPIH = 0x3fd45f306dc9c883ul;
GPGA_CONST gpga_double RECPRPIM = 0xbc76b01ec5417056ul;
GPGA_CONST gpga_double RECPRPIL = 0xb916447e493ad4ceul;
GPGA_CONST gpga_double MRECPRPIH = 0xbfd45f306dc9c883ul;
GPGA_CONST gpga_double MRECPRPIM = 0x3c76b01ec5417056ul;
GPGA_CONST gpga_double MRECPRPIL = 0x3916447e493ad4ceul;
GPGA_CONST gpga_double HALFPLUSULP = 0x3fe0000000000001ul;
GPGA_CONST gpga_double HALFMINUSHALFULP = 0x3fdffffffffffffful;
GPGA_CONST gpga_double TWO1000 = 0x7e70000000000000ul;
GPGA_CONST gpga_double TWOM1000 = 0x0170000000000000ul;
GPGA_CONST gpga_double TWO999 = 0x7e60000000000000ul;
GPGA_CONST gpga_double ASINBADCASEX = 0x3fde9950730c4696ul;
GPGA_CONST gpga_double ASINBADCASEYRU = 0x3fdfe767739d0f6eul;
GPGA_CONST gpga_double ASINBADCASEYRD = 0x3fdfe767739d0f6dul;
GPGA_CONST gpga_double p0_quick_coeff_19h = 0x3f8a4b92dae969edul;
GPGA_CONST gpga_double p0_quick_coeff_17h = 0x3f86c7aa165208f9ul;
GPGA_CONST gpga_double p0_quick_coeff_15h = 0x3f8caa781489e2b8ul;
GPGA_CONST gpga_double p0_quick_coeff_13h = 0x3f91c48b99d291deul;
GPGA_CONST gpga_double p0_quick_coeff_11h = 0x3f96e8bcd6ff71c4ul;
GPGA_CONST gpga_double p0_quick_coeff_9h = 0x3f9f1c71bbd33c20ul;
GPGA_CONST gpga_double p0_quick_coeff_7h = 0x3fa6db6db6e9018dul;
GPGA_CONST gpga_double p0_quick_coeff_5h = 0x3fb3333333332b26ul;
GPGA_CONST gpga_double p0_quick_coeff_3h = 0x3fc5555555555557ul;
GPGA_CONST gpga_double p0_quick_coeff_1h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double MI_9 = 0x3fec000000001b87ul;
GPGA_CONST gpga_double p9_quick_coeff_0h = 0x3ff02be9ce0b8696ul;
GPGA_CONST gpga_double p9_quick_coeff_1h = 0xbfb69ab5325bba15ul;
GPGA_CONST gpga_double p9_quick_coeff_2h = 0x3f958a4c3097993aul;
GPGA_CONST gpga_double p9_quick_coeff_3h = 0xbf7b3db36068b22cul;
GPGA_CONST gpga_double p9_quick_coeff_4h = 0x3f63b9482181094eul;
GPGA_CONST gpga_double p9_quick_coeff_5h = 0xbf4eedc823c6567ful;
GPGA_CONST gpga_double p9_quick_coeff_6h = 0x3f398e361865b1daul;
GPGA_CONST gpga_double p9_quick_coeff_7h = 0xbf25ea4eb35ec8feul;
GPGA_CONST gpga_double p9_quick_coeff_8h = 0x3f135231ff7355f4ul;
GPGA_CONST gpga_double p9_quick_coeff_9h = 0xbf01681507f3d4a9ul;
GPGA_CONST gpga_double p9_quick_coeff_10h = 0x3ef01cad0aca1cc8ul;
GPGA_CONST gpga_double p9_quick_coeff_11h = 0xbedd8b99c74259f7ul;
GPGA_CONST gpga_double p0_accu_coeff_1h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double p0_accu_coeff_3h = 0x3fc5555555555555ul;
GPGA_CONST gpga_double p0_accu_coeff_3m = 0x3c65555555555553ul;
GPGA_CONST gpga_double p0_accu_coeff_5h = 0x3fb3333333333333ul;
GPGA_CONST gpga_double p0_accu_coeff_5m = 0x3c4999999999ee40ul;
GPGA_CONST gpga_double p0_accu_coeff_7h = 0x3fa6db6db6db6db7ul;
GPGA_CONST gpga_double p0_accu_coeff_7m = 0xbc324924946fc466ul;
GPGA_CONST gpga_double p0_accu_coeff_9h = 0x3f9f1c71c71c71c7ul;
GPGA_CONST gpga_double p0_accu_coeff_9m = 0x3c1c71d56e6e2658ul;
GPGA_CONST gpga_double p0_accu_coeff_11h = 0x3f96e8ba2e8ba2e9ul;
GPGA_CONST gpga_double p0_accu_coeff_11m = 0xbc3177f0b6a02ad8ul;
GPGA_CONST gpga_double p0_accu_coeff_13h = 0x3f91c4ec4ec4ec4ful;
GPGA_CONST gpga_double p0_accu_coeff_13m = 0xbc28d703ddf5c0eeul;
GPGA_CONST gpga_double p0_accu_coeff_15h = 0x3f8c999999999991ul;
GPGA_CONST gpga_double p0_accu_coeff_17h = 0x3f87a87878787b52ul;
GPGA_CONST gpga_double p0_accu_coeff_19h = 0x3f83fde50d788fa8ul;
GPGA_CONST gpga_double p0_accu_coeff_21h = 0x3f812ef3cf5e6feeul;
GPGA_CONST gpga_double p0_accu_coeff_23h = 0x3f7df3bd2e1e27c2ul;
GPGA_CONST gpga_double p0_accu_coeff_25h = 0x3f7a6864e1905aeeul;
GPGA_CONST gpga_double p0_accu_coeff_27h = 0x3f7782c75df2256eul;
GPGA_CONST gpga_double p0_accu_coeff_29h = 0x3f751d0b829517ccul;
GPGA_CONST gpga_double p0_accu_coeff_31h = 0x3f7305990a9ad96cul;
GPGA_CONST gpga_double p0_accu_coeff_33h = 0x3f71f051de5bc8f4ul;
GPGA_CONST gpga_double p0_accu_coeff_35h = 0x3f6941e71c6b15aful;
GPGA_CONST gpga_double p0_accu_coeff_37h = 0x3f798ced3dfaf58aul;
GPGA_CONST gpga_double p9_accu_coeff_0h = 0x3ff02be9ce0b8696ul;
GPGA_CONST gpga_double p9_accu_coeff_0m = 0x3bd06cf1ba8caf60ul;
GPGA_CONST gpga_double p9_accu_coeff_1h = 0xbfb69ab5325bba15ul;
GPGA_CONST gpga_double p9_accu_coeff_1m = 0x3c464d2b56116c2aul;
GPGA_CONST gpga_double p9_accu_coeff_2h = 0x3f958a4c3097991eul;
GPGA_CONST gpga_double p9_accu_coeff_2m = 0x3c33fc0eca2ce284ul;
GPGA_CONST gpga_double p9_accu_coeff_3h = 0xbf7b3db36068bb92ul;
GPGA_CONST gpga_double p9_accu_coeff_3m = 0xbc1817caa85a093eul;
GPGA_CONST gpga_double p9_accu_coeff_4h = 0x3f63b9482183be1cul;
GPGA_CONST gpga_double p9_accu_coeff_4m = 0xbc0d1c776f95416bul;
GPGA_CONST gpga_double p9_accu_coeff_5h = 0xbf4eedc82374e2ecul;
GPGA_CONST gpga_double p9_accu_coeff_5m = 0xbbc7c28f8ed03cb8ul;
GPGA_CONST gpga_double p9_accu_coeff_6h = 0x3f398e36009d94c6ul;
GPGA_CONST gpga_double p9_accu_coeff_6m = 0xbb9fde9c3b0f5980ul;
GPGA_CONST gpga_double p9_accu_coeff_7h = 0xbf25ea4f480d9f95ul;
GPGA_CONST gpga_double p9_accu_coeff_7m = 0x3bcc488ea1a7b842ul;
GPGA_CONST gpga_double p9_accu_coeff_8h = 0x3f135260961ad1dcul;
GPGA_CONST gpga_double p9_accu_coeff_9h = 0xbf0167a6f81b3715ul;
GPGA_CONST gpga_double p9_accu_coeff_10h = 0x3eefe5d222269587ul;
GPGA_CONST gpga_double p9_accu_coeff_11h = 0xbedda3f6c87e1372ul;
GPGA_CONST gpga_double p9_accu_coeff_12h = 0x3ecbdd61f82c378cul;
GPGA_CONST gpga_double p9_accu_coeff_13h = 0xbeba7427808ac3beul;
GPGA_CONST gpga_double p9_accu_coeff_14h = 0x3ea9536e8db047ebul;
GPGA_CONST gpga_double p9_accu_coeff_15h = 0xbe986c4cdc80ebd3ul;
GPGA_CONST gpga_double p9_accu_coeff_16h = 0x3e87b3d309e2ad0bul;
GPGA_CONST gpga_double p9_accu_coeff_17h = 0xbe7720d2fc58438aul;
GPGA_CONST gpga_double p9_accu_coeff_18h = 0x3e66af52d4c5c5cbul;
GPGA_CONST gpga_double p9_accu_coeff_19h = 0xbe56cece65f20c88ul;
GPGA_CONST gpga_double p9_accu_coeff_20h = 0x3e46893e81151731ul;

GPGA_CONST gpga_double gpga_asin_poly_quick[128] = {
    0x3fd3504f333f9e0aul, 0x3fd39e9bbade6b79ul, 0x3c04585d01c53454ul, 0x3ff0c84ca0d3b637ul,
    0xbc9df9a90b236e11ul, 0x3fc649b4bb6046ecul, 0x3fd0025bb2b6e4daul, 0x3fc5763aba8a7efcul,
    0x3fc9d3e7dd28aa5aul, 0x3fc974b48a3e278ful, 0x3fce31fa2d8288aful, 0x3fd125c497bf4c4cul,
    0x3fd4ca9b46c4e92aul, 0x3fd926a55b412e00ul, 0x3fdfb48d5ff5a09bul, 0x3fe3c33a3da06de1ul,
    0x3fd92b8ca76bc49bul, 0x3fd9de6847a96ffbul, 0xbbc1390a635af5b1ul, 0x3ff166feae7767e7ul,
    0x3c94b1ecf53b8c9eul, 0x3fd0314f3026f22aul, 0x3fd541ffb60cc9e3ul, 0x3fd2bf68c34466aaul,
    0x3fd78706fbb7e0f7ul, 0x3fdc3f4ab6db78ebul, 0x3fe29bf12c04e023ul, 0x3fe8cf43c2e30416ul,
    0x3ff11be33c24e743ul, 0x3ff7f8569c1c8b3eul, 0x40014481661100a4ul, 0x40088de63befe1d1ul,
    0x3fdddb3d742c25abul, 0x3fdf0fc25d05490ful, 0x3c46eff8d1e71faaul, 0x3ff216c57db79f68ul,
    0x3c97fa39f98eacecul, 0x3fd59265e1e68e58ul, 0x3fdc46c6c1d5f04bul, 0x3fde443188963258ul,
    0x3fe4c3cca91f5737ul, 0x3fece36adde60c19ul, 0x3ff56ee956243c84ul, 0x40005168eb2c1282ul,
    0x400989a57ef4f055ul, 0x40145c3d209770d1ul, 0x4020a8736cff612ful, 0x402b2311886b9cfbul,
    0x3fe0f1bbcdcbfb6bul, 0x3fe1db4efec353dcul, 0x3bfaeb231477d7b0ul, 0x3ff2dc7440f68902ul,
    0x3c46684c2e35ef32ul, 0x3fdbc1b542b73ed6ul, 0x3fe2f2d20cfa6afeul, 0x3fe7db589d1234aeul,
    0x3ff236660b4542e6ul, 0x3ffcf12c7922f800ul, 0x400843d0056e2d07ul, 0x4014f87082f39d33ul,
    0x4022984c8aa1c14aul, 0x4030cfc0cc418ad9ul, 0x403f3004534cd6c4ul, 0x404cd4c1445848c6ul,
    0x3fe2be02d7d08921ul, 0x3fe4060965b082c3ul, 0x3bc9eb453c402a88ul, 0x3ff3bd790165b0eful,
    0xbc72590012460dc3ul, 0x3fe199460df37244ul, 0x3fe9b49b09d18d99ul, 0x3ff2c9abf973ee12ul,
    0x4000206d04afa0d2ul, 0x400d312706a4457aul, 0x401bc111e7b5264dul, 0x402b3ecfd749de41ul,
    0x403b6d98c804f4d9ul, 0x404c2707c64d29f8ul, 0x405dacd2996fd0d3ul, 0x406fe26f78f7048cul,
    0x3fe46186deee0c8cul, 0x3fe6186398850c4dul, 0x3c4f4f7f4235a128ul, 0x3ff4c0fabe41c88cul,
    0xbc8c936bd705a984ul, 0x3fe63d5e3c64ee74ul, 0x3ff1bc5917e77f8bul, 0x3ffdfe4b4faf594dul,
    0x400d366e88a3f638ul, 0x401e34e572f1a140ul, 0x40305fa0126ec5fful, 0x4042570729ebbe4eul,
    0x405510464e198fb5ul, 0x4068abcb817a3240ul, 0x407db2a61acb5e30ul, 0x4091eab94d890683ul,
    0x3fe5e58f08291918ul, 0x3fe81d88ed215a4bul, 0xbbf41c8167325436ul, 0x3ff5f0e8eca0a04dul,
    0x3c90969b85c59c2aul, 0x3fec3bb8ca0fc932ul, 0x3ff90aae2f36a8fdul, 0x4008905ab488633dul,
    0x401b6c6e338ecedaul, 0x40305001af5e8cdbul, 0x4044546714e2aed0ul, 0x405a2f564f392152ul,
    0x40714a5498a97bc3ul, 0x408749794e3b6fe4ul, 0x40a02545e1bc9fa1ul, 0x40b67970de1f16a3ul,
    0x3fe7504f333f9cdeul, 0x3fea1e5f9c00704bul, 0x3bcaeacd5a47b9f0ul, 0x3ff75ba86bdd39a2ul,
    0x3c9cfde24d6b630dul, 0x3ff22260c989df64ul, 0x40023a2c5c9f70baul, 0x4014e8cd1470b313ul,
    0x402b18c740df8df1ul, 0x4042bfa0dccb59b5ul, 0x405b2b87f4bcf3f2ul, 0x4074597eb5ed5a13ul,
    0x408f411591fccdf0ul, 0x40a87a9aac5cde95ul, 0x40c3ca3f7574c7f5ul, 0x40dff8d69f2bfc18ul,
};

GPGA_CONST gpga_double gpga_asin_poly_accu[256] = {
    0x3fd39e9bbade6b79ul, 0x3c046c8e980714a1ul, 0x3ff0c84ca0d3b637ul, 0xbc9db627c6ba1cccul,
    0x3fc649b4bb6046ecul, 0xbc6c3a235f1337deul, 0x3fd0025bb2b6e42dul, 0xbc530da0bb1270eeul,
    0x3fc5763aba8a8539ul, 0x3c64f2ab0b823883ul, 0x3fc9d3e7dd38782cul, 0xbc49f2aa049ffc24ul,
    0x3fc974b48a21acb7ul, 0x3c565486d2a39a1cul, 0x3fce31f9de75a68eul, 0xbc61c2d881bd2398ul,
    0x3fd125c4a8d997eaul, 0xbc741592c1489cf7ul, 0x3fd4cafb43b84312ul, 0x3fd926c92dccc115ul,
    0x3fdf44de3ae25162ul, 0x3fe390cdd1eaf796ul, 0x3fe8d5b4efb3e5bcul, 0x3fefc16f9f335908ul,
    0x3ff47bb4ff13fbc2ul, 0x3ffa97a326cd67aful, 0x40015eeb297d7dc1ul, 0x4006cfc4b64a8157ul,
    0x400e16cbc1a2e219ul, 0x4013eecca844f868ul, 0x401b57ba4343c98ful, 0x40225a79f662d7f3ul,
    0x3fd9de6847a96ffbul, 0xbbbe8ba38f61b5d5ul, 0x3ff166feae7767e7ul, 0x3c94c190f5dd7c05ul,
    0x3fd0314f3026f22aul, 0xbc7e4e201eb5d0b4ul, 0x3fd541ffb60cc99eul, 0xbc7908e1ebf842eful,
    0x3fd2bf68c34471aeul, 0xbc7d57b4fdfc6227ul, 0x3fd78706fbbd4dc6ul, 0xbc5f3842ce74357ful,
    0x3fdc3f4ab679183bul, 0xbc4aced383ed27e2ul, 0x3fe29bf114de14d1ul, 0x3c78d89e72191c31ul,
    0x3fe8cf44892fada2ul, 0xbc8d65fb96fc66b8ul, 0x3ff11c1321ccf199ul, 0x3ff7f7add7862e82ul,
    0x4001150e724c320bul, 0x4008a5cdbd8648aeul, 0x4011fb24d057f6cbul, 0x401a792b1d54f62bul,
    0x4023a4d91933c6cbul, 0x402d5a47d121fe75ul, 0x4036100393f66399ul, 0x4040abe2d257304bul,
    0x4049509e3a68c60aul, 0x40534f1c55aaa1feul, 0x405de6bde6d6bd51ul, 0x406685d4508bf1d0ul,
    0x3fdf0fc25d05490ful, 0x3c46f08a2b679db6ul, 0x3ff216c57db79f68ul, 0x3c9804528fe01eb7ul,
    0x3fd59265e1e68e58ul, 0xbc6892a5140721f3ul, 0x3fdc46c6c1d5f00cul, 0x3c67290f0ee2391eul,
    0x3fde443188963880ul, 0x3c53f2fe63413632ul, 0x3fe4c3cca922cb37ul, 0x3c8fe3669650fe6bul,
    0x3fece36addc0cb9dul, 0x3c751cf5e20376d3ul, 0x3ff56ee941684e75ul, 0xbc974edc62b1fa1eul,
    0x400051691becfc61ul, 0x3ca4143c84850a0cul, 0x400989e1dc6a563bul, 0x40145c0feb51ed02ul,
    0x40207e5e4e9bee5eul, 0x402b11605f45881ful, 0x403674119f7b7091ul, 0x4042cc19b683161ful,
    0x404fb90f939c90e5ul, 0x405af3e8527de716ul, 0x406709ce8ce23733ul, 0x4073cc2815c26b54ul,
    0x40811834a5b4041ful, 0x408da9b898d5f628ul, 0x4099fd031a9cfd05ul, 0x40a5f41e23d811b7ul,
    0x3fe1db4efec353dcul, 0x3bfafa6aaac798c6ul, 0x3ff2dc7440f68902ul, 0x3c47c034ddc514d3ul,
    0x3fdbc1b542b73ed6ul, 0xbc6a48a23c0dfe4eul, 0x3fe2f2d20cfa6ad3ul, 0x3c71758f7ae33227ul,
    0x3fe7db589d1238deul, 0xbc8c92bf33fa750eul, 0x3ff236660b484a1aul, 0x3c922c17bce51ebful,
    0x3ffcf12c7902ea42ul, 0x3c92d5c56f9ba9f4ul, 0x400843cfee0a425cul, 0xbcaeeccb21c9de62ul,
    0x4014f870b73b0e19ul, 0xbc8f811b38a52c40ul, 0x402298785d672a62ul, 0x4030cfa52af524b8ul,
    0x403ee15a851100ecul, 0x404cbb17732780bbul, 0x405b05b94d25e9d4ul, 0x4069a67db6eb4618ul,
    0x40788a9a75b19e68ul, 0x4087a474d5e64935ul, 0x4096e9fd3505dc07ul, 0x40a653a54130dab1ul,
    0x40b5dd834372dd92ul, 0x40c5834a42d3d9b6ul, 0x40d50fe8f6f2c2f0ul, 0x40e40d488ac9be75ul,
    0x3fe4060965b082c3ul, 0x3bc854ae016d3a8eul, 0x3ff3bd790165b0eful, 0xbc721824f056ff12ul,
    0x3fe199460df37244ul, 0x3c8b89f84d4ab242ul, 0x3fe9b49b09d18d4bul, 0x3c6301ccd087ba62ul,
    0x3ff2c9abf973e29eul, 0x3c5e1019ee9fd63cul, 0x4000206d04b2f94dul, 0xbca9349e9fb0605ful,
    0x400d3127071d7308ul, 0x3ca4a702c221b2d2ul, 0x401bc111c834d427ul, 0xbcb9feaa433d3133ul,
    0x402b3ecea3244efdul, 0x3cc4489475514812ul, 0x403b6de0d167866cul, 0x404c288cbeae7290ul,
    0x405d5ddbb05488c9ul, 0x406f06ab370bd675ul, 0x4080917f3947aaadul, 0x4091dbf289d49de6ul,
    0x40a36751668480acul, 0x40b53a239d9e5067ul, 0x40c75cd357d61582ul, 0x40d9d9b4f9f8c017ul,
    0x40ecbab60bc8c883ul, 0x410009d1bc5e8652ul, 0x4112831d78f2827bul, 0x4124d1cf7c99ba83ul,
    0x3fe6186398850c4dul, 0x3c4f4f6b76705b95ul, 0x3ff4c0fabe41c88cul, 0xbc8c55cc2eb4b19cul,
    0x3fe63d5e3c64ee74ul, 0x3c53184e2a2f1a19ul, 0x3ff1bc5917e77f33ul, 0x3c87ff041af095bcul,
    0x3ffdfe4b4faf5782ul, 0x3c9d37e3f298ba60ul, 0x400d366e88ace1d7ul, 0x3ca23c5f3ac1688bul,
    0x401e34e57302b9a0ul, 0xbcaede3bbda3acbaul, 0x40305f9ff99aa1b3ul, 0x3cdb363750ba514aul,
    0x40425707023b6951ul, 0xbce1b23da9ecdd4cul, 0x405510896e248370ul, 0x4068ac2ad67a84f9ul,
    0x407d5babb6f32e02ul, 0x4091b1dd5b68ecfeul, 0x40a590197dbe8cc6ul, 0x40ba8516308a9868ul,
    0x40d0701bfeb9be4aul, 0x40e484b3d18d3b7bul, 0x40f9c44faf02f0f8ul, 0x411043f9eebccf66ul,
    0x4124a152baa67ce6ul, 0x413a4730578c0829ul, 0x4151229bc2d78245ul, 0x41661fc344bcf6fdul,
    0x3fe81d88ed215a4bul, 0xbbf441902fc946dcul, 0x3ff5f0e8eca0a04dul, 0x3c90e1ec58c5aaa0ul,
    0x3fec3bb8ca0fc932ul, 0x3c8dc461952be298ul, 0x3ff90aae2f36a806ul, 0xbc8950e92e5a9a02ul,
    0x4008905ab48859d9ul, 0xbc9fc201e31f76d1ul, 0x401b6c6e339d48b0ul, 0x3cbf63d044e181f5ul,
    0x40305001af85634bul, 0xbca494cb9e5186c4ul, 0x40445466e669cb2dul, 0x3cd6c7874fa2f3f7ul,
    0x405a2f55ae23cae4ul, 0xbcbe5ca453f7eaa0ul, 0x40714a9d10022c86ul, 0x40874a2a996661f0ul,
    0x409fde3bf5e68dfcul, 0x40b6168a2d8835ceul, 0x40cef4096c8957beul, 0x40e5e3a83f657747ul,
    0x40ff3497a045279bul, 0x411665a5d99621e8ul, 0x41302c284a13e104ul, 0x41477b0fb994268aul,
    0x41611ecd82a9b598ul, 0x4179150e1c6d564cul, 0x4193024aaf342294ul, 0x41ac0bc0ae14b118ul,
    0x3fea1e5f9c00704bul, 0x3bc9f9b5421d2d36ul, 0x3ff75ba86bdd39a2ul, 0x3c9de9eba2b04c4aul,
    0x3ff22260c989df64ul, 0x3c91deae15880c8dul, 0x40023a2c5c9f6f03ul, 0x3ca2ae4748354027ul,
    0x4014e8cd1470ab07ul, 0xbcb9171802e6f34bul, 0x402b18c740fca856ul, 0xbc8a216612c55c4ful,
    0x4042bfa0dcf9b0edul, 0x3cec8920e64370dbul, 0x405b2b878ae2a4c8ul, 0xbcfca6294af59c54ul,
    0x4074597e32a88adful, 0x3d15897a9d05d504ul, 0x408f41d0942948f7ul, 0x40a87b5d1fa9f5e0ul,
    0x40c37b18207d4e0ful, 0x40df68864f74ce92ul, 0x40f9989787959070ul, 0x41150d86cf9f7939ul,
    0x41317436f0253311ul, 0x414d240ae7cd7b77ul, 0x4168795a5d430687ul, 0x4184aa2a1b880a71ul,
    0x41a185f6f19ef4feul, 0x41bddc0fb00260f1ul, 0x41da6554c55ed3d3ul, 0x41f6a957d868d924ul,
};


inline void gpga_asin_p0_quick(thread gpga_double* p_resh,
                               thread gpga_double* p_resm, gpga_double x,
                               int xhi) {
  if (xhi < (int)EXTRABOUND2) {
    gpga_double t2 = gpga_double_mul(p0_quick_coeff_3h, x);
    gpga_double t1 = gpga_double_mul(x, x);
    gpga_double t3 = gpga_double_mul(t1, t2);
    Add12(p_resh, p_resm, x, t3);
    return;
  }

  gpga_double p_x_0_pow2h = gpga_double_zero(0u);
  gpga_double p_x_0_pow2m = gpga_double_zero(0u);
  Mul12(&p_x_0_pow2h, &p_x_0_pow2m, x, x);

  gpga_double p_t_15_0h = p0_quick_coeff_5h;
  if (xhi > (int)EXTRABOUND) {
    gpga_double p_t_1_0h = p0_quick_coeff_19h;
    gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, p_x_0_pow2h);
    gpga_double p_t_3_0h = gpga_double_add(p0_quick_coeff_17h, p_t_2_0h);
    gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, p_x_0_pow2h);
    gpga_double p_t_5_0h = gpga_double_add(p0_quick_coeff_15h, p_t_4_0h);
    gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, p_x_0_pow2h);
    gpga_double p_t_7_0h = gpga_double_add(p0_quick_coeff_13h, p_t_6_0h);
    gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, p_x_0_pow2h);
    gpga_double p_t_9_0h = gpga_double_add(p0_quick_coeff_11h, p_t_8_0h);
    gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, p_x_0_pow2h);
    gpga_double p_t_11_0h = gpga_double_add(p0_quick_coeff_9h, p_t_10_0h);
    gpga_double p_t_12_0h = gpga_double_mul(p_t_11_0h, p_x_0_pow2h);
    gpga_double p_t_13_0h = gpga_double_add(p0_quick_coeff_7h, p_t_12_0h);
    gpga_double p_t_14_0h = gpga_double_mul(p_t_13_0h, p_x_0_pow2h);
    p_t_15_0h = gpga_double_add(p_t_15_0h, p_t_14_0h);
  }

  gpga_double p_t_16_0h = gpga_double_mul(p_t_15_0h, p_x_0_pow2h);
  gpga_double p_t_17_0h = gpga_double_zero(0u);
  gpga_double p_t_17_0m = gpga_double_zero(0u);
  Add12(&p_t_17_0h, &p_t_17_0m, p0_quick_coeff_3h, p_t_16_0h);

  gpga_double p_t_18_0h = gpga_double_zero(0u);
  gpga_double p_t_18_0m = gpga_double_zero(0u);
  Mul122(&p_t_18_0h, &p_t_18_0m, x, p_x_0_pow2h, p_x_0_pow2m);
  gpga_double p_t_19_0h = gpga_double_zero(0u);
  gpga_double p_t_19_0m = gpga_double_zero(0u);
  Mul22(&p_t_19_0h, &p_t_19_0m, p_t_17_0h, p_t_17_0m, p_t_18_0h,
        p_t_18_0m);
  gpga_double p_t_20_0h = gpga_double_zero(0u);
  gpga_double p_t_20_0m = gpga_double_zero(0u);
  Add122(&p_t_20_0h, &p_t_20_0m, x, p_t_19_0h, p_t_19_0m);

  *p_resh = p_t_20_0h;
  *p_resm = p_t_20_0m;
}

inline void gpga_asin_p_quick(thread gpga_double* p_resh,
                              thread gpga_double* p_resm, gpga_double x,
                              int index) {
  constant gpga_double* tbl = gpga_asin_poly_quick + (16 * index);
  gpga_double p_quick_coeff_0h = tbl[1];
  gpga_double p_quick_coeff_0m = tbl[2];
  gpga_double p_quick_coeff_1h = tbl[3];
  gpga_double p_quick_coeff_1m = tbl[4];
  gpga_double p_quick_coeff_2h = tbl[5];
  gpga_double p_quick_coeff_3h = tbl[6];
  gpga_double p_quick_coeff_4h = tbl[7];
  gpga_double p_quick_coeff_5h = tbl[8];
  gpga_double p_quick_coeff_6h = tbl[9];
  gpga_double p_quick_coeff_7h = tbl[10];
  gpga_double p_quick_coeff_8h = tbl[11];
  gpga_double p_quick_coeff_9h = tbl[12];
  gpga_double p_quick_coeff_10h = tbl[13];
  gpga_double p_quick_coeff_11h = tbl[14];
  gpga_double p_quick_coeff_12h = tbl[15];

  gpga_double p_t_1_0h = p_quick_coeff_12h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, x);
  gpga_double p_t_3_0h = gpga_double_add(p_quick_coeff_11h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, x);
  gpga_double p_t_5_0h = gpga_double_add(p_quick_coeff_10h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, x);
  gpga_double p_t_7_0h = gpga_double_add(p_quick_coeff_9h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, x);
  gpga_double p_t_9_0h = gpga_double_add(p_quick_coeff_8h, p_t_8_0h);
  gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, x);
  gpga_double p_t_11_0h = gpga_double_add(p_quick_coeff_7h, p_t_10_0h);
  gpga_double p_t_12_0h = gpga_double_mul(p_t_11_0h, x);
  gpga_double p_t_13_0h = gpga_double_add(p_quick_coeff_6h, p_t_12_0h);
  gpga_double p_t_14_0h = gpga_double_mul(p_t_13_0h, x);
  gpga_double p_t_15_0h = gpga_double_add(p_quick_coeff_5h, p_t_14_0h);
  gpga_double p_t_16_0h = gpga_double_mul(p_t_15_0h, x);
  gpga_double p_t_17_0h = gpga_double_add(p_quick_coeff_4h, p_t_16_0h);
  gpga_double p_t_18_0h = gpga_double_mul(p_t_17_0h, x);
  gpga_double p_t_19_0h = gpga_double_add(p_quick_coeff_3h, p_t_18_0h);
  gpga_double p_t_20_0h = gpga_double_mul(p_t_19_0h, x);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  Add12(&p_t_21_0h, &p_t_21_0m, p_quick_coeff_2h, p_t_20_0h);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_22_0h, &p_t_22_0m, p_quick_coeff_1h, p_quick_coeff_1m, x,
            p_t_21_0h, p_t_21_0m);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_23_0h, &p_t_23_0m, p_quick_coeff_0h, p_quick_coeff_0m, x,
            p_t_22_0h, p_t_22_0m);

  *p_resh = p_t_23_0h;
  *p_resm = p_t_23_0m;
}

inline void gpga_asin_p9_quick(thread gpga_double* p_resh,
                               thread gpga_double* p_resm, gpga_double x) {
  gpga_double p_t_1_0h = p9_quick_coeff_11h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, x);
  gpga_double p_t_3_0h = gpga_double_add(p9_quick_coeff_10h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, x);
  gpga_double p_t_5_0h = gpga_double_add(p9_quick_coeff_9h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, x);
  gpga_double p_t_7_0h = gpga_double_add(p9_quick_coeff_8h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, x);
  gpga_double p_t_9_0h = gpga_double_add(p9_quick_coeff_7h, p_t_8_0h);
  gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, x);
  gpga_double p_t_11_0h = gpga_double_add(p9_quick_coeff_6h, p_t_10_0h);
  gpga_double p_t_12_0h = gpga_double_mul(p_t_11_0h, x);
  gpga_double p_t_13_0h = gpga_double_add(p9_quick_coeff_5h, p_t_12_0h);
  gpga_double p_t_14_0h = gpga_double_mul(p_t_13_0h, x);
  gpga_double p_t_15_0h = gpga_double_add(p9_quick_coeff_4h, p_t_14_0h);
  gpga_double p_t_16_0h = gpga_double_mul(p_t_15_0h, x);
  gpga_double p_t_17_0h = gpga_double_add(p9_quick_coeff_3h, p_t_16_0h);
  gpga_double p_t_18_0h = gpga_double_mul(p_t_17_0h, x);
  gpga_double p_t_19_0h = gpga_double_add(p9_quick_coeff_2h, p_t_18_0h);
  gpga_double p_t_20_0h = gpga_double_mul(p_t_19_0h, x);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  Add12(&p_t_21_0h, &p_t_21_0m, p9_quick_coeff_1h, p_t_20_0h);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  Mul122(&p_t_22_0h, &p_t_22_0m, x, p_t_21_0h, p_t_21_0m);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  Add122(&p_t_23_0h, &p_t_23_0m, p9_quick_coeff_0h, p_t_22_0h, p_t_22_0m);

  *p_resh = p_t_23_0h;
  *p_resm = p_t_23_0m;
}

inline void gpga_asin_p0_accu(thread gpga_double* p_resh,
                              thread gpga_double* p_resm,
                              thread gpga_double* p_resl, gpga_double x) {
  gpga_double p_x_0_pow2h = gpga_double_zero(0u);
  gpga_double p_x_0_pow2m = gpga_double_zero(0u);
  Mul12(&p_x_0_pow2h, &p_x_0_pow2m, x, x);

  gpga_double p_t_1_0h = p0_accu_coeff_37h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, p_x_0_pow2h);
  gpga_double p_t_3_0h = gpga_double_add(p0_accu_coeff_35h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, p_x_0_pow2h);
  gpga_double p_t_5_0h = gpga_double_add(p0_accu_coeff_33h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, p_x_0_pow2h);
  gpga_double p_t_7_0h = gpga_double_zero(0u);
  gpga_double p_t_7_0m = gpga_double_zero(0u);
  Add12(&p_t_7_0h, &p_t_7_0m, p0_accu_coeff_31h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_zero(0u);
  gpga_double p_t_8_0m = gpga_double_zero(0u);
  Mul22(&p_t_8_0h, &p_t_8_0m, p_t_7_0h, p_t_7_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_9_0h = gpga_double_zero(0u);
  gpga_double p_t_9_0m = gpga_double_zero(0u);
  Add122(&p_t_9_0h, &p_t_9_0m, p0_accu_coeff_29h, p_t_8_0h, p_t_8_0m);
  gpga_double p_t_10_0h = gpga_double_zero(0u);
  gpga_double p_t_10_0m = gpga_double_zero(0u);
  Mul22(&p_t_10_0h, &p_t_10_0m, p_t_9_0h, p_t_9_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_11_0h = gpga_double_zero(0u);
  gpga_double p_t_11_0m = gpga_double_zero(0u);
  Add122(&p_t_11_0h, &p_t_11_0m, p0_accu_coeff_27h, p_t_10_0h, p_t_10_0m);
  gpga_double p_t_12_0h = gpga_double_zero(0u);
  gpga_double p_t_12_0m = gpga_double_zero(0u);
  Mul22(&p_t_12_0h, &p_t_12_0m, p_t_11_0h, p_t_11_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_13_0h = gpga_double_zero(0u);
  gpga_double p_t_13_0m = gpga_double_zero(0u);
  Add122(&p_t_13_0h, &p_t_13_0m, p0_accu_coeff_25h, p_t_12_0h, p_t_12_0m);
  gpga_double p_t_14_0h = gpga_double_zero(0u);
  gpga_double p_t_14_0m = gpga_double_zero(0u);
  Mul22(&p_t_14_0h, &p_t_14_0m, p_t_13_0h, p_t_13_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_15_0h = gpga_double_zero(0u);
  gpga_double p_t_15_0m = gpga_double_zero(0u);
  Add122(&p_t_15_0h, &p_t_15_0m, p0_accu_coeff_23h, p_t_14_0h, p_t_14_0m);
  gpga_double p_t_16_0h = gpga_double_zero(0u);
  gpga_double p_t_16_0m = gpga_double_zero(0u);
  Mul22(&p_t_16_0h, &p_t_16_0m, p_t_15_0h, p_t_15_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_17_0h = gpga_double_zero(0u);
  gpga_double p_t_17_0m = gpga_double_zero(0u);
  Add122(&p_t_17_0h, &p_t_17_0m, p0_accu_coeff_21h, p_t_16_0h, p_t_16_0m);
  gpga_double p_t_18_0h = gpga_double_zero(0u);
  gpga_double p_t_18_0m = gpga_double_zero(0u);
  Mul22(&p_t_18_0h, &p_t_18_0m, p_t_17_0h, p_t_17_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_19_0h = gpga_double_zero(0u);
  gpga_double p_t_19_0m = gpga_double_zero(0u);
  Add122(&p_t_19_0h, &p_t_19_0m, p0_accu_coeff_19h, p_t_18_0h, p_t_18_0m);
  gpga_double p_t_20_0h = gpga_double_zero(0u);
  gpga_double p_t_20_0m = gpga_double_zero(0u);
  Mul22(&p_t_20_0h, &p_t_20_0m, p_t_19_0h, p_t_19_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  Add122(&p_t_21_0h, &p_t_21_0m, p0_accu_coeff_17h, p_t_20_0h, p_t_20_0m);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  Mul22(&p_t_22_0h, &p_t_22_0m, p_t_21_0h, p_t_21_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  Add122(&p_t_23_0h, &p_t_23_0m, p0_accu_coeff_15h, p_t_22_0h, p_t_22_0m);
  gpga_double p_t_24_0h = gpga_double_zero(0u);
  gpga_double p_t_24_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_24_0h, &p_t_24_0m, p0_accu_coeff_13h, p0_accu_coeff_13m,
           p_x_0_pow2h, p_x_0_pow2m, p_t_23_0h, p_t_23_0m);
  gpga_double p_t_25_0h = gpga_double_zero(0u);
  gpga_double p_t_25_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_25_0h, &p_t_25_0m, p0_accu_coeff_11h, p0_accu_coeff_11m,
           p_x_0_pow2h, p_x_0_pow2m, p_t_24_0h, p_t_24_0m);
  gpga_double p_t_26_0h = gpga_double_zero(0u);
  gpga_double p_t_26_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_26_0h, &p_t_26_0m, p0_accu_coeff_9h, p0_accu_coeff_9m,
           p_x_0_pow2h, p_x_0_pow2m, p_t_25_0h, p_t_25_0m);
  gpga_double p_t_27_0h = gpga_double_zero(0u);
  gpga_double p_t_27_0m = gpga_double_zero(0u);
  Mul22(&p_t_27_0h, &p_t_27_0m, p_t_26_0h, p_t_26_0m, p_x_0_pow2h,
        p_x_0_pow2m);
  gpga_double p_t_28_0h = gpga_double_zero(0u);
  gpga_double p_t_28_0m = gpga_double_zero(0u);
  gpga_double p_t_28_0l = gpga_double_zero(0u);
  Add23(&p_t_28_0h, &p_t_28_0m, &p_t_28_0l, p0_accu_coeff_7h,
        p0_accu_coeff_7m, p_t_27_0h, p_t_27_0m);
  gpga_double p_t_29_0h = gpga_double_zero(0u);
  gpga_double p_t_29_0m = gpga_double_zero(0u);
  gpga_double p_t_29_0l = gpga_double_zero(0u);
  Mul233(&p_t_29_0h, &p_t_29_0m, &p_t_29_0l, p_x_0_pow2h, p_x_0_pow2m,
         p_t_28_0h, p_t_28_0m, p_t_28_0l);
  gpga_double p_t_30_0h = gpga_double_zero(0u);
  gpga_double p_t_30_0m = gpga_double_zero(0u);
  gpga_double p_t_30_0l = gpga_double_zero(0u);
  Add233(&p_t_30_0h, &p_t_30_0m, &p_t_30_0l, p0_accu_coeff_5h,
         p0_accu_coeff_5m, p_t_29_0h, p_t_29_0m, p_t_29_0l);
  gpga_double p_t_31_0h = gpga_double_zero(0u);
  gpga_double p_t_31_0m = gpga_double_zero(0u);
  gpga_double p_t_31_0l = gpga_double_zero(0u);
  Mul233(&p_t_31_0h, &p_t_31_0m, &p_t_31_0l, p_x_0_pow2h, p_x_0_pow2m,
         p_t_30_0h, p_t_30_0m, p_t_30_0l);
  gpga_double p_t_32_0h = gpga_double_zero(0u);
  gpga_double p_t_32_0m = gpga_double_zero(0u);
  gpga_double p_t_32_0l = gpga_double_zero(0u);
  Add233(&p_t_32_0h, &p_t_32_0m, &p_t_32_0l, p0_accu_coeff_3h,
         p0_accu_coeff_3m, p_t_31_0h, p_t_31_0m, p_t_31_0l);
  gpga_double p_t_33_0h = gpga_double_zero(0u);
  gpga_double p_t_33_0m = gpga_double_zero(0u);
  gpga_double p_t_33_0l = gpga_double_zero(0u);
  Mul233(&p_t_33_0h, &p_t_33_0m, &p_t_33_0l, p_x_0_pow2h, p_x_0_pow2m,
         p_t_32_0h, p_t_32_0m, p_t_32_0l);
  gpga_double p_t_34_0h = gpga_double_zero(0u);
  gpga_double p_t_34_0m = gpga_double_zero(0u);
  gpga_double p_t_34_0l = gpga_double_zero(0u);
  Add133(&p_t_34_0h, &p_t_34_0m, &p_t_34_0l, p0_accu_coeff_1h,
         p_t_33_0h, p_t_33_0m, p_t_33_0l);
  gpga_double p_t_35_0h = gpga_double_zero(0u);
  gpga_double p_t_35_0m = gpga_double_zero(0u);
  gpga_double p_t_35_0l = gpga_double_zero(0u);
  Mul133(&p_t_35_0h, &p_t_35_0m, &p_t_35_0l, x, p_t_34_0h, p_t_34_0m,
         p_t_34_0l);
  Renormalize3(p_resh, p_resm, p_resl, p_t_35_0h, p_t_35_0m, p_t_35_0l);
}

inline void gpga_asin_p_accu(thread gpga_double* p_resh,
                             thread gpga_double* p_resm,
                             thread gpga_double* p_resl, gpga_double x,
                             int index) {
  constant gpga_double* tbl = gpga_asin_poly_accu + (32 * index);
  gpga_double p_accu_coeff_0h = tbl[0];
  gpga_double p_accu_coeff_0m = tbl[1];
  gpga_double p_accu_coeff_1h = tbl[2];
  gpga_double p_accu_coeff_1m = tbl[3];
  gpga_double p_accu_coeff_2h = tbl[4];
  gpga_double p_accu_coeff_2m = tbl[5];
  gpga_double p_accu_coeff_3h = tbl[6];
  gpga_double p_accu_coeff_3m = tbl[7];
  gpga_double p_accu_coeff_4h = tbl[8];
  gpga_double p_accu_coeff_4m = tbl[9];
  gpga_double p_accu_coeff_5h = tbl[10];
  gpga_double p_accu_coeff_5m = tbl[11];
  gpga_double p_accu_coeff_6h = tbl[12];
  gpga_double p_accu_coeff_6m = tbl[13];
  gpga_double p_accu_coeff_7h = tbl[14];
  gpga_double p_accu_coeff_7m = tbl[15];
  gpga_double p_accu_coeff_8h = tbl[16];
  gpga_double p_accu_coeff_8m = tbl[17];
  gpga_double p_accu_coeff_9h = tbl[18];
  gpga_double p_accu_coeff_10h = tbl[19];
  gpga_double p_accu_coeff_11h = tbl[20];
  gpga_double p_accu_coeff_12h = tbl[21];
  gpga_double p_accu_coeff_13h = tbl[22];
  gpga_double p_accu_coeff_14h = tbl[23];
  gpga_double p_accu_coeff_15h = tbl[24];
  gpga_double p_accu_coeff_16h = tbl[25];
  gpga_double p_accu_coeff_17h = tbl[26];
  gpga_double p_accu_coeff_18h = tbl[27];
  gpga_double p_accu_coeff_19h = tbl[28];
  gpga_double p_accu_coeff_20h = tbl[29];
  gpga_double p_accu_coeff_21h = tbl[30];
  gpga_double p_accu_coeff_22h = tbl[31];

  gpga_double p_t_1_0h = p_accu_coeff_22h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, x);
  gpga_double p_t_3_0h = gpga_double_add(p_accu_coeff_21h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, x);
  gpga_double p_t_5_0h = gpga_double_add(p_accu_coeff_20h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, x);
  gpga_double p_t_7_0h = gpga_double_add(p_accu_coeff_19h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, x);
  gpga_double p_t_9_0h = gpga_double_add(p_accu_coeff_18h, p_t_8_0h);
  gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, x);
  gpga_double p_t_11_0h = gpga_double_add(p_accu_coeff_17h, p_t_10_0h);
  gpga_double p_t_12_0h = gpga_double_mul(p_t_11_0h, x);
  gpga_double p_t_13_0h = gpga_double_add(p_accu_coeff_16h, p_t_12_0h);
  gpga_double p_t_14_0h = gpga_double_mul(p_t_13_0h, x);
  gpga_double p_t_15_0h = gpga_double_add(p_accu_coeff_15h, p_t_14_0h);
  gpga_double p_t_16_0h = gpga_double_mul(p_t_15_0h, x);
  gpga_double p_t_17_0h = gpga_double_zero(0u);
  gpga_double p_t_17_0m = gpga_double_zero(0u);
  Add12(&p_t_17_0h, &p_t_17_0m, p_accu_coeff_14h, p_t_16_0h);
  gpga_double p_t_18_0h = gpga_double_zero(0u);
  gpga_double p_t_18_0m = gpga_double_zero(0u);
  Add122(&p_t_18_0h, &p_t_18_0m, p_accu_coeff_13h, p_t_17_0h, p_t_17_0m);
  gpga_double p_t_19_0h = gpga_double_zero(0u);
  gpga_double p_t_19_0m = gpga_double_zero(0u);
  Add122(&p_t_19_0h, &p_t_19_0m, p_accu_coeff_12h, p_t_18_0h, p_t_18_0m);
  gpga_double p_t_20_0h = gpga_double_zero(0u);
  gpga_double p_t_20_0m = gpga_double_zero(0u);
  Add122(&p_t_20_0h, &p_t_20_0m, p_accu_coeff_11h, p_t_19_0h, p_t_19_0m);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  Add122(&p_t_21_0h, &p_t_21_0m, p_accu_coeff_10h, p_t_20_0h, p_t_20_0m);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  Add122(&p_t_22_0h, &p_t_22_0m, p_accu_coeff_9h, p_t_21_0h, p_t_21_0m);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_23_0h, &p_t_23_0m, p_accu_coeff_8h, p_accu_coeff_8m, x,
            p_t_22_0h, p_t_22_0m);
  gpga_double p_t_24_0h = gpga_double_zero(0u);
  gpga_double p_t_24_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_24_0h, &p_t_24_0m, p_accu_coeff_7h, p_accu_coeff_7m, x,
            p_t_23_0h, p_t_23_0m);
  gpga_double p_t_25_0h = gpga_double_zero(0u);
  gpga_double p_t_25_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_25_0h, &p_t_25_0m, p_accu_coeff_6h, p_accu_coeff_6m, x,
            p_t_24_0h, p_t_24_0m);
  gpga_double p_t_26_0h = gpga_double_zero(0u);
  gpga_double p_t_26_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_26_0h, &p_t_26_0m, p_accu_coeff_5h, p_accu_coeff_5m, x,
            p_t_25_0h, p_t_25_0m);
  gpga_double p_t_27_0h = gpga_double_zero(0u);
  gpga_double p_t_27_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_27_0h, &p_t_27_0m, p_accu_coeff_4h, p_accu_coeff_4m, x,
            p_t_26_0h, p_t_26_0m);
  gpga_double p_t_28_0h = gpga_double_zero(0u);
  gpga_double p_t_28_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_28_0h, &p_t_28_0m, p_accu_coeff_3h, p_accu_coeff_3m, x,
            p_t_27_0h, p_t_27_0m);
  gpga_double p_t_29_0h = gpga_double_zero(0u);
  gpga_double p_t_29_0m = gpga_double_zero(0u);
  Add23(&p_t_29_0h, &p_t_29_0m, p_resl, p_accu_coeff_2h, p_accu_coeff_2m,
        p_t_28_0h, p_t_28_0m);
  gpga_double p_t_30_0h = gpga_double_zero(0u);
  gpga_double p_t_30_0m = gpga_double_zero(0u);
  gpga_double p_t_30_0l = gpga_double_zero(0u);
  Add233(&p_t_30_0h, &p_t_30_0m, &p_t_30_0l, p_accu_coeff_1h,
         p_accu_coeff_1m, p_t_29_0h, p_t_29_0m, *p_resl);
  gpga_double p_t_31_0h = gpga_double_zero(0u);
  gpga_double p_t_31_0m = gpga_double_zero(0u);
  gpga_double p_t_31_0l = gpga_double_zero(0u);
  Add233(&p_t_31_0h, &p_t_31_0m, &p_t_31_0l, p_accu_coeff_0h,
         p_accu_coeff_0m, p_t_30_0h, p_t_30_0m, p_t_30_0l);
  Renormalize3(p_resh, p_resm, p_resl, p_t_31_0h, p_t_31_0m, p_t_31_0l);
}

inline void gpga_asin_p9_accu(thread gpga_double* p_resh,
                              thread gpga_double* p_resm,
                              thread gpga_double* p_resl, gpga_double x) {
  gpga_double p_t_1_0h = p9_accu_coeff_20h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, x);
  gpga_double p_t_3_0h = gpga_double_add(p9_accu_coeff_19h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, x);
  gpga_double p_t_5_0h = gpga_double_add(p9_accu_coeff_18h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, x);
  gpga_double p_t_7_0h = gpga_double_add(p9_accu_coeff_17h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, x);
  gpga_double p_t_9_0h = gpga_double_add(p9_accu_coeff_16h, p_t_8_0h);
  gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, x);
  gpga_double p_t_11_0h = gpga_double_add(p9_accu_coeff_15h, p_t_10_0h);
  gpga_double p_t_12_0h = gpga_double_mul(p_t_11_0h, x);
  gpga_double p_t_13_0h = gpga_double_add(p9_accu_coeff_14h, p_t_12_0h);
  gpga_double p_t_14_0h = gpga_double_mul(p_t_13_0h, x);
  gpga_double p_t_15_0h = gpga_double_add(p9_accu_coeff_13h, p_t_14_0h);
  gpga_double p_t_16_0h = gpga_double_mul(p_t_15_0h, x);
  gpga_double p_t_17_0h = gpga_double_zero(0u);
  gpga_double p_t_17_0m = gpga_double_zero(0u);
  Add12(&p_t_17_0h, &p_t_17_0m, p9_accu_coeff_12h, p_t_16_0h);
  gpga_double p_t_18_0h = gpga_double_zero(0u);
  gpga_double p_t_18_0m = gpga_double_zero(0u);
  Add122(&p_t_18_0h, &p_t_18_0m, p9_accu_coeff_11h, p_t_17_0h, p_t_17_0m);
  gpga_double p_t_19_0h = gpga_double_zero(0u);
  gpga_double p_t_19_0m = gpga_double_zero(0u);
  Add122(&p_t_19_0h, &p_t_19_0m, p9_accu_coeff_10h, p_t_18_0h, p_t_18_0m);
  gpga_double p_t_20_0h = gpga_double_zero(0u);
  gpga_double p_t_20_0m = gpga_double_zero(0u);
  Add122(&p_t_20_0h, &p_t_20_0m, p9_accu_coeff_9h, p_t_19_0h, p_t_19_0m);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  Add122(&p_t_21_0h, &p_t_21_0m, p9_accu_coeff_8h, p_t_20_0h, p_t_20_0m);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_22_0h, &p_t_22_0m, p9_accu_coeff_7h, p9_accu_coeff_7m, x,
            p_t_21_0h, p_t_21_0m);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_23_0h, &p_t_23_0m, p9_accu_coeff_6h, p9_accu_coeff_6m, x,
            p_t_22_0h, p_t_22_0m);
  gpga_double p_t_24_0h = gpga_double_zero(0u);
  gpga_double p_t_24_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_24_0h, &p_t_24_0m, p9_accu_coeff_5h, p9_accu_coeff_5m, x,
            p_t_23_0h, p_t_23_0m);
  gpga_double p_t_25_0h = gpga_double_zero(0u);
  gpga_double p_t_25_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_25_0h, &p_t_25_0m, p9_accu_coeff_4h, p9_accu_coeff_4m, x,
            p_t_24_0h, p_t_24_0m);
  gpga_double p_t_26_0h = gpga_double_zero(0u);
  gpga_double p_t_26_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_26_0h, &p_t_26_0m, p9_accu_coeff_3h, p9_accu_coeff_3m, x,
            p_t_25_0h, p_t_25_0m);
  gpga_double p_t_27_0h = gpga_double_zero(0u);
  gpga_double p_t_27_0m = gpga_double_zero(0u);
  MulAdd212(&p_t_27_0h, &p_t_27_0m, p9_accu_coeff_2h, p9_accu_coeff_2m, x,
            p_t_26_0h, p_t_26_0m);
  gpga_double p_t_28_0h = gpga_double_zero(0u);
  gpga_double p_t_28_0m = gpga_double_zero(0u);
  Add23(&p_t_28_0h, &p_t_28_0m, p_resl, p9_accu_coeff_1h, p9_accu_coeff_1m,
        p_t_27_0h, p_t_27_0m);
  gpga_double p_t_29_0h = gpga_double_zero(0u);
  gpga_double p_t_29_0m = gpga_double_zero(0u);
  gpga_double p_t_29_0l = gpga_double_zero(0u);
  Add233(&p_t_29_0h, &p_t_29_0m, &p_t_29_0l, p9_accu_coeff_0h,
         p9_accu_coeff_0m, p_t_28_0h, p_t_28_0m, *p_resl);
  Renormalize3(p_resh, p_resm, p_resl, p_t_29_0h, p_t_29_0m, p_t_29_0l);
}

inline gpga_double gpga_asin_rn(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ASINSIMPLEBOUND) {
    return x;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return PIHALFH;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(PIHALFH);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);

  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinhover = gpga_double_zero(0u);
  gpga_double asinmover = gpga_double_zero(0u);
  gpga_double asinlover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    gpga_double test =
        gpga_double_add(asinh, gpga_double_mul(asinm, RNROUNDCST));
    if (gpga_double_eq(asinh, test)) {
      return asinh;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    return ReturnRoundToNearest3(asinh, asinm, asinl);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));

    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Add22(&asinh, &asinm, PIHALFH, PIHALFM, t1h, t1m);

    gpga_double test =
        gpga_double_add(asinh, gpga_double_mul(asinm, RNROUNDCST));
    if (gpga_double_eq(asinh, test)) {
      return neg ? gpga_double_neg(asinh) : asinh;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Add33(&asinhover, &asinmover, &asinlover, PIHALFH, PIHALFM, PIHALFL,
          t1h, t1m, t1l);
    Renormalize3(&asinh, &asinm, &asinl, asinhover, asinmover, asinlover);
    gpga_double asin = ReturnRoundToNearest3(asinh, asinm, asinl);
    return neg ? gpga_double_neg(asin) : asin;
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  gpga_double test =
      gpga_double_add(asinh, gpga_double_mul(asinm, RNROUNDCST));
  if (gpga_double_eq(asinh, test)) {
    return neg ? gpga_double_neg(asinh) : asinh;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  gpga_double asin = ReturnRoundToNearest3(asinh, asinm, asinl);
  return neg ? gpga_double_neg(asin) : asin;
}

inline gpga_double gpga_asin_ru(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ASINSIMPLEBOUND) {
    if (gpga_double_le(x, gpga_double_zero(0u))) {
      return x;
    }
    return gpga_double_next_up(x);
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return PIHALFRU;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(PIHALFH);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinhover = gpga_double_zero(0u);
  gpga_double asinmover = gpga_double_zero(0u);
  gpga_double asinlover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    return ReturnRoundUpwards3(asinh, asinm, asinl);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Add22(&asinh, &asinm, PIHALFH, PIHALFM, t1h, t1m);

    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Add33(&asinhover, &asinmover, &asinlover, PIHALFH, PIHALFM, PIHALFL,
          t1h, t1m, t1l);
    Renormalize3(&asinh, &asinm, &asinl, asinhover, asinmover, asinlover);
    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
      asinl = gpga_double_neg(asinl);
    }
    return ReturnRoundUpwards3(asinh, asinm, asinl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(asinh, asinm, RDROUNDCST, &quick)) {
    return quick;
  }
  if (gpga_double_eq(x, ASINBADCASEX)) {
    return ASINBADCASEYRU;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  return ReturnRoundUpwards3(asinh, asinm, asinl);
}

inline gpga_double gpga_asin_rd(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ASINSIMPLEBOUND) {
    if (gpga_double_ge(x, gpga_double_zero(0u))) {
      return x;
    }
    return gpga_double_next_down(x);
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return PIHALFH;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(PIHALFRU);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinhover = gpga_double_zero(0u);
  gpga_double asinmover = gpga_double_zero(0u);
  gpga_double asinlover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    return ReturnRoundDownwards3(asinh, asinm, asinl);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Add22(&asinh, &asinm, PIHALFH, PIHALFM, t1h, t1m);

    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Add33(&asinhover, &asinmover, &asinlover, PIHALFH, PIHALFM, PIHALFL,
          t1h, t1m, t1l);
    Renormalize3(&asinh, &asinm, &asinl, asinhover, asinmover, asinlover);
    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
      asinl = gpga_double_neg(asinl);
    }
    return ReturnRoundDownwards3(asinh, asinm, asinl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(asinh, asinm, RDROUNDCST, &quick)) {
    return quick;
  }
  if (gpga_double_eq(x, ASINBADCASEX)) {
    return ASINBADCASEYRD;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  return ReturnRoundDownwards3(asinh, asinm, asinl);
}

inline gpga_double gpga_asin_rz(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ASINSIMPLEBOUND) {
    return x;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return PIHALFH;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(PIHALFH);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinhover = gpga_double_zero(0u);
  gpga_double asinmover = gpga_double_zero(0u);
  gpga_double asinlover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    return ReturnRoundTowardsZero3(asinh, asinm, asinl);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Add22(&asinh, &asinm, PIHALFH, PIHALFM, t1h, t1m);

    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(asinh, asinm, RDROUNDCST, &quick)) {
      return quick;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Add33(&asinhover, &asinmover, &asinlover, PIHALFH, PIHALFM, PIHALFL,
          t1h, t1m, t1l);
    Renormalize3(&asinh, &asinm, &asinl, asinhover, asinmover, asinlover);
    if (neg) {
      asinh = gpga_double_neg(asinh);
      asinm = gpga_double_neg(asinm);
      asinl = gpga_double_neg(asinl);
    }
    return ReturnRoundTowardsZero3(asinh, asinm, asinl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(asinh, asinm, RDROUNDCST, &quick)) {
    return quick;
  }
  if (gpga_double_eq(x, ASINBADCASEX)) {
    return ASINBADCASEYRD;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (neg) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  return ReturnRoundTowardsZero3(asinh, asinm, asinl);
}

inline gpga_double gpga_acos_rn(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ACOSSIMPLEBOUND) {
    gpga_double acosh = gpga_double_zero(0u);
    gpga_double acosm = gpga_double_zero(0u);
    Add212(&acosh, &acosm, PIHALFH, PIHALFM, gpga_double_neg(x));
    return acosh;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return PIH;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    Add22(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
    gpga_double test =
        gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
    if (gpga_double_eq(acosh, test)) {
      return acosh;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
    Add33(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
          asinh, asinm, asinl);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundToNearest3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t1h;
      acosm = t1m;
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      Add22(&acosh, &acosm, PIH, PIM, t1h, t1m);
    }
    gpga_double test =
        gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
    if (gpga_double_eq(acosh, test)) {
      return acosh;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t1h, t1m, t1l);
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      t1l = gpga_double_neg(t1l);
      Add33(&acoshover, &acosmover, &acoslover, PIH, PIM, PIL, t1h, t1m,
            t1l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundToNearest3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Add22Cond(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
  gpga_double test =
      gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
  if (gpga_double_eq(acosh, test)) {
    return acosh;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Add33Cond(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
            asinh, asinm, asinl);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundToNearest3(acosh, acosm, acosl);
}

inline gpga_double gpga_acos_ru(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ACOSSIMPLEBOUND) {
    gpga_double acosh = gpga_double_zero(0u);
    gpga_double acosm = gpga_double_zero(0u);
    Add212(&acosh, &acosm, PIHALFH, PIHALFM, gpga_double_neg(x));
    if (gpga_double_lt(acosm, gpga_double_zero(0u))) {
      return acosh;
    }
    return gpga_double_next_up(acosh);
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return PIRU;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    Add22(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
    Add33(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
          asinh, asinm, asinl);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundUpwards3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t1h;
      acosm = t1m;
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      Add22(&acosh, &acosm, PIH, PIM, t1h, t1m);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t1h, t1m, t1l);
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      t1l = gpga_double_neg(t1l);
      Add33(&acoshover, &acosmover, &acoslover, PIH, PIM, PIL, t1h, t1m,
            t1l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundUpwards3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Add22Cond(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Add33Cond(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
            asinh, asinm, asinl);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundUpwards3(acosh, acosm, acosl);
}

inline gpga_double gpga_acos_rd(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;

  if (abs_hi < ACOSSIMPLEBOUND) {
    gpga_double acosh = gpga_double_zero(0u);
    gpga_double acosm = gpga_double_zero(0u);
    Add212(&acosh, &acosm, PIHALFH, PIHALFM, gpga_double_neg(x));
    if (gpga_double_gt(acosm, gpga_double_zero(0u))) {
      return acosh;
    }
    return gpga_double_next_down(acosh);
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return PIH;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double xabs = gpga_double_abs(x);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    Add22(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
    Add33(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
          asinh, asinm, asinl);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundDownwards3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t1h;
      acosm = t1m;
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      Add22(&acosh, &acosm, PIH, PIM, t1h, t1m);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t1h, t1m, t1l);
    } else {
      t1h = gpga_double_neg(t1h);
      t1m = gpga_double_neg(t1m);
      t1l = gpga_double_neg(t1l);
      Add33(&acoshover, &acosmover, &acoslover, PIH, PIM, PIL, t1h, t1m,
            t1l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundDownwards3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Add22Cond(&acosh, &acosm, PIHALFH, PIHALFM, asinh, asinm);
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Add33Cond(&acoshover, &acosmover, &acoslover, PIHALFH, PIHALFM, PIHALFL,
            asinh, asinm, asinl);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundDownwards3(acosh, acosm, acosl);
}

inline gpga_double gpga_acos_rz(gpga_double x) {
  return gpga_acos_rd(x);
}

inline gpga_double gpga_asinpi_rn(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ASINPISIMPLEBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }

    if (abs_hi >= ASINPINOSUBNORMALBOUND) {
      gpga_double xPih = gpga_double_zero(0u);
      gpga_double xPim = gpga_double_zero(0u);
      Mul122(&xPih, &xPim, x, RECPRPIH, RECPRPIM);
      gpga_double test = gpga_double_add(
          xPih, gpga_double_mul(xPim, RNROUNDCSTASINPI));
      if (gpga_double_eq(xPih, test)) {
        return xPih;
      }
      gpga_double xPihover = gpga_double_zero(0u);
      gpga_double xPimover = gpga_double_zero(0u);
      gpga_double xPilover = gpga_double_zero(0u);
      gpga_double xPil = gpga_double_zero(0u);
      Mul133(&xPihover, &xPimover, &xPilover, x, RECPRPIH, RECPRPIM,
             RECPRPIL);
      Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);
      return ReturnRoundToNearest3(xPih, xPim, xPil);
    }

    gpga_double xScaled = gpga_double_mul(x, TWO1000);
    gpga_double xPih = gpga_double_zero(0u);
    gpga_double xPim = gpga_double_zero(0u);
    gpga_double xPil = gpga_double_zero(0u);
    gpga_double xPihover = gpga_double_zero(0u);
    gpga_double xPimover = gpga_double_zero(0u);
    gpga_double xPilover = gpga_double_zero(0u);
    Mul133(&xPihover, &xPimover, &xPilover, xScaled, RECPRPIH, RECPRPIM,
           RECPRPIL);
    Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);

    gpga_double asinhdb = gpga_double_mul(xPih, TWOM1000);
    gpga_double tempdb = asinhdb;
    gpga_double temp1 = gpga_double_mul(asinhdb, TWO1000);
    gpga_double deltatemp = gpga_double_sub(xPih, temp1);

    gpga_double temp2h = gpga_double_zero(0u);
    gpga_double temp2l = gpga_double_zero(0u);
    Add12Cond(&temp2h, &temp2l, deltatemp, xPim);
    gpga_double temp3 = gpga_double_add(temp2l, xPil);
    gpga_double deltah = gpga_double_zero(0u);
    gpga_double deltal = gpga_double_zero(0u);
    Add12(&deltah, &deltal, temp2h, temp3);

    bool x_neg = gpga_double_sign(x) != 0u;
    bool delta_neg = gpga_double_sign(deltah) != 0u;
    if (x_neg ^ delta_neg) {
      tempdb -= 1ul;
    } else {
      tempdb += 1ul;
    }

    gpga_double miulp =
        gpga_double_mul(TWO999, gpga_double_sub(tempdb, asinhdb));
    gpga_double abs_deltah = gpga_double_abs(deltah);
    gpga_double abs_miulp = gpga_double_abs(miulp);

    if (gpga_double_lt(abs_deltah, abs_miulp)) {
      return asinhdb;
    }
    if (gpga_double_gt(abs_deltah, abs_miulp)) {
      return tempdb;
    }
    bool deltah_neg = gpga_double_sign(deltah) != 0u;
    bool deltal_neg = gpga_double_sign(deltal) != 0u;
    if (deltah_neg ^ deltal_neg) {
      return asinhdb;
    }
    return tempdb;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return half_val;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(half_val);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double asinpihover = gpga_double_zero(0u);
  gpga_double asinpimover = gpga_double_zero(0u);
  gpga_double asinpilover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
    gpga_double test =
        gpga_double_add(asinpih, gpga_double_mul(asinpim, RNROUNDCST));
    if (gpga_double_eq(asinpih, test)) {
      return asinpih;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
          RECPRPIH, RECPRPIM, RECPRPIL);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    return ReturnRoundToNearest3(asinpih, asinpim, asinpil);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, t1h, t1m, RECPRPIH, RECPRPIM);
    Add122(&asinpih, &asinpim, half_val, t2h, t2m);

    gpga_double test =
        gpga_double_add(asinpih, gpga_double_mul(asinpim, RNROUNDCST));
    if (gpga_double_eq(asinpih, test)) {
      return neg ? gpga_double_neg(asinpih) : asinpih;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, t1h, t1m, t1l, RECPRPIH, RECPRPIM, RECPRPIL);
    Add133(&asinpihover, &asinpimover, &asinpilover, half_val, t2h, t2m, t2l);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    gpga_double asinpi = ReturnRoundToNearest3(asinpih, asinpim, asinpil);
    return neg ? gpga_double_neg(asinpi) : asinpi;
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
  gpga_double test =
      gpga_double_add(asinpih, gpga_double_mul(asinpim, RNROUNDCST));
  if (gpga_double_eq(asinpih, test)) {
    return neg ? gpga_double_neg(asinpih) : asinpih;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
        RECPRPIH, RECPRPIM, RECPRPIL);
  Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
               asinpilover);
  gpga_double asinpi = ReturnRoundToNearest3(asinpih, asinpim, asinpil);
  return neg ? gpga_double_neg(asinpi) : asinpi;
}

inline gpga_double gpga_asinpi_rd(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ASINPISIMPLEBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    if (abs_hi >= ASINPINOSUBNORMALBOUND) {
      gpga_double xPih = gpga_double_zero(0u);
      gpga_double xPim = gpga_double_zero(0u);
      Mul122(&xPih, &xPim, x, RECPRPIH, RECPRPIM);
      gpga_double quick = gpga_double_zero(0u);
      if (gpga_test_and_return_rd(xPih, xPim, RDROUNDCSTASINPI, &quick)) {
        return quick;
      }
      gpga_double xPihover = gpga_double_zero(0u);
      gpga_double xPimover = gpga_double_zero(0u);
      gpga_double xPilover = gpga_double_zero(0u);
      gpga_double xPil = gpga_double_zero(0u);
      Mul133(&xPihover, &xPimover, &xPilover, x, RECPRPIH, RECPRPIM,
             RECPRPIL);
      Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);
      return ReturnRoundDownwards3(xPih, xPim, xPil);
    }

    gpga_double xScaled = gpga_double_mul(x, TWO1000);
    gpga_double xPih = gpga_double_zero(0u);
    gpga_double xPim = gpga_double_zero(0u);
    gpga_double xPil = gpga_double_zero(0u);
    gpga_double xPihover = gpga_double_zero(0u);
    gpga_double xPimover = gpga_double_zero(0u);
    gpga_double xPilover = gpga_double_zero(0u);
    Mul133(&xPihover, &xPimover, &xPilover, xScaled, RECPRPIH, RECPRPIM,
           RECPRPIL);
    Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);

    gpga_double asinhdb = gpga_double_mul(xPih, TWOM1000);
    gpga_double temp1 = gpga_double_mul(asinhdb, TWO1000);
    gpga_double deltatemp = gpga_double_sub(xPih, temp1);
    gpga_double temp2h = gpga_double_zero(0u);
    gpga_double temp2l = gpga_double_zero(0u);
    Add12Cond(&temp2h, &temp2l, deltatemp, xPim);
    gpga_double temp3 = gpga_double_add(temp2l, xPil);
    gpga_double deltah = gpga_double_zero(0u);
    gpga_double deltal = gpga_double_zero(0u);
    Add12(&deltah, &deltal, temp2h, temp3);

    if (gpga_double_ge(deltah, gpga_double_zero(0u))) {
      return asinhdb;
    }
    if (gpga_double_lt(x, gpga_double_zero(0u))) {
      asinhdb += 1ul;
    } else {
      asinhdb -= 1ul;
    }
    return asinhdb;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return half_val;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(half_val);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double asinpihover = gpga_double_zero(0u);
  gpga_double asinpimover = gpga_double_zero(0u);
  gpga_double asinpilover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
          RECPRPIH, RECPRPIM, RECPRPIL);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    return ReturnRoundDownwards3(asinpih, asinpim, asinpil);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, t1h, t1m, RECPRPIH, RECPRPIM);
    Add122(&asinpih, &asinpim, half_val, t2h, t2m);
    if (neg) {
      asinpih = gpga_double_neg(asinpih);
      asinpim = gpga_double_neg(asinpim);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }

    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, t1h, t1m, t1l, RECPRPIH, RECPRPIM, RECPRPIL);
    Add133(&asinpihover, &asinpimover, &asinpilover, half_val, t2h, t2m, t2l);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    gpga_double asinpi = ReturnRoundDownwards3(asinpih, asinpim, asinpil);
    return neg ? gpga_double_neg(asinpi) : asinpi;
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
  if (neg) {
    asinpih = gpga_double_neg(asinpih);
    asinpim = gpga_double_neg(asinpim);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(asinpih, asinpim, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
        RECPRPIH, RECPRPIM, RECPRPIL);
  Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
               asinpilover);
  gpga_double asinpi = ReturnRoundDownwards3(asinpih, asinpim, asinpil);
  return neg ? gpga_double_neg(asinpi) : asinpi;
}

inline gpga_double gpga_asinpi_ru(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ASINPISIMPLEBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    if (abs_hi >= ASINPINOSUBNORMALBOUND) {
      gpga_double xPih = gpga_double_zero(0u);
      gpga_double xPim = gpga_double_zero(0u);
      Mul122(&xPih, &xPim, x, RECPRPIH, RECPRPIM);
      gpga_double quick = gpga_double_zero(0u);
      if (gpga_test_and_return_ru(xPih, xPim, RDROUNDCSTASINPI, &quick)) {
        return quick;
      }
      gpga_double xPihover = gpga_double_zero(0u);
      gpga_double xPimover = gpga_double_zero(0u);
      gpga_double xPilover = gpga_double_zero(0u);
      gpga_double xPil = gpga_double_zero(0u);
      Mul133(&xPihover, &xPimover, &xPilover, x, RECPRPIH, RECPRPIM,
             RECPRPIL);
      Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);
      return ReturnRoundUpwards3(xPih, xPim, xPil);
    }

    gpga_double xScaled = gpga_double_mul(x, TWO1000);
    gpga_double xPih = gpga_double_zero(0u);
    gpga_double xPim = gpga_double_zero(0u);
    gpga_double xPil = gpga_double_zero(0u);
    gpga_double xPihover = gpga_double_zero(0u);
    gpga_double xPimover = gpga_double_zero(0u);
    gpga_double xPilover = gpga_double_zero(0u);
    Mul133(&xPihover, &xPimover, &xPilover, xScaled, RECPRPIH, RECPRPIM,
           RECPRPIL);
    Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);

    gpga_double asinhdb = gpga_double_mul(xPih, TWOM1000);
    gpga_double temp1 = gpga_double_mul(asinhdb, TWO1000);
    gpga_double deltatemp = gpga_double_sub(xPih, temp1);
    gpga_double temp2h = gpga_double_zero(0u);
    gpga_double temp2l = gpga_double_zero(0u);
    Add12Cond(&temp2h, &temp2l, deltatemp, xPim);
    gpga_double temp3 = gpga_double_add(temp2l, xPil);
    gpga_double deltah = gpga_double_zero(0u);
    gpga_double deltal = gpga_double_zero(0u);
    Add12(&deltah, &deltal, temp2h, temp3);

    if (gpga_double_le(deltah, gpga_double_zero(0u))) {
      return asinhdb;
    }
    if (gpga_double_lt(x, gpga_double_zero(0u))) {
      asinhdb -= 1ul;
    } else {
      asinhdb += 1ul;
    }
    return asinhdb;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return half_val;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(half_val);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double asinpihover = gpga_double_zero(0u);
  gpga_double asinpimover = gpga_double_zero(0u);
  gpga_double asinpilover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
          RECPRPIH, RECPRPIM, RECPRPIL);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    return ReturnRoundUpwards3(asinpih, asinpim, asinpil);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, t1h, t1m, RECPRPIH, RECPRPIM);
    Add122(&asinpih, &asinpim, half_val, t2h, t2m);
    if (neg) {
      asinpih = gpga_double_neg(asinpih);
      asinpim = gpga_double_neg(asinpim);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, t1h, t1m, t1l, RECPRPIH, RECPRPIM, RECPRPIL);
    Add133(&asinpihover, &asinpimover, &asinpilover, half_val, t2h, t2m, t2l);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    gpga_double asinpi = ReturnRoundUpwards3(asinpih, asinpim, asinpil);
    return neg ? gpga_double_neg(asinpi) : asinpi;
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
  if (neg) {
    asinpih = gpga_double_neg(asinpih);
    asinpim = gpga_double_neg(asinpim);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(asinpih, asinpim, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
        RECPRPIH, RECPRPIM, RECPRPIL);
  Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
               asinpilover);
  gpga_double asinpi = ReturnRoundUpwards3(asinpih, asinpim, asinpil);
  return neg ? gpga_double_neg(asinpi) : asinpi;
}

inline gpga_double gpga_asinpi_rz(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ASINPISIMPLEBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    if (abs_hi >= ASINPINOSUBNORMALBOUND) {
      gpga_double xPih = gpga_double_zero(0u);
      gpga_double xPim = gpga_double_zero(0u);
      Mul122(&xPih, &xPim, x, RECPRPIH, RECPRPIM);
      gpga_double quick = gpga_double_zero(0u);
      if (gpga_test_and_return_rz(xPih, xPim, RDROUNDCSTASINPI, &quick)) {
        return quick;
      }
      gpga_double xPihover = gpga_double_zero(0u);
      gpga_double xPimover = gpga_double_zero(0u);
      gpga_double xPilover = gpga_double_zero(0u);
      gpga_double xPil = gpga_double_zero(0u);
      Mul133(&xPihover, &xPimover, &xPilover, x, RECPRPIH, RECPRPIM,
             RECPRPIL);
      Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);
      return ReturnRoundTowardsZero3(xPih, xPim, xPil);
    }

    gpga_double xScaled = gpga_double_mul(x, TWO1000);
    gpga_double xPih = gpga_double_zero(0u);
    gpga_double xPim = gpga_double_zero(0u);
    gpga_double xPil = gpga_double_zero(0u);
    gpga_double xPihover = gpga_double_zero(0u);
    gpga_double xPimover = gpga_double_zero(0u);
    gpga_double xPilover = gpga_double_zero(0u);
    Mul133(&xPihover, &xPimover, &xPilover, xScaled, RECPRPIH, RECPRPIM,
           RECPRPIL);
    Renormalize3(&xPih, &xPim, &xPil, xPihover, xPimover, xPilover);

    gpga_double asinhdb = gpga_double_mul(xPih, TWOM1000);
    gpga_double temp1 = gpga_double_mul(asinhdb, TWO1000);
    gpga_double deltatemp = gpga_double_sub(xPih, temp1);
    gpga_double temp2h = gpga_double_zero(0u);
    gpga_double temp2l = gpga_double_zero(0u);
    Add12Cond(&temp2h, &temp2l, deltatemp, xPim);
    gpga_double temp3 = gpga_double_add(temp2l, xPil);
    gpga_double deltah = gpga_double_zero(0u);
    gpga_double deltal = gpga_double_zero(0u);
    Add12(&deltah, &deltal, temp2h, temp3);

    bool x_pos = gpga_double_gt(x, gpga_double_zero(0u));
    bool delta_neg = gpga_double_lt(deltah, gpga_double_zero(0u));
    if (x_pos ^ delta_neg) {
      return asinhdb;
    }
    asinhdb -= 1ul;
    return asinhdb;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return half_val;
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_neg(half_val);
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double asinpihover = gpga_double_zero(0u);
  gpga_double asinpimover = gpga_double_zero(0u);
  gpga_double asinpilover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
          RECPRPIH, RECPRPIM, RECPRPIL);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    return ReturnRoundTowardsZero3(asinpih, asinpim, asinpil);
  }

  bool neg = gpga_double_sign(x) != 0u;
  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, t1h, t1m, RECPRPIH, RECPRPIM);
    Add122(&asinpih, &asinpim, half_val, t2h, t2m);
    if (neg) {
      asinpih = gpga_double_neg(asinpih);
      asinpim = gpga_double_neg(asinpim);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(asinpih, asinpim, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    p9h = gpga_double_neg(p9h);
    p9m = gpga_double_neg(p9m);
    p9l = gpga_double_neg(p9l);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, t1h, t1m, t1l, RECPRPIH, RECPRPIM, RECPRPIL);
    Add133(&asinpihover, &asinpimover, &asinpilover, half_val, t2h, t2m, t2l);
    Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
                 asinpilover);
    gpga_double asinpi = ReturnRoundTowardsZero3(asinpih, asinpim, asinpil);
    return neg ? gpga_double_neg(asinpi) : asinpi;
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  Mul22(&asinpih, &asinpim, asinh, asinm, RECPRPIH, RECPRPIM);
  if (neg) {
    asinpih = gpga_double_neg(asinpih);
    asinpim = gpga_double_neg(asinpim);
  }
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(asinpih, asinpim, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  Mul33(&asinpihover, &asinpimover, &asinpilover, asinh, asinm, asinl,
        RECPRPIH, RECPRPIM, RECPRPIL);
  Renormalize3(&asinpih, &asinpim, &asinpil, asinpihover, asinpimover,
               asinpilover);
  gpga_double asinpi = ReturnRoundTowardsZero3(asinpih, asinpim, asinpil);
  return neg ? gpga_double_neg(asinpi) : asinpi;
}

inline gpga_double gpga_acospi_rn(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ACOSPISIMPLEBOUND) {
    return half_val;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return one;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, MRECPRPIH, MRECPRPIM, asinh, asinm);
    Add122(&acosh, &acosm, half_val, asinpih, asinpim);
    gpga_double test =
        gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
    if (gpga_double_eq(acosh, test)) {
      return acosh;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpih, &asinpim, &asinpil, MRECPRPIH, MRECPRPIM, MRECPRPIL,
          asinh, asinm, asinl);
    Add133(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
           asinpil);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundToNearest3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, RECPRPIH, RECPRPIM, t1h, t1m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t2h;
      acosm = t2m;
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      Add122(&acosh, &acosm, one, t2h, t2m);
    }
    gpga_double test =
        gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
    if (gpga_double_eq(acosh, test)) {
      return acosh;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, RECPRPIH, RECPRPIM, RECPRPIL, t1h, t1m, t1l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t2h, t2m, t2l);
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      t2l = gpga_double_neg(t2l);
      Add133(&acoshover, &acosmover, &acoslover, one, t2h, t2m, t2l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundToNearest3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Mul22(&asinpih, &asinpim, RECPRPIH, RECPRPIM, asinh, asinm);
  Add122Cond(&acosh, &acosm, half_val, asinpih, asinpim);
  gpga_double test =
      gpga_double_add(acosh, gpga_double_mul(acosm, RNROUNDCST));
  if (gpga_double_eq(acosh, test)) {
    return acosh;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Mul33(&asinpih, &asinpim, &asinpil, RECPRPIH, RECPRPIM, RECPRPIL, asinh,
        asinm, asinl);
  Add133Cond(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
             asinpil);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundToNearest3(acosh, acosm, acosl);
}

inline gpga_double gpga_acospi_rd(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ACOSPISIMPLEBOUND) {
    if (gpga_double_le(x, gpga_double_zero(0u))) {
      return half_val;
    }
    return HALFMINUSHALFULP;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return one;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, MRECPRPIH, MRECPRPIM, asinh, asinm);
    Add122(&acosh, &acosm, half_val, asinpih, asinpim);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpih, &asinpim, &asinpil, MRECPRPIH, MRECPRPIM, MRECPRPIL,
          asinh, asinm, asinl);
    Add133(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
           asinpil);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundDownwards3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, RECPRPIH, RECPRPIM, t1h, t1m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t2h;
      acosm = t2m;
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      Add122(&acosh, &acosm, one, t2h, t2m);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, RECPRPIH, RECPRPIM, RECPRPIL, t1h, t1m, t1l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t2h, t2m, t2l);
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      t2l = gpga_double_neg(t2l);
      Add133(&acoshover, &acosmover, &acoslover, one, t2h, t2m, t2l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundDownwards3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Mul22(&asinpih, &asinpim, RECPRPIH, RECPRPIM, asinh, asinm);
  Add122Cond(&acosh, &acosm, half_val, asinpih, asinpim);
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(acosh, acosm, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Mul33(&asinpih, &asinpim, &asinpil, RECPRPIH, RECPRPIM, RECPRPIL, asinh,
        asinm, asinl);
  Add133Cond(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
             asinpil);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundDownwards3(acosh, acosm, acosl);
}

inline gpga_double gpga_acospi_ru(gpga_double x) {
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double zdb = gpga_double_add(one, gpga_double_mul(x, x));
  uint abs_hi = gpga_u64_hi(x) & 0x7fffffffU;
  gpga_double xabs = gpga_double_abs(x);

  if (abs_hi < ACOSPISIMPLEBOUND) {
    if (gpga_double_ge(x, gpga_double_zero(0u))) {
      return half_val;
    }
    return HALFPLUSULP;
  }

  if (abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, one)) {
      return gpga_double_zero(0u);
    }
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return one;
    }
    return gpga_double_nan();
  }

  int index = (int)((gpga_u64_hi(zdb) & 0x000f0000u) >> 16);
  gpga_double asinh = gpga_double_zero(0u);
  gpga_double asinm = gpga_double_zero(0u);
  gpga_double asinl = gpga_double_zero(0u);
  gpga_double asinpih = gpga_double_zero(0u);
  gpga_double asinpim = gpga_double_zero(0u);
  gpga_double asinpil = gpga_double_zero(0u);
  gpga_double acosh = gpga_double_zero(0u);
  gpga_double acosm = gpga_double_zero(0u);
  gpga_double acosl = gpga_double_zero(0u);
  gpga_double acoshover = gpga_double_zero(0u);
  gpga_double acosmover = gpga_double_zero(0u);
  gpga_double acoslover = gpga_double_zero(0u);
  gpga_double p9h = gpga_double_zero(0u);
  gpga_double p9m = gpga_double_zero(0u);
  gpga_double p9l = gpga_double_zero(0u);
  gpga_double sqrh = gpga_double_zero(0u);
  gpga_double sqrm = gpga_double_zero(0u);
  gpga_double sqrl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2m = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);

  if (index == 0) {
    gpga_asin_p0_quick(&asinh, &asinm, x, (int)abs_hi);
    Mul22(&asinpih, &asinpim, MRECPRPIH, MRECPRPIM, asinh, asinm);
    Add122(&acosh, &acosm, half_val, asinpih, asinpim);
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p0_accu(&asinh, &asinm, &asinl, x);
    Mul33(&asinpih, &asinpim, &asinpil, MRECPRPIH, MRECPRPIM, MRECPRPIL,
          asinh, asinm, asinl);
    Add133(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
           asinpil);
    Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    return ReturnRoundUpwards3(acosh, acosm, acosl);
  }

  index--;
  if ((index & 0x8) != 0) {
    gpga_double z = gpga_double_sub(xabs, MI_9);
    gpga_double zp = gpga_double_mul(
        gpga_double_from_u32(2u), gpga_double_sub(one, xabs));
    gpga_asin_p9_quick(&p9h, &p9m, z);
    gpga_sqrt12_64_unfiltered(&sqrh, &sqrm, zp);
    Mul22(&t1h, &t1m, sqrh, sqrm, p9h, p9m);
    Mul22(&t2h, &t2m, RECPRPIH, RECPRPIM, t1h, t1m);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      acosh = t2h;
      acosm = t2m;
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      Add122(&acosh, &acosm, one, t2h, t2m);
    }
    gpga_double quick = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
      return quick;
    }
    gpga_asin_p9_accu(&p9h, &p9m, &p9l, z);
    gpga_sqrt13(&sqrh, &sqrm, &sqrl, zp);
    Mul33(&t1h, &t1m, &t1l, sqrh, sqrm, sqrl, p9h, p9m, p9l);
    Mul33(&t2h, &t2m, &t2l, RECPRPIH, RECPRPIM, RECPRPIL, t1h, t1m, t1l);
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      Renormalize3(&acosh, &acosm, &acosl, t2h, t2m, t2l);
    } else {
      t2h = gpga_double_neg(t2h);
      t2m = gpga_double_neg(t2m);
      t2l = gpga_double_neg(t2l);
      Add133(&acoshover, &acosmover, &acoslover, one, t2h, t2m, t2l);
      Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
    }
    return ReturnRoundUpwards3(acosh, acosm, acosl);
  }

  gpga_double mi_i = gpga_asin_poly_quick[16 * index];
  gpga_double z = gpga_double_sub(xabs, mi_i);
  gpga_asin_p_quick(&asinh, &asinm, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
  }
  Mul22(&asinpih, &asinpim, RECPRPIH, RECPRPIM, asinh, asinm);
  Add122Cond(&acosh, &acosm, half_val, asinpih, asinpim);
  gpga_double quick = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(acosh, acosm, RDROUNDCST, &quick)) {
    return quick;
  }
  gpga_asin_p_accu(&asinh, &asinm, &asinl, z, index);
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    asinh = gpga_double_neg(asinh);
    asinm = gpga_double_neg(asinm);
    asinl = gpga_double_neg(asinl);
  }
  Mul33(&asinpih, &asinpim, &asinpil, RECPRPIH, RECPRPIM, RECPRPIL, asinh,
        asinm, asinl);
  Add133Cond(&acoshover, &acosmover, &acoslover, half_val, asinpih, asinpim,
             asinpil);
  Renormalize3(&acosh, &acosm, &acosl, acoshover, acosmover, acoslover);
  return ReturnRoundUpwards3(acosh, acosm, acosl);
}

inline gpga_double gpga_acospi_rz(gpga_double x) {
  return gpga_acospi_rd(x);
}

// CRLIBM_TRIGO_ACCURATE_SCS

inline void scs_sin(scs_ptr x) {
  scs_t res_scs;
  scs_t x2;
  scs_t coeff;
  scs_square(x2, x);
  scs_set_const(res_scs, sin_scs_poly_ptr);
  scs_mul(res_scs, res_scs, x2);
  for (int i = 1; i < (DEGREE_SIN_SCS - 1) / 2; ++i) {
    scs_set_const(coeff, sin_scs_poly_ptr + i);
    scs_add(res_scs, coeff, res_scs);
    scs_mul(res_scs, res_scs, x2);
  }
  scs_mul(res_scs, res_scs, x);
  scs_add(x, x, res_scs);
}

inline void scs_cos(scs_ptr x) {
  scs_t res_scs;
  scs_t x2;
  scs_t coeff;
  scs_square(x2, x);
  scs_set_const(res_scs, cos_scs_poly_ptr);
  scs_mul(res_scs, res_scs, x2);
  for (int i = 1; i < DEGREE_COS_SCS / 2; ++i) {
    scs_set_const(coeff, cos_scs_poly_ptr + i);
    scs_add(res_scs, coeff, res_scs);
    scs_mul(res_scs, res_scs, x2);
  }
  scs_add(x, res_scs, SCS_ONE);
}

inline int rem_pio2_scs(scs_ptr result, scs_ptr x) {
  ulong r[SCS_NB_WORDS + 3];
  ulong tmp = 0ul;
  uint N = 0u;
  int sign = 1;
  int i = 0;
  int j = 0;
  int ind = 0;
  constant uint* two_over_pi_pt = nullptr;

  if ((X_EXP != gpga_double_from_u32(1u)) || (X_IND < -1)) {
    scs_set(result, x);
    return 0;
  }

  if (X_IND == -1) {
    r[0] = 0ul;
    r[1] = 0ul;
    r[2] = (ulong)scs_two_over_pi[0] * X_HW[0];
    r[3] = (ulong)scs_two_over_pi[0] * X_HW[1] +
           (ulong)scs_two_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 4; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 3] * X_HW[1] +
               (ulong)scs_two_over_pi[i - 2] * X_HW[0];
      }
    } else {
      for (i = 4; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 4] * X_HW[2] +
               (ulong)scs_two_over_pi[i - 3] * X_HW[1] +
               (ulong)scs_two_over_pi[i - 2] * X_HW[0];
      }
    }
  } else if (X_IND == 0) {
    r[0] = 0ul;
    r[1] = (ulong)scs_two_over_pi[0] * X_HW[0];
    r[2] = (ulong)scs_two_over_pi[0] * X_HW[1] +
           (ulong)scs_two_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 3; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 2] * X_HW[1] +
               (ulong)scs_two_over_pi[i - 1] * X_HW[0];
      }
    } else {
      for (i = 3; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 3] * X_HW[2] +
               (ulong)scs_two_over_pi[i - 2] * X_HW[1] +
               (ulong)scs_two_over_pi[i - 1] * X_HW[0];
      }
    }
  } else if (X_IND == 1) {
    r[0] = (ulong)scs_two_over_pi[0] * X_HW[0];
    r[1] = (ulong)scs_two_over_pi[0] * X_HW[1] +
           (ulong)scs_two_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 2; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 1] * X_HW[1] +
               (ulong)scs_two_over_pi[i] * X_HW[0];
      }
    } else {
      for (i = 2; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 2] * X_HW[2] +
               (ulong)scs_two_over_pi[i - 1] * X_HW[1] +
               (ulong)scs_two_over_pi[i] * X_HW[0];
      }
    }
  } else if (X_IND == 2) {
    r[0] = (ulong)scs_two_over_pi[0] * X_HW[1] +
           (ulong)scs_two_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i] * X_HW[1] +
               (ulong)scs_two_over_pi[i + 1] * X_HW[0];
      }
    } else {
      for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)scs_two_over_pi[i - 1] * X_HW[2] +
               (ulong)scs_two_over_pi[i] * X_HW[1] +
               (ulong)scs_two_over_pi[i + 1] * X_HW[0];
      }
    }
  } else {
    ind = X_IND - 3;
    two_over_pi_pt = scs_two_over_pi + ind;
    if (X_HW[2] == 0u) {
      for (i = 0; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)two_over_pi_pt[i + 1] * X_HW[1] +
               (ulong)two_over_pi_pt[i + 2] * X_HW[0];
      }
    } else {
      for (i = 0; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)two_over_pi_pt[i] * X_HW[2] +
               (ulong)two_over_pi_pt[i + 1] * X_HW[1] +
               (ulong)two_over_pi_pt[i + 2] * X_HW[0];
      }
    }
  }

  r[SCS_NB_WORDS + 1] += r[SCS_NB_WORDS + 2] >> SCS_NB_BITS;
  for (i = (SCS_NB_WORDS + 1); i > 0; --i) {
    tmp = r[i] >> SCS_NB_BITS;
    r[i - 1] += tmp;
    r[i] -= (tmp << SCS_NB_BITS);
  }

  N = (uint)r[0];

  if (r[1] > (ulong)(SCS_RADIX / 2u)) {
    N += 1u;
    sign = -1;
    for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
      r[i] = ((~(uint)r[i]) & 0x3fffffffU);
    }
  } else {
    sign = 1;
  }

  if (r[1] == 0ul) {
    if (r[2] == 0ul) {
      i = 3;
    } else {
      i = 2;
    }
  } else {
    i = 1;
  }

  for (j = 0; j < SCS_NB_WORDS; ++j) {
    R_HW[j] = (uint)r[i + j];
  }

  R_EXP = gpga_double_from_u32(1u);
  R_IND = -i;
  R_SGN = sign * X_SGN;

  scs_mul(result, Pio2_ptr, result);
  return X_SGN * (int)N;
}

inline gpga_double scs_sin_rn(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_sin(sc2);
      scs_get_d(&resd, sc2);
      return resd;
    case 1:
      scs_cos(sc2);
      scs_get_d(&resd, sc2);
      return resd;
    case 2:
      scs_sin(sc2);
      scs_get_d(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_cos(sc2);
      scs_get_d(&resd, sc2);
      return gpga_double_neg(resd);
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_sin_rd(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_sin(sc2);
      scs_get_d_minf(&resd, sc2);
      return resd;
    case 1:
      scs_cos(sc2);
      scs_get_d_minf(&resd, sc2);
      return resd;
    case 2:
      scs_sin(sc2);
      scs_get_d_pinf(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_cos(sc2);
      scs_get_d_pinf(&resd, sc2);
      return gpga_double_neg(resd);
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_sin_ru(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_sin(sc2);
      scs_get_d_pinf(&resd, sc2);
      return resd;
    case 1:
      scs_cos(sc2);
      scs_get_d_pinf(&resd, sc2);
      return resd;
    case 2:
      scs_sin(sc2);
      scs_get_d_minf(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_cos(sc2);
      scs_get_d_minf(&resd, sc2);
      return gpga_double_neg(resd);
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_sin_rz(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_sin(sc2);
      scs_get_d_zero(&resd, sc2);
      return resd;
    case 1:
      scs_cos(sc2);
      scs_get_d_zero(&resd, sc2);
      return resd;
    case 2:
      scs_sin(sc2);
      scs_get_d_zero(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_cos(sc2);
      scs_get_d_zero(&resd, sc2);
      return gpga_double_neg(resd);
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_cos_rn(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_cos(sc2);
      scs_get_d(&resd, sc2);
      return resd;
    case 1:
      scs_sin(sc2);
      scs_get_d(&resd, sc2);
      return gpga_double_neg(resd);
    case 2:
      scs_cos(sc2);
      scs_get_d(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_sin(sc2);
      scs_get_d(&resd, sc2);
      return resd;
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_cos_rd(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_cos(sc2);
      scs_get_d_minf(&resd, sc2);
      return resd;
    case 1:
      scs_sin(sc2);
      scs_get_d_pinf(&resd, sc2);
      return gpga_double_neg(resd);
    case 2:
      scs_cos(sc2);
      scs_get_d_pinf(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_sin(sc2);
      scs_get_d_minf(&resd, sc2);
      return resd;
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_cos_ru(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_cos(sc2);
      scs_get_d_pinf(&resd, sc2);
      return resd;
    case 1:
      scs_sin(sc2);
      scs_get_d_minf(&resd, sc2);
      return gpga_double_neg(resd);
    case 2:
      scs_cos(sc2);
      scs_get_d_minf(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_sin(sc2);
      scs_get_d_pinf(&resd, sc2);
      return resd;
    default:
      return gpga_double_zero(0u);
  }
}

inline gpga_double scs_cos_rz(gpga_double x) {
  scs_t sc1;
  scs_t sc2;
  gpga_double resd = gpga_double_zero(0u);
  int N = 0;

  scs_set_d(sc1, x);
  N = rem_pio2_scs(sc2, sc1);
  N = N & 0x00000003;
  switch (N) {
    case 0:
      scs_cos(sc2);
      scs_get_d_zero(&resd, sc2);
      return resd;
    case 1:
      scs_sin(sc2);
      scs_get_d_zero(&resd, sc2);
      return gpga_double_neg(resd);
    case 2:
      scs_cos(sc2);
      scs_get_d_zero(&resd, sc2);
      return gpga_double_neg(resd);
    case 3:
      scs_sin(sc2);
      scs_get_d_zero(&resd, sc2);
      return resd;
    default:
      return gpga_double_zero(0u);
  }
}

inline void scs_tan(gpga_double x, scs_ptr res_scs) {
  scs_t x_scs;
  scs_t y_scs;
  scs_t x2;
  scs_t coeff;
  int N = 0;

  scs_set_d(x_scs, x);
  N = rem_pio2_scs(y_scs, x_scs);
  N = N & 1;
  scs_square(x2, y_scs);

  scs_set_const(res_scs, tan_scs_poly_ptr);
  scs_mul(res_scs, res_scs, x2);
  for (int i = 1; i < (DEGREE_TAN_SCS - 1) / 2; ++i) {
    scs_set_const(coeff, tan_scs_poly_ptr + i);
    scs_add(res_scs, coeff, res_scs);
    scs_mul(res_scs, res_scs, x2);
  }
  scs_mul(res_scs, res_scs, y_scs);
  scs_add(res_scs, y_scs, res_scs);

  if (N == 1) {
    scs_inv(res_scs, res_scs);
    res_scs->sign = -res_scs->sign;
  }
}

inline gpga_double scs_tan_rn(gpga_double x) {
  scs_t res_scs;
  gpga_double resd = gpga_double_zero(0u);
  scs_tan(x, res_scs);
  scs_get_d(&resd, res_scs);
  return resd;
}

inline gpga_double scs_tan_rd(gpga_double x) {
  scs_t res_scs;
  gpga_double resd = gpga_double_zero(0u);
  scs_tan(x, res_scs);
  scs_get_d_minf(&resd, res_scs);
  return resd;
}

inline gpga_double scs_tan_ru(gpga_double x) {
  scs_t res_scs;
  gpga_double resd = gpga_double_zero(0u);
  scs_tan(x, res_scs);
  scs_get_d_pinf(&resd, res_scs);
  return resd;
}

inline gpga_double scs_tan_rz(gpga_double x) {
  scs_t res_scs;
  gpga_double resd = gpga_double_zero(0u);
  scs_tan(x, res_scs);
  scs_get_d_zero(&resd, res_scs);
  return resd;
}

GPGA_CONST uint XMAX_RETURN_X_FOR_SIN = 0x3E4FFFFEu;
GPGA_CONST uint XMAX_SIN_CASE2 = 0x3F8921F9u;
GPGA_CONST uint XMAX_RETURN_1_FOR_COS_RN = 0x3E46A09Cu;
GPGA_CONST uint XMAX_RETURN_1_FOR_COS_RDIR = 0x3E4FFFFEu;
GPGA_CONST uint XMAX_COS_CASE2 = 0x3F8921F9u;
GPGA_CONST uint XMAX_RETURN_X_FOR_TAN = 0x3E3FFFFEu;
GPGA_CONST uint XMAX_TAN_CASE2 = 0x3FAFFFFEu;

GPGA_CONST gpga_double ONE_ROUNDED_DOWN =
    gpga_bits_to_real(0x3feffffffffffffful);
GPGA_CONST gpga_double EPS_SIN_CASE2 =
    gpga_bits_to_real(0x3bcbf6ecf516aab6ul);
GPGA_CONST gpga_double RN_CST_SIN_CASE2 =
    gpga_bits_to_real(0x3ff000dfc563fef6ul);
GPGA_CONST gpga_double EPS_COS_CASE2 =
    gpga_bits_to_real(0x3be6564c8577e0bdul);
GPGA_CONST gpga_double RN_CST_COS_CASE2 =
    gpga_bits_to_real(0x3ff002cb7c6fcaeful);
GPGA_CONST gpga_double EPS_SINCOS_CASE3 =
    gpga_bits_to_real(0x3be8000000000000ul);
GPGA_CONST gpga_double RN_CST_SINCOS_CASE3 =
    gpga_bits_to_real(0x3ff00300c0300c04ul);
GPGA_CONST gpga_double EPS_TAN_CASE2 =
    gpga_bits_to_real(0x3c20f4d172f7c308ul);
GPGA_CONST gpga_double EPS_TAN_CASE3 =
    gpga_bits_to_real(0x3bf9333333333333ul);
GPGA_CONST gpga_double RN_CST_TAN_CASE3 =
    gpga_bits_to_real(0x3ff0064ff4c73065ul);

GPGA_CONST gpga_double INV_PIO256 =
    gpga_bits_to_real(0x40545f306dc9c883ul);

GPGA_CONST uint XMAX_CODY_WAITE_2 = 0x40B921F9u;
GPGA_CONST uint XMAX_CODY_WAITE_3 = 0x416921F9u;
GPGA_CONST uint XMAX_DDRR = 0x426921F9u;

GPGA_CONST gpga_double RR_CW2_CH =
    gpga_bits_to_real(0x3f8921fb54480000ul);
GPGA_CONST gpga_double RR_CW2_MCL =
    gpga_bits_to_real(0x3d5e973dcb3b399dul);
GPGA_CONST gpga_double RR_CW3_CH =
    gpga_bits_to_real(0x3f8921fb40000000ul);
GPGA_CONST gpga_double RR_CW3_CM =
    gpga_bits_to_real(0x3e04442d00000000ul);
GPGA_CONST gpga_double RR_CW3_MCL =
    gpga_bits_to_real(0xbc88469898cc5170ul);
GPGA_CONST gpga_double RR_DD_MCH =
    gpga_bits_to_real(0xbf8921fb54442d18ul);
GPGA_CONST gpga_double RR_DD_MCM =
    gpga_bits_to_real(0xbc21a62633145c07ul);
GPGA_CONST gpga_double RR_DD_CL =
    gpga_bits_to_real(0xb8af1976b7ed8fbcul);

// trigo_fast constants (little-endian)
GPGA_CONST gpga_double trigo_fast_s3 = 0xbfc5555555555555ul;
GPGA_CONST gpga_double trigo_fast_s5 = 0x3f81111111111111ul;
GPGA_CONST gpga_double trigo_fast_s7 = 0xbf2a01a01a01a01aul;
GPGA_CONST gpga_double trigo_fast_c2 = 0xbfe0000000000000ul;
GPGA_CONST gpga_double trigo_fast_c4 = 0x3fa5555555555555ul;
GPGA_CONST gpga_double trigo_fast_c6 = 0xbf56c16c16c16c17ul;
GPGA_CONST gpga_double trigo_fast_t3h = 0x3fd5555555555555ul;
GPGA_CONST gpga_double trigo_fast_t3l = 0x3c7cb8e2b4ee83f1ul;
GPGA_CONST gpga_double trigo_fast_t5 = 0x3fc1111111110586ul;
GPGA_CONST gpga_double trigo_fast_t7 = 0x3faba1ba1d1301a5ul;
GPGA_CONST gpga_double trigo_fast_t9 = 0x3f9664ec751be4a4ul;
GPGA_CONST gpga_double trigo_fast_t11 = 0x3f823953efc04f73ul;

GPGA_CONST gpga_double trigo_fast_sincos_table[260] = {
    0x0000000000000000ul, 0x0000000000000000ul, 0x3ff0000000000000ul,
    0x0000000000000000ul, 0x3f8921d1fcdec784ul, 0x3c29878ebe836d9dul,
    0x3fefff62169b92dbul, 0x3c85dda3c81fbd0dul, 0x3f992155f7a3667eul,
    0xbbfb1d63091a0130ul, 0x3feffd886084cd0dul, 0xbc81354d4556e4cbul,
    0x3fa2d865759455cdul, 0x3c2686f65ba93ac0ul, 0x3feffa72effef75dul,
    0xbc88b4cdcdb25956ul, 0x3fa91f65f10dd814ul, 0xbc2912bd0d569a90ul,
    0x3feff621e3796d7eul, 0xbc6c57bc2e24aa15ul, 0x3faf656e79f820e0ul,
    0xbc22e1ebe392bffeul, 0x3feff095658e71adul, 0x3c801a8ce18a4b9eul,
    0x3fb2d52092ce19f6ul, 0xbc49a088a8bf6b2cul, 0x3fefe9cdad01883aul,
    0x3c6521ecd0c67e35ul, 0x3fb5f6d00a9aa419ul, 0xbc4f4022d03f6c9aul,
    0x3fefe1cafcbd5b09ul, 0x3c6a23e3202a884eul, 0x3fb917a6bc29b42cul,
    0xbc3e2718d26ed688ul, 0x3fefd88da3d12526ul, 0xbc887df6378811c7ul,
    0x3fbc3785c79ec2d5ul, 0xbc24f39df133fb21ul, 0x3fefce15fd6da67bul,
    0xbc75dd6f830d4c09ul, 0x3fbf564e56a9730eul, 0x3c4a2704729ae56dul,
    0x3fefc26470e19fd3ul, 0x3c81ec8668ecaceeul, 0x3fc139f0cedaf577ul,
    0xbc6523434d1b3cfaul, 0x3fefb5797195d741ul, 0x3c71bfac7397cc08ul,
    0x3fc2c8106e8e613aul, 0x3c513000a89a11e0ul, 0x3fefa7557f08a517ul,
    0xbc87a0a8ca13571ful, 0x3fc45576b1293e5aul, 0xbc5285a24119f7b1ul,
    0x3fef97f924c9099bul, 0xbc8e2ae0eea5963bul, 0x3fc5e214448b3fc6ul,
    0x3c6531ff779ddac6ul, 0x3fef8764fa714ba9ul, 0x3c7ab256778ffcb6ul,
    0x3fc76dd9de50bf31ul, 0x3c61d5eeec501b2ful, 0x3fef7599a3a12077ul,
    0x3c884f31d743195cul, 0x3fc8f8b83c69a60bul, 0xbc626d19b9ff8d82ul,
    0x3fef6297cff75cb0ul, 0x3c7562172a361fd3ul, 0x3fca82a025b00451ul,
    0xbc687905ffd084adul, 0x3fef4e603b0b2f2dul, 0xbc78ee01e695ac05ul,
    0x3fcc0b826a7e4f63ul, 0xbc1af1439e521935ul, 0x3fef38f3ac64e589ul,
    0xbc7d7bafb51f72e6ul, 0x3fcd934fe5454311ul, 0x3c675b92277107adul,
    0x3fef2252f7763adaul, 0xbc820cb81c8d94abul, 0x3fcf19f97b215f1bul,
    0xbc642deef11da2c4ul, 0x3fef0a7efb9230d7ul, 0x3c752c7adc6b4989ul,
    0x3fd04fb80e37fdaeul, 0xbc0412cdb72583ccul, 0x3feef178a3e473c2ul,
    0x3c86310a67fe774ful, 0x3fd111d262b1f677ul, 0x3c7824c20ab7aa9aul,
    0x3feed740e7684963ul, 0x3c7e82c791f59cc2ul, 0x3fd1d3443f4cdb3eul,
    0xbc6720d41c13519eul, 0x3feebbd8c8df0b74ul, 0x3c7c6c8c615e7277ul,
    0x3fd294062ed59f06ul, 0xbc75d28da2c4612dul, 0x3fee9f4156c62ddaul,
    0x3c8760b1e2e3f81eul, 0x3fd35410c2e18152ul, 0xbc73cb002f96e062ul,
    0x3fee817bab4cd10dul, 0xbc7d0afe686b5e0aul, 0x3fd4135c94176601ul,
    0x3c70c97c4afa2518ul, 0x3fee6288ec48e112ul, 0xbc616b56f2847754ul,
    0x3fd4d1e24278e76aul, 0x3c62417218792858ul, 0x3fee426a4b2bc17eul,
    0x3c8a873889744882ul, 0x3fd58f9a75ab1fddul, 0xbc1efdc0d58cf620ul,
    0x3fee212104f686e5ul, 0xbc8014c76c126527ul, 0x3fd64c7ddd3f27c6ul,
    0x3c510d2b4a664121ul, 0x3fedfeae622dbe2bul, 0xbc8514ea88425567ul,
    0x3fd7088530fa459ful, 0xbc744b19e0864c5dul, 0x3feddb13b6ccc23cul,
    0x3c883c37c6107db3ul, 0x3fd7c3a9311dcce7ul, 0x3c19a3f21ef3e8d9ul,
    0x3fedb6526238a09bul, 0xbc7adee7eae69460ul, 0x3fd87de2a6aea963ul,
    0xbc672cedd3d5a610ul, 0x3fed906bcf328d46ul, 0x3c7457e610231ac2ul,
    0x3fd9372a63bc93d7ul, 0x3c6684319e5ad5b1ul, 0x3fed696173c9e68bul,
    0xbc7e8c61c6393d55ul, 0x3fd9ef7943a8ed8aul, 0x3c66da81290bdbabul,
    0x3fed4134d14dc93aul, 0xbc84ef5295d25af2ul, 0x3fdaa6c82b6d3fcaul,
    0xbc7d5f106ee5ccf7ul, 0x3fed17e7743e35dcul, 0xbc5101da3540130aul,
    0x3fdb5d1009e15cc0ul, 0x3c65b362cb974183ul, 0x3feced7af43cc773ul,
    0xbc5e7b6bb5ab58aeul, 0x3fdc1249d8011ee7ul, 0xbc7813aabb515206ul,
    0x3fecc1f0f3fcfc5cul, 0x3c7e57613b68f6abul, 0x3fdcc66e9931c45eul,
    0x3c56850e59c37f8ful, 0x3fec954b213411f5ul, 0xbc52fb761e946603ul,
    0x3fdd79775b86e389ul, 0x3c7550ec87bc0575ul, 0x3fec678b3488739bul,
    0x3c6d86cac7c5ff5bul, 0x3fde2b5d3806f63bul, 0x3c5e0d891d3c6841ul,
    0x3fec38b2f180bdb1ul, 0xbc76e0b1757c8d07ul, 0x3fdedc1952ef78d6ul,
    0xbc7dd0f7c33edee6ul, 0x3fec08c426725549ul, 0x3c5b157fd80e2946ul,
    0x3fdf8ba4dbf89abaul, 0xbc32ec1fc1b776b8ul, 0x3febd7c0ac6f952aul,
    0xbc8825a732ac700aul, 0x3fe01cfc874c3eb7ul, 0xbc734a35e7c2368cul,
    0x3feba5aa673590d2ul, 0x3c87ea4e370753b6ul, 0x3fe073879922ffeeul,
    0xbc8a5a014347406cul, 0x3feb728345196e3eul, 0xbc8bc69f324e6d61ul,
    0x3fe0c9704d5d898ful, 0xbc88d3d7de6ee9b2ul, 0x3feb3e4d3ef55712ul,
    0xbc8eb6b8bf11a493ul, 0x3fe11eb3541b4b23ul, 0xbc8ef23b69abe4f1ul,
    0x3feb090a58150200ul, 0xbc8926da300ffcceul, 0x3fe1734d63dedb49ul,
    0xbc87eef2ccc50575ul, 0x3fead2bc9e21d511ul, 0xbc847fbe07bea548ul,
    0x3fe1c73b39ae68c8ul, 0x3c8b25dd267f6600ul, 0x3fea9b66290ea1a3ul,
    0x3c39f630e8b6dac8ul, 0x3fe21a799933eb59ul, 0xbc83a7b177c68fb2ul,
    0x3fea63091b02fae2ul, 0xbc7e911152248d10ul, 0x3fe26d054cdd12dful,
    0xbc85da743ef3770cul, 0x3fea29a7a0462782ul, 0xbc7128bb015df175ul,
    0x3fe2bedb25faf3eaul, 0xbc514981c796ee46ul, 0x3fe9ef43ef29af94ul,
    0x3c7b1dfcb60445c2ul, 0x3fe30ff7fce17035ul, 0xbc6efcc626f74a6ful,
    0x3fe9b3e047f38741ul, 0xbc830ee286712474ul, 0x3fe36058b10659f3ul,
    0xbc81fcb3a35857e7ul, 0x3fe9777ef4c7d742ul, 0xbc815479a240665eul,
    0x3fe3affa292050b9ul, 0x3c7e3e25e3954964ul, 0x3fe93a22499263fbul,
    0x3c83d419a920df0bul, 0x3fe3fed9534556d4ul, 0x3c836916608c5061ul,
    0x3fe8fbcca3ef940dul, 0xbc66dfa99c86f2f1ul, 0x3fe44cf325091dd6ul,
    0x3c68076a2cfdc6b3ul, 0x3fe8bc806b151741ul, 0xbc82c5e12ed1336dul,
    0x3fe49a449b9b0939ul, 0xbc827ee16d719b94ul, 0x3fe87c400fba2ebful,
    0xbc82dabc0c3f64cdul, 0x3fe4e6cabbe3e5e9ul, 0x3c63c293edceb327ul,
    0x3fe83b0e0bff976eul, 0xbc76f420f8ea3475ul, 0x3fe5328292a35596ul,
    0xbc7a12eb89da0257ul, 0x3fe7f8ece3571771ul, 0xbc89c8d8ce93c917ul,
    0x3fe57d69348ceca0ul, 0xbc875720992bfbb2ul, 0x3fe7b5df226aafaful,
    0xbc70f537acdf0ad7ul, 0x3fe5c77bbe65018cul, 0x3c8069ea9c0bc32aul,
    0x3fe771e75f037261ul, 0x3c75cfce8d84068ful, 0x3fe610b7551d2cdful,
    0xbc7251b352ff2a37ul, 0x3fe72d0837efff96ul, 0x3c80d4ef0f1d915cul,
    0x3fe6591925f0783dul, 0x3c8c3d64fbf5de23ul, 0x3fe6e74454eaa8aful,
    0xbc8dbc03c84e226eul, 0x3fe6a09e667f3bcdul, 0xbc8bdd3413b26456ul,
    0x3fe6a09e667f3bcdul, 0xbc8bdd3413b26456ul,
};

GPGA_CONST int trigo_fast_digits_256_over_pi[] = {
    0x51,       0x1F306DC9, 0x3220A94F, 0x384EAFA3, 0x3A9A6EE0,
    0x1B6C52B3, 0x9E21C82,  0x3FCA2C7,  0x15EF5DE2, 0x2C36E48D,
    0x31D2126E, 0x25C00C92, 0x177504E8, 0x32439FC3, 0x2F58E589,
    0x134E7DD1, 0x11AFA97,  0x1768909D, 0xCE38135,  0x28BEFC82,
    0x1CC8EB1C, 0x306A673E, 0x24E422FC, 0x177BF250, 0x1D8FFC4B,
    0x3FFBC0B3, 0x7F7978,   0x2316B414, 0x368FB69B, 0xFD9E4F9,
    0x184DBA7A, 0xC7ECD3C,  0x2FF516BA, 0x24F758FD, 0x1F2F8BD9,
    0x3A0E73EF, 0x5294975,  0xD7F6BF6,  0x8FC6AE8,  0x10AC0660,
    0x237E3DB5, 0x357E19F7, 0x2104D7A1, 0x2C3B53C7, 0x8B0AF73,
    0x3610CB3,  0xC2AF8A5,  0xD0811C,
};

GPGA_CONST scs trigo_fast_pio256 = {{
    0x00c90fdau,
    0x28885a30u,
    0x234c4c66u,
    0x0a2e0370u,
    0x1cd12902u,
    0x13822299u,
    0x3cc74020u,
    0x2efa98ecu,
}, gpga_db_one, -1, 1};

#define trigo_fast_pio256_ptr ((scs_const_ptr)&trigo_fast_pio256)

inline uint gpga_u64_hi(ulong value) {
  return (uint)(value >> 32);
}

inline uint gpga_u64_lo(ulong value) {
  return (uint)(value & 0xfffffffful);
}

inline gpga_double gpga_u64_from_words(uint hi, uint lo) {
  return ((ulong)hi << 32) | (ulong)lo;
}

GPGA_CONST gpga_double TRIGO_SHIFT1 =
    gpga_bits_to_real(0x3e10000000000000ul);
GPGA_CONST gpga_double TRIGO_SHIFT2 =
    gpga_bits_to_real(0x3c30000000000000ul);
GPGA_CONST gpga_double TRIGO_SHIFT3 =
    gpga_bits_to_real(0x3a50000000000000ul);

GPGA_CONST int GPGA_TRIGO_SIN = 0;
GPGA_CONST int GPGA_TRIGO_COS = 1;
GPGA_CONST int GPGA_TRIGO_TAN = 2;

struct gpga_rrinfo {
  gpga_double rh;
  gpga_double rl;
  gpga_double x;
  int absxhi;
  int function;
  int changesign;
};

inline int rem_pio256_scs(scs_ptr result, scs_ptr x) {
  ulong r[SCS_NB_WORDS + 3];
  ulong tmp = 0ul;
  uint N = 0u;
  int sign = 1;
  int i = 0;
  int j = 0;
  int ind = 0;
  constant int* digits_pt = nullptr;

  if ((X_EXP != gpga_double_from_u32(1u)) || (X_IND < -2)) {
    scs_set(result, x);
    return 0;
  }

  if (X_IND == -2) {
    r[0] = 0ul;
    r[1] = 0ul;
    r[2] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[0];
    r[3] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[1] +
           (ulong)trigo_fast_digits_256_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 4; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 3] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i - 2] * X_HW[0];
      }
    } else {
      for (i = 4; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 4] * X_HW[2] +
               (ulong)trigo_fast_digits_256_over_pi[i - 3] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i - 2] * X_HW[0];
      }
    }
  } else if (X_IND == -1) {
    r[0] = 0ul;
    r[1] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[0];
    r[2] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[1] +
           (ulong)trigo_fast_digits_256_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 3; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 2] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i - 1] * X_HW[0];
      }
    } else {
      for (i = 3; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 3] * X_HW[2] +
               (ulong)trigo_fast_digits_256_over_pi[i - 2] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i - 1] * X_HW[0];
      }
    }
  } else if (X_IND == 0) {
    r[0] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[0];
    r[1] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[1] +
           (ulong)trigo_fast_digits_256_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 2; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 1] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i] * X_HW[0];
      }
    } else {
      for (i = 2; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 2] * X_HW[2] +
               (ulong)trigo_fast_digits_256_over_pi[i - 1] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i] * X_HW[0];
      }
    }
  } else if (X_IND == 1) {
    r[0] = (ulong)trigo_fast_digits_256_over_pi[0] * X_HW[1] +
           (ulong)trigo_fast_digits_256_over_pi[1] * X_HW[0];
    if (X_HW[2] == 0u) {
      for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i + 1] * X_HW[0];
      }
    } else {
      for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)trigo_fast_digits_256_over_pi[i - 1] * X_HW[2] +
               (ulong)trigo_fast_digits_256_over_pi[i] * X_HW[1] +
               (ulong)trigo_fast_digits_256_over_pi[i + 1] * X_HW[0];
      }
    }
  } else {
    ind = (X_IND - 2);
    digits_pt = trigo_fast_digits_256_over_pi + ind;
    if (X_HW[2] == 0u) {
      for (i = 0; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)digits_pt[i + 1] * X_HW[1] +
               (ulong)digits_pt[i + 2] * X_HW[0];
      }
    } else {
      for (i = 0; i < (SCS_NB_WORDS + 3); ++i) {
        r[i] = (ulong)digits_pt[i] * X_HW[2] +
               (ulong)digits_pt[i + 1] * X_HW[1] +
               (ulong)digits_pt[i + 2] * X_HW[0];
      }
    }
  }

  r[SCS_NB_WORDS + 1] += r[SCS_NB_WORDS + 2] >> SCS_NB_BITS;
  for (i = (SCS_NB_WORDS + 1); i > 0; --i) {
    tmp = r[i] >> SCS_NB_BITS;
    r[i - 1] += tmp;
    r[i] -= (tmp << SCS_NB_BITS);
  }

  N = (uint)r[0];

  if (r[1] > (ulong)(SCS_RADIX / 2u)) {
    N += 1u;
    sign = -1;
    for (i = 1; i < (SCS_NB_WORDS + 3); ++i) {
      r[i] = ((~(uint)r[i]) & 0x3fffffffU);
    }
  } else {
    sign = 1;
  }

  if (r[1] == 0ul) {
    if (r[2] == 0ul) {
      i = 3;
    } else {
      i = 2;
    }
  } else {
    i = 1;
  }

  for (j = 0; j < SCS_NB_WORDS; ++j) {
    R_HW[j] = (uint)r[i + j];
  }

  R_EXP = gpga_double_from_u32(1u);
  R_IND = -i;
  R_SGN = sign * X_SGN;

  scs_mul(result, trigo_fast_pio256_ptr, result);
  return X_SGN * (int)N;
}

inline void gpga_range_reduction_scs(gpga_double x, thread int* k_out,
                                     thread int* index_out,
                                     thread int* quadrant_out,
                                     thread gpga_double* yh_out,
                                     thread gpga_double* yl_out) {
  scs_t X;
  scs_t Y;
  scs_set_d(X, x);
  int k = rem_pio256_scs(Y, X);
  int index = (k & 127) << 2;
  int quadrant = (k >> 7) & 3;
  gpga_double x0 = gpga_double_from_u32(Y->h_word[0]);
  gpga_double x1 =
      gpga_double_mul(gpga_double_from_u32(Y->h_word[1]), TRIGO_SHIFT1);
  gpga_double x2 =
      gpga_double_mul(gpga_double_from_u32(Y->h_word[2]), TRIGO_SHIFT2);
  gpga_double x3 =
      gpga_double_mul(gpga_double_from_u32(Y->h_word[3]), TRIGO_SHIFT3);
  int exp_bits = (Y->index * SCS_NB_BITS) + 1023;
  uint sign = (Y->sign < 0) ? 1u : 0u;
  gpga_double nb = gpga_double_pack(sign, (uint)exp_bits, 0ul);
  gpga_double yh = gpga_double_add(gpga_double_add(x2, x1), x0);
  gpga_double yl = gpga_double_add(
      gpga_double_add(gpga_double_add(gpga_double_sub(x0, yh), x1), x2), x3);
  yh = gpga_double_mul(yh, nb);
  yl = gpga_double_mul(yl, nb);
  *k_out = k;
  *index_out = index;
  *quadrant_out = quadrant;
  *yh_out = yh;
  *yl_out = yl;
}

inline void gpga_trigo_do_sin_zero(thread gpga_double* psh,
                                   thread gpga_double* psl, gpga_double yh,
                                   gpga_double yl) {
  gpga_double yh2 = gpga_double_mul(yh, yh);
  gpga_double ts = gpga_double_mul(
      yh2, gpga_double_add(
                trigo_fast_s3,
                gpga_double_mul(yh2, gpga_double_add(
                                          trigo_fast_s5,
                                          gpga_double_mul(yh2,
                                                          trigo_fast_s7)))));
  Add12(psh, psl, yh, gpga_double_add(yl, gpga_double_mul(ts, yh)));
}

inline void gpga_trigo_do_cos_zero(thread gpga_double* pch,
                                   thread gpga_double* pcl, gpga_double yh,
                                   gpga_double yl) {
  (void)yl;
  gpga_double yh2 = gpga_double_mul(yh, yh);
  gpga_double tc = gpga_double_mul(
      yh2, gpga_double_add(
                trigo_fast_c2,
                gpga_double_mul(yh2, gpga_double_add(
                                          trigo_fast_c4,
                                          gpga_double_mul(yh2,
                                                          trigo_fast_c6)))));
  Add12(pch, pcl, gpga_double_from_u32(1u), tc);
}

inline void gpga_trigo_do_sin_not_zero(
    thread gpga_double* psh, thread gpga_double* psl, gpga_double sah,
    gpga_double sal, gpga_double cah, gpga_double cal, gpga_double yh,
    gpga_double yl, gpga_double ts, gpga_double tc) {
  gpga_double thi = gpga_double_zero(0u);
  gpga_double tlo = gpga_double_zero(0u);
  gpga_double cahyh_h = gpga_double_zero(0u);
  gpga_double cahyh_l = gpga_double_zero(0u);
  Mul12(&cahyh_h, &cahyh_l, cah, yh);
  Add12(&thi, &tlo, sah, cahyh_h);
  gpga_double term =
      gpga_double_add(gpga_double_mul(cal, yh), gpga_double_mul(cah, yl));
  gpga_double inner = gpga_double_add(cahyh_l, term);
  gpga_double sum =
      gpga_double_add(sal, gpga_double_add(tlo, inner));
  gpga_double mix =
      gpga_double_add(gpga_double_mul(ts, cahyh_h), sum);
  gpga_double tlo2 = gpga_double_add(gpga_double_mul(tc, sah), mix);
  Add12(psh, psl, thi, tlo2);
}

inline void gpga_trigo_do_cos_not_zero(
    thread gpga_double* pch, thread gpga_double* pcl, gpga_double sah,
    gpga_double sal, gpga_double cah, gpga_double cal, gpga_double yh,
    gpga_double yl, gpga_double ts, gpga_double tc) {
  gpga_double thi = gpga_double_zero(0u);
  gpga_double tlo = gpga_double_zero(0u);
  gpga_double sahyh_h = gpga_double_zero(0u);
  gpga_double sahyh_l = gpga_double_zero(0u);
  Mul12(&sahyh_h, &sahyh_l, sah, yh);
  Add12(&thi, &tlo, cah, gpga_double_neg(sahyh_h));
  gpga_double term =
      gpga_double_add(gpga_double_mul(sal, yh), gpga_double_mul(sah, yl));
  gpga_double inner =
      gpga_double_add(sahyh_l, term);
  gpga_double sum =
      gpga_double_add(cal, gpga_double_sub(tlo, inner));
  gpga_double mix =
      gpga_double_sub(gpga_double_mul(ts, sahyh_h), sum);
  gpga_double tlo2 = gpga_double_sub(gpga_double_mul(tc, cah), mix);
  Add12(pch, pcl, thi, tlo2);
}

inline void gpga_compute_trig_with_argred(thread gpga_rrinfo* rri) {
  gpga_double sah = gpga_double_zero(0u);
  gpga_double sal = gpga_double_zero(0u);
  gpga_double cah = gpga_double_zero(0u);
  gpga_double cal = gpga_double_zero(0u);
  gpga_double yh = gpga_double_zero(0u);
  gpga_double yl = gpga_double_zero(0u);
  gpga_double yh2 = gpga_double_zero(0u);
  gpga_double ts = gpga_double_zero(0u);
  gpga_double tc = gpga_double_zero(0u);
  gpga_double kd = gpga_double_zero(0u);
  gpga_double kch_h = gpga_double_zero(0u);
  gpga_double kch_l = gpga_double_zero(0u);
  gpga_double kcm_h = gpga_double_zero(0u);
  gpga_double kcm_l = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double tl = gpga_double_zero(0u);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  gpga_double ch = gpga_double_zero(0u);
  gpga_double cl = gpga_double_zero(0u);
  int k = 0;
  int quadrant = 0;
  int index = 0;
  long kl = 0l;
  bool index_zero = false;

  if (rri->absxhi < (int)XMAX_CODY_WAITE_3) {
    gpga_double prod = gpga_double_mul(rri->x, INV_PIO256);
    k = (int)gpga_double_round_to_s64(prod);
    kd = gpga_double_from_s64((long)k);
    quadrant = (k >> 7) & 3;
    index = (k & 127) << 2;
    if (index == 0) {
      Mul12(&kch_h, &kch_l, kd, RR_DD_MCH);
      Mul12(&kcm_h, &kcm_l, kd, RR_DD_MCM);
      Add12(&th, &tl, kch_l, kcm_h);
      Add22(&yh, &yl, gpga_double_add(rri->x, kch_h),
            gpga_double_sub(kcm_l, gpga_double_mul(kd, RR_DD_CL)), th, tl);
      index_zero = true;
    } else {
      if (rri->absxhi < (int)XMAX_CODY_WAITE_2) {
        Add12(&yh, &yl,
              gpga_double_sub(rri->x, gpga_double_mul(kd, RR_CW2_CH)),
              gpga_double_mul(kd, RR_CW2_MCL));
      } else {
        Add12Cond(&yh, &yl,
                  gpga_double_sub(
                      gpga_double_sub(rri->x, gpga_double_mul(kd, RR_CW3_CH)),
                      gpga_double_mul(kd, RR_CW3_CM)),
                  gpga_double_mul(kd, RR_CW3_MCL));
      }
    }
  } else if (rri->absxhi < (int)XMAX_DDRR) {
    gpga_double prod = gpga_double_mul(rri->x, INV_PIO256);
    kl = gpga_double_round_to_s64(prod);
    kd = gpga_double_from_s64(kl);
    quadrant = ((int)kl >> 7) & 3;
    index = ((int)kl & 127) << 2;
    if (index == 0) {
      gpga_range_reduction_scs(rri->x, &k, &index, &quadrant, &yh, &yl);
      index_zero = (index == 0);
    } else {
      Mul12(&kch_h, &kch_l, kd, RR_DD_MCH);
      Mul12(&kcm_h, &kcm_l, kd, RR_DD_MCM);
      Add12(&th, &tl, kch_l, kcm_h);
      Add22(&yh, &yl, gpga_double_add(rri->x, kch_h),
            gpga_double_sub(kcm_l, gpga_double_mul(kd, RR_DD_CL)), th, tl);
    }
  } else {
    gpga_range_reduction_scs(rri->x, &k, &index, &quadrant, &yh, &yl);
    index_zero = (index == 0);
  }

  if (index_zero) {
    switch (rri->function) {
      case GPGA_TRIGO_SIN:
        if (quadrant & 1) {
          gpga_trigo_do_cos_zero(&rri->rh, &rri->rl, yh, yl);
        } else {
          gpga_trigo_do_sin_zero(&rri->rh, &rri->rl, yh, yl);
        }
        rri->changesign = (quadrant == 2) || (quadrant == 3);
        return;
      case GPGA_TRIGO_COS:
        if (quadrant & 1) {
          gpga_trigo_do_sin_zero(&rri->rh, &rri->rl, yh, yl);
        } else {
          gpga_trigo_do_cos_zero(&rri->rh, &rri->rl, yh, yl);
        }
        rri->changesign = (quadrant == 1) || (quadrant == 2);
        return;
      case GPGA_TRIGO_TAN:
        rri->changesign = quadrant & 1;
        if (quadrant & 1) {
          gpga_trigo_do_sin_zero(&ch, &cl, yh, yl);
          gpga_trigo_do_cos_zero(&sh, &sl, yh, yl);
        } else {
          gpga_trigo_do_sin_zero(&sh, &sl, yh, yl);
          gpga_trigo_do_cos_zero(&ch, &cl, yh, yl);
        }
        Div22(&rri->rh, &rri->rl, sh, sl, ch, cl);
        return;
    }
  }

  if (index <= (64 << 2)) {
    sah = trigo_fast_sincos_table[index + 0];
    sal = trigo_fast_sincos_table[index + 1];
    cah = trigo_fast_sincos_table[index + 2];
    cal = trigo_fast_sincos_table[index + 3];
  } else {
    index = (128 << 2) - index;
    cah = trigo_fast_sincos_table[index + 0];
    cal = trigo_fast_sincos_table[index + 1];
    sah = trigo_fast_sincos_table[index + 2];
    sal = trigo_fast_sincos_table[index + 3];
  }

  yh2 = gpga_double_mul(yh, yh);
  ts = gpga_double_mul(
      yh2, gpga_double_add(
                trigo_fast_s3,
                gpga_double_mul(yh2, gpga_double_add(
                                          trigo_fast_s5,
                                          gpga_double_mul(yh2,
                                                          trigo_fast_s7)))));
  tc = gpga_double_mul(
      yh2, gpga_double_add(
                trigo_fast_c2,
                gpga_double_mul(yh2, gpga_double_add(
                                          trigo_fast_c4,
                                          gpga_double_mul(yh2,
                                                          trigo_fast_c6)))));

  switch (rri->function) {
    case GPGA_TRIGO_SIN:
      if (quadrant & 1) {
        gpga_trigo_do_cos_not_zero(&rri->rh, &rri->rl, sah, sal, cah, cal, yh,
                                   yl, ts, tc);
      } else {
        gpga_trigo_do_sin_not_zero(&rri->rh, &rri->rl, sah, sal, cah, cal, yh,
                                   yl, ts, tc);
      }
      rri->changesign = (quadrant == 2) || (quadrant == 3);
      return;
    case GPGA_TRIGO_COS:
      if (quadrant & 1) {
        gpga_trigo_do_sin_not_zero(&rri->rh, &rri->rl, sah, sal, cah, cal, yh,
                                   yl, ts, tc);
      } else {
        gpga_trigo_do_cos_not_zero(&rri->rh, &rri->rl, sah, sal, cah, cal, yh,
                                   yl, ts, tc);
      }
      rri->changesign = (quadrant == 1) || (quadrant == 2);
      return;
    case GPGA_TRIGO_TAN:
      rri->changesign = quadrant & 1;
      if (quadrant & 1) {
        gpga_trigo_do_sin_not_zero(&ch, &cl, sah, sal, cah, cal, yh, yl, ts,
                                   tc);
        gpga_trigo_do_cos_not_zero(&sh, &sl, sah, sal, cah, cal, yh, yl, ts,
                                   tc);
      } else {
        gpga_trigo_do_sin_not_zero(&sh, &sl, sah, sal, cah, cal, yh, yl, ts,
                                   tc);
        gpga_trigo_do_cos_not_zero(&ch, &cl, sah, sal, cah, cal, yh, yl, ts,
                                   tc);
      }
      Div22(&rri->rh, &rri->rl, sh, sl, ch, cl);
      return;
  }
}

inline gpga_double gpga_sin_rn(gpga_double x) {
  gpga_rrinfo rri;
  gpga_double r = gpga_double_zero(0u);
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;
  rri.changesign = 0;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_SIN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_SIN) {
      return x;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double ts = gpga_double_mul(
        x2, gpga_double_add(
                  trigo_fast_s3,
                  gpga_double_mul(x2, gpga_double_add(
                                            trigo_fast_s5,
                                            gpga_double_mul(x2,
                                                            trigo_fast_s7)))));
    Add12(&rri.rh, &rri.rl, x, gpga_double_mul(ts, x));
    gpga_double check =
        gpga_double_add(rri.rh, gpga_double_mul(rri.rl, RN_CST_SIN_CASE2));
    if (gpga_double_eq(rri.rh, check)) {
      return rri.rh;
    }
    return scs_sin_rn(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_SIN;
  gpga_compute_trig_with_argred(&rri);
  r = rri.changesign ? gpga_double_neg(rri.rh) : rri.rh;
  gpga_double check =
      gpga_double_add(rri.rh, gpga_double_mul(rri.rl, RN_CST_SINCOS_CASE3));
  if (gpga_double_eq(rri.rh, check)) {
    return r;
  }
  return scs_sin_rn(x);
}

inline gpga_double gpga_sin_ru(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_SIN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_SIN) {
      if (gpga_double_ge(x, gpga_double_zero(0u))) {
        return x;
      }
      if (gpga_double_is_zero(x)) {
        return x;
      }
      return x - 1ull;
    }
    gpga_double xx = gpga_double_mul(x, x);
    gpga_double ts = gpga_double_mul(
        x, gpga_double_mul(xx, gpga_double_add(
                                   trigo_fast_s3,
                                   gpga_double_mul(
                                       xx, gpga_double_add(
                                               trigo_fast_s5,
                                               gpga_double_mul(
                                                   xx, trigo_fast_s7))))));
    Add12(&rri.rh, &rri.rl, x, ts);
    epsilon = EPS_SIN_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_SIN;
    gpga_compute_trig_with_argred(&rri);
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
    epsilon = EPS_SINCOS_CASE3;
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_sin_ru(x);
}

inline gpga_double gpga_sin_rd(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_SIN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_SIN) {
      if (gpga_double_le(x, gpga_double_zero(0u))) {
        return x;
      }
      if (gpga_double_is_zero(x)) {
        return x;
      }
      return x - 1ull;
    }
    gpga_double xx = gpga_double_mul(x, x);
    gpga_double ts = gpga_double_mul(
        x, gpga_double_mul(xx, gpga_double_add(
                                   trigo_fast_s3,
                                   gpga_double_mul(
                                       xx, gpga_double_add(
                                               trigo_fast_s5,
                                               gpga_double_mul(
                                                   xx, trigo_fast_s7))))));
    Add12(&rri.rh, &rri.rl, x, ts);
    epsilon = EPS_SIN_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_SIN;
    gpga_compute_trig_with_argred(&rri);
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
    epsilon = EPS_SINCOS_CASE3;
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_sin_rd(x);
}

inline gpga_double gpga_sin_rz(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_SIN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_SIN) {
      return x;
    }
    gpga_double xx = gpga_double_mul(x, x);
    gpga_double ts = gpga_double_mul(
        x, gpga_double_mul(xx, gpga_double_add(
                                   trigo_fast_s3,
                                   gpga_double_mul(
                                       xx, gpga_double_add(
                                               trigo_fast_s5,
                                               gpga_double_mul(
                                                   xx, trigo_fast_s7))))));
    Add12(&rri.rh, &rri.rl, x, ts);
    epsilon = EPS_SIN_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_SIN;
    gpga_compute_trig_with_argred(&rri);
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
    epsilon = EPS_SINCOS_CASE3;
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_sin_rz(x);
}

inline gpga_double gpga_cos_rn(gpga_double x) {
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;
  rri.changesign = 0;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_COS_CASE2) {
    if (absxhi < XMAX_RETURN_1_FOR_COS_RN) {
      return gpga_double_from_u32(1u);
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double tc = gpga_double_mul(
        x2, gpga_double_add(
                  trigo_fast_c2,
                  gpga_double_mul(x2, gpga_double_add(
                                            trigo_fast_c4,
                                            gpga_double_mul(x2,
                                                            trigo_fast_c6)))));
    Add12(&rri.rh, &rri.rl, gpga_double_from_u32(1u), tc);
    gpga_double check =
        gpga_double_add(rri.rh, gpga_double_mul(rri.rl, RN_CST_COS_CASE2));
    if (gpga_double_eq(rri.rh, check)) {
      return rri.rh;
    }
    return scs_cos_rn(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_COS;
  gpga_compute_trig_with_argred(&rri);
  gpga_double check =
      gpga_double_add(rri.rh, gpga_double_mul(rri.rl, RN_CST_SINCOS_CASE3));
  if (gpga_double_eq(rri.rh, check)) {
    return rri.changesign ? gpga_double_neg(rri.rh) : rri.rh;
  }
  return scs_cos_rn(x);
}

inline gpga_double gpga_cos_ru(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_COS_CASE2) {
    if (absxhi < XMAX_RETURN_1_FOR_COS_RDIR) {
      return gpga_double_from_u32(1u);
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double tc = gpga_double_mul(
        x2, gpga_double_add(
                  trigo_fast_c2,
                  gpga_double_mul(x2, gpga_double_add(
                                            trigo_fast_c4,
                                            gpga_double_mul(x2,
                                                            trigo_fast_c6)))));
    Add12(&rri.rh, &rri.rl, gpga_double_from_u32(1u), tc);
    epsilon = EPS_COS_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_COS;
    gpga_compute_trig_with_argred(&rri);
    epsilon = EPS_SINCOS_CASE3;
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_cos_ru(x);
}

inline gpga_double gpga_cos_rd(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_COS_CASE2) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    if (absxhi < XMAX_RETURN_1_FOR_COS_RDIR) {
      return ONE_ROUNDED_DOWN;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double tc = gpga_double_mul(
        x2, gpga_double_add(
                  trigo_fast_c2,
                  gpga_double_mul(x2, gpga_double_add(
                                            trigo_fast_c4,
                                            gpga_double_mul(x2,
                                                            trigo_fast_c6)))));
    Add12(&rri.rh, &rri.rl, gpga_double_from_u32(1u), tc);
    epsilon = EPS_COS_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_COS;
    gpga_compute_trig_with_argred(&rri);
    epsilon = EPS_SINCOS_CASE3;
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_cos_rd(x);
}

inline gpga_double gpga_cos_rz(gpga_double x) {
  gpga_double epsilon = EPS_SINCOS_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_COS_CASE2) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    if (absxhi < XMAX_RETURN_1_FOR_COS_RDIR) {
      return ONE_ROUNDED_DOWN;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double tc = gpga_double_mul(
        x2, gpga_double_add(
                  trigo_fast_c2,
                  gpga_double_mul(x2, gpga_double_add(
                                            trigo_fast_c4,
                                            gpga_double_mul(x2,
                                                            trigo_fast_c6)))));
    Add12(&rri.rh, &rri.rl, gpga_double_from_u32(1u), tc);
    epsilon = EPS_COS_CASE2;
  } else {
    rri.x = x;
    rri.function = GPGA_TRIGO_COS;
    gpga_compute_trig_with_argred(&rri);
    epsilon = EPS_SINCOS_CASE3;
    if (rri.changesign) {
      rri.rh = gpga_double_neg(rri.rh);
      rri.rl = gpga_double_neg(rri.rl);
    }
  }

  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_cos_rz(x);
}

inline gpga_double gpga_tan_rn(gpga_double x) {
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_TAN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_TAN) {
      return x;
    }
    uint exp_bits = absxhi >> 20;
    uint mant = (absxhi & 0x000fffffu) + 0x00100000u;
    uint shift = (uint)((0x3ff + 2) - exp_bits);
    uint hi = 0x3ff00000u + (mant >> shift);
    gpga_double rndcst = ((ulong)hi << 32) | 0xfffffffful;
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double p5 = gpga_double_add(
        trigo_fast_t5,
        gpga_double_mul(x2, gpga_double_add(
                               trigo_fast_t7,
                               gpga_double_mul(
                                   x2, gpga_double_add(
                                           trigo_fast_t9,
                                           gpga_double_mul(x2,
                                                           trigo_fast_t11))))));
    gpga_double tt = gpga_double_mul(
        x2, gpga_double_add(trigo_fast_t3h,
                            gpga_double_add(trigo_fast_t3l,
                                            gpga_double_mul(x2, p5))));
    Add12(&rri.rh, &rri.rl, x, gpga_double_mul(x, tt));
    gpga_double check =
        gpga_double_add(rri.rh, gpga_double_mul(rri.rl, rndcst));
    if (gpga_double_eq(rri.rh, check)) {
      return rri.rh;
    }
    return scs_tan_rn(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_TAN;
  gpga_compute_trig_with_argred(&rri);
  gpga_double check =
      gpga_double_add(rri.rh, gpga_double_mul(rri.rl, RN_CST_TAN_CASE3));
  if (gpga_double_eq(rri.rh, check)) {
    return rri.changesign ? gpga_double_neg(rri.rh) : rri.rh;
  }
  return scs_tan_rn(x);
}

inline gpga_double gpga_tan_ru(gpga_double x) {
  gpga_double epsilon = EPS_TAN_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_TAN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_TAN) {
      if (gpga_double_le(x, gpga_double_zero(0u))) {
        return x;
      }
      if (gpga_double_is_zero(x)) {
        return x;
      }
      return x + 1ull;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double p5 = gpga_double_add(
        trigo_fast_t5,
        gpga_double_mul(x2, gpga_double_add(
                               trigo_fast_t7,
                               gpga_double_mul(
                                   x2, gpga_double_add(
                                           trigo_fast_t9,
                                           gpga_double_mul(x2,
                                                           trigo_fast_t11))))));
    gpga_double tt = gpga_double_mul(
        x2, gpga_double_add(trigo_fast_t3h,
                            gpga_double_add(trigo_fast_t3l,
                                            gpga_double_mul(x2, p5))));
    Add12(&rri.rh, &rri.rl, x, gpga_double_mul(x, tt));
    gpga_double out = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(rri.rh, rri.rl, EPS_TAN_CASE2, &out)) {
      return out;
    }
    return scs_tan_ru(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_TAN;
  gpga_compute_trig_with_argred(&rri);
  epsilon = EPS_TAN_CASE3;
  if (rri.changesign) {
    rri.rh = gpga_double_neg(rri.rh);
    rri.rl = gpga_double_neg(rri.rl);
  }
  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_tan_ru(x);
}

inline gpga_double gpga_tan_rd(gpga_double x) {
  gpga_double epsilon = EPS_TAN_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_TAN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_TAN) {
      if (gpga_double_ge(x, gpga_double_zero(0u))) {
        return x;
      }
      if (gpga_double_is_zero(x)) {
        return x;
      }
      return x + 1ull;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double p5 = gpga_double_add(
        trigo_fast_t5,
        gpga_double_mul(x2, gpga_double_add(
                               trigo_fast_t7,
                               gpga_double_mul(
                                   x2, gpga_double_add(
                                           trigo_fast_t9,
                                           gpga_double_mul(x2,
                                                           trigo_fast_t11))))));
    gpga_double tt = gpga_double_mul(
        x2, gpga_double_add(trigo_fast_t3h,
                            gpga_double_add(trigo_fast_t3l,
                                            gpga_double_mul(x2, p5))));
    Add12(&rri.rh, &rri.rl, x, gpga_double_mul(x, tt));
    gpga_double out = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(rri.rh, rri.rl, EPS_TAN_CASE2, &out)) {
      return out;
    }
    return scs_tan_rd(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_TAN;
  gpga_compute_trig_with_argred(&rri);
  epsilon = EPS_TAN_CASE3;
  if (rri.changesign) {
    rri.rh = gpga_double_neg(rri.rh);
    rri.rl = gpga_double_neg(rri.rl);
  }
  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_tan_rd(x);
}

inline gpga_double gpga_tan_rz(gpga_double x) {
  gpga_double epsilon = EPS_TAN_CASE3;
  gpga_rrinfo rri;
  uint absxhi = gpga_u64_hi(x) & 0x7fffffffU;
  rri.absxhi = (int)absxhi;

  if (absxhi >= 0x7ff00000u) {
    return gpga_double_nan();
  }

  if (absxhi < XMAX_TAN_CASE2) {
    if (absxhi < XMAX_RETURN_X_FOR_TAN) {
      return x;
    }
    gpga_double x2 = gpga_double_mul(x, x);
    gpga_double p5 = gpga_double_add(
        trigo_fast_t5,
        gpga_double_mul(x2, gpga_double_add(
                               trigo_fast_t7,
                               gpga_double_mul(
                                   x2, gpga_double_add(
                                           trigo_fast_t9,
                                           gpga_double_mul(x2,
                                                           trigo_fast_t11))))));
    gpga_double tt = gpga_double_mul(
        x2, gpga_double_add(trigo_fast_t3h,
                            gpga_double_add(trigo_fast_t3l,
                                            gpga_double_mul(x2, p5))));
    Add12(&rri.rh, &rri.rl, x, gpga_double_mul(x, tt));
    gpga_double out = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(rri.rh, rri.rl, EPS_TAN_CASE2, &out)) {
      return out;
    }
    return scs_tan_rz(x);
  }

  rri.x = x;
  rri.function = GPGA_TRIGO_TAN;
  gpga_compute_trig_with_argred(&rri);
  epsilon = EPS_TAN_CASE3;
  if (rri.changesign) {
    rri.rh = gpga_double_neg(rri.rh);
    rri.rl = gpga_double_neg(rri.rl);
  }
  gpga_double out = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(rri.rh, rri.rl, epsilon, &out)) {
    return out;
  }
  return scs_tan_rz(x);
}

inline gpga_double scs_double_pow2_bits(int exp_bits) {
  if (exp_bits <= 0) {
    return gpga_double_zero(0u);
  }
  if (exp_bits >= 0x7ff) {
    return gpga_double_inf(0u);
  }
  return gpga_double_pack(0u, (uint)exp_bits, 0ul);
}

inline gpga_double scs_radix_one_double() {
  return scs_double_pow2_bits(1023 + SCS_NB_BITS);
}

inline gpga_double scs_radix_two_double() {
  return scs_double_pow2_bits(1023 + 2 * SCS_NB_BITS);
}

inline gpga_double scs_radix_mone_double() {
  return scs_double_pow2_bits(1023 - SCS_NB_BITS);
}

inline gpga_double scs_radix_mtwo_double() {
  return scs_double_pow2_bits(1023 - 2 * SCS_NB_BITS);
}

inline gpga_double scs_radix_rng_double() {
  return scs_double_pow2_bits(1023 + (SCS_NB_BITS * SCS_MAX_RANGE));
}

inline gpga_double scs_radix_mrng_double() {
  return scs_double_pow2_bits(1023 - (SCS_NB_BITS * SCS_MAX_RANGE));
}

inline gpga_double scs_max_double() {
  return gpga_bits_to_real(0x7feffffffffffffful);
}

inline gpga_double scs_min_double() {
  return gpga_bits_to_real(0x0000000000000001ul);
}

inline void scs_zero(scs_ptr result) {
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    R_HW[i] = 0u;
  }
  R_EXP = gpga_double_zero(0u);
  R_IND = 0;
  R_SGN = 1;
}

inline void scs_set(scs_ptr result, scs_ptr x) {
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    R_HW[i] = X_HW[i];
  }
  R_EXP = X_EXP;
  R_IND = X_IND;
  R_SGN = X_SGN;
}

inline void scs_set_si(scs_ptr result, int x) {
  uint ux = 0u;
  if (x >= 0) {
    R_SGN = 1;
    ux = (uint)x;
  } else {
    R_SGN = -1;
    ux = (uint)(-x);
  }

  if (ux > SCS_RADIX) {
    R_IND = 1;
    R_HW[0] = (ux - SCS_RADIX) >> SCS_NB_BITS;
    R_HW[1] = ux - (R_HW[0] << SCS_NB_BITS);
  } else {
    R_IND = 0;
    R_HW[0] = ux;
    R_HW[1] = 0;
  }

  for (int i = 2; i < SCS_NB_WORDS; ++i) {
    R_HW[i] = 0;
  }

  if (x != 0) {
    R_EXP = gpga_double_from_u32(1u);
  } else {
    R_EXP = gpga_double_zero(0u);
  }
}

inline void scs_set_d(scs_ptr result, gpga_double x) {
  gpga_double nb = gpga_double_abs(x);
  if (gpga_double_is_zero(nb)) {
    scs_zero(result);
    return;
  }
  if (gpga_double_sign(x) != 0u) {
    R_SGN = -1;
  } else {
    R_SGN = 1;
  }

  uint exponent_bits = gpga_double_exp(nb);
  if (exponent_bits == 0x7ffu) {
    R_EXP = x;
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = 0u;
    }
    R_IND = 0;
    R_SGN = 1;
    return;
  }

  R_EXP = gpga_double_from_u32(1u);
  int ind_offset = 0;
  if (exponent_bits == 0u) {
    nb = gpga_double_mul(nb, scs_radix_two_double());
    exponent_bits = gpga_double_exp(nb);
    ind_offset = -2;
  }

  int exponent = (int)exponent_bits;
  int ind = ((exponent + (100 * SCS_NB_BITS) - 1023) / SCS_NB_BITS) - 100;
  int exponent_remainder = exponent - 1022 - (SCS_NB_BITS * ind);
  R_IND = ind_offset + ind;

  ulong mantissa = (nb & 0x000ffffffffffffful) | 0x0010000000000000ul;
  R_HW[0] = (uint)(mantissa >> (53 - exponent_remainder));
  mantissa = mantissa << (exponent_remainder + 11);
  R_HW[1] = (gpga_u64_hi(mantissa) >> (32 - SCS_NB_BITS)) & SCS_MASK_RADIX;
  mantissa = mantissa << SCS_NB_BITS;
  R_HW[2] = (gpga_u64_hi(mantissa) >> (32 - SCS_NB_BITS)) & SCS_MASK_RADIX;
  if (SCS_NB_BITS < 27) {
    mantissa = mantissa << SCS_NB_BITS;
    R_HW[3] = (gpga_u64_hi(mantissa) >> (32 - SCS_NB_BITS)) & SCS_MASK_RADIX;
  } else {
    R_HW[3] = 0;
  }
  for (int i = 4; i < SCS_NB_WORDS; ++i) {
    R_HW[i] = 0u;
  }
}

inline void scs_get_d(thread gpga_double* result, scs_ptr x) {
  if (X_EXP != gpga_double_from_u32(1u)) {
    *result = X_EXP;
    return;
  }
  gpga_double nb = gpga_double_from_u32(X_HW[0]);
  ulong lowpart = ((ulong)X_HW[1] << SCS_NB_BITS) + (ulong)X_HW[2];
  int expo = (int)gpga_double_exp(nb) - 1023;
  int expofinal = expo + SCS_NB_BITS * X_IND;

  gpga_double res = gpga_double_zero(0u);
  if (expofinal > 1023) {
    res = gpga_double_mul(scs_radix_rng_double(), scs_radix_rng_double());
  } else if (expofinal >= -1022) {
    int shift = expo + (2 * SCS_NB_BITS) - 53;
    ulong roundbits = lowpart << (64 - shift);
    lowpart = lowpart >> shift;
    gpga_double rndcorr = gpga_double_zero(0u);
    if ((lowpart & 1ull) != 0ull) {
      if (roundbits == 0ull) {
        for (int i = 3; i < SCS_NB_WORDS; ++i) {
          roundbits |= (ulong)X_HW[i];
        }
      }
      if (roundbits == 0ull) {
        if ((lowpart & 2ull) != 0ull) {
          rndcorr = scs_double_pow2_bits(expo - 52 + 1023);
        }
      } else {
        rndcorr = scs_double_pow2_bits(expo - 52 + 1023);
      }
    }
    lowpart = lowpart >> 1;
    nb |= lowpart;
    res = gpga_double_add(nb, rndcorr);
    int scale_exp = (X_IND * SCS_NB_BITS) + 1023;
    if (scale_exp > 0) {
      gpga_double scale = scs_double_pow2_bits(scale_exp);
      res = gpga_double_mul(res, scale);
    } else {
      gpga_double scale = scs_double_pow2_bits(scale_exp + 2 * SCS_NB_BITS);
      res = gpga_double_mul(res, scs_radix_mtwo_double());
      res = gpga_double_mul(res, scale);
    }
  } else {
    if (expofinal < -1022 - 53) {
      res = gpga_double_zero(0u);
    } else {
      lowpart = lowpart >> (expo + (2 * SCS_NB_BITS) - 52);
      nb |= lowpart;
      nb = (nb & 0x000ffffffffffffful) | 0x0010000000000000ul;
      nb = nb >> (-1023 - expofinal);
      gpga_double rndcorr = gpga_double_zero(0u);
      if ((gpga_u64_lo(nb) & 0x1u) != 0u) {
        rndcorr = gpga_bits_to_real(1ul);
      }
      res = gpga_double_mul(gpga_double_const_inv2(),
                            gpga_double_add(nb, rndcorr));
    }
  }

  if (X_SGN < 0) {
    *result = gpga_double_neg(res);
  } else {
    *result = res;
  }
}

inline void scs_get_d_directed(thread gpga_double* result, scs_ptr x,
                               int rnd_mantissa_up) {
  if (X_EXP != gpga_double_from_u32(1u)) {
    *result = X_EXP;
    return;
  }
  gpga_double nb = gpga_double_from_u32(X_HW[0]);
  ulong lowpart = ((ulong)X_HW[1] << SCS_NB_BITS) + (ulong)X_HW[2];
  int expo = (int)gpga_double_exp(nb) - 1023;
  int expofinal = expo + SCS_NB_BITS * X_IND;
  int not_null =
      ((lowpart << (64 + 52 - 2 * SCS_NB_BITS - expo)) != 0ull);
  for (int i = 3; i < SCS_NB_WORDS; ++i) {
    if (X_HW[i] != 0u) {
      not_null = 1;
    }
  }

  gpga_double res = gpga_double_zero(0u);
  if (expofinal > 1023) {
    if (rnd_mantissa_up) {
      res = gpga_double_mul(scs_radix_rng_double(), scs_radix_rng_double());
    } else {
      res = scs_max_double();
    }
  } else if (expofinal >= -1022) {
    lowpart = lowpart >> (expo + (2 * SCS_NB_BITS) - 52);
    nb |= lowpart;
    gpga_double rndcorr = gpga_double_zero(0u);
    if (rnd_mantissa_up && not_null) {
      rndcorr = scs_double_pow2_bits(expo - 52 + 1023);
    }
    res = gpga_double_add(nb, rndcorr);
    int scale_exp = (X_IND * SCS_NB_BITS) + 1023;
    if (scale_exp > 0) {
      gpga_double scale = scs_double_pow2_bits(scale_exp);
      res = gpga_double_mul(res, scale);
    } else {
      gpga_double scale = scs_double_pow2_bits(scale_exp + 2 * SCS_NB_BITS);
      res = gpga_double_mul(res, scs_radix_mtwo_double());
      res = gpga_double_mul(res, scale);
    }
  } else {
    if (expofinal < -1022 - 53) {
      res = rnd_mantissa_up ? scs_min_double() : gpga_double_zero(0u);
    } else {
      lowpart = lowpart >> (expo + (2 * SCS_NB_BITS) - 52);
      nb |= lowpart;
      nb = (nb & 0x000ffffffffffffful) | 0x0010000000000000ul;
      if (rnd_mantissa_up && not_null) {
        nb = nb >> (-1022 - expofinal);
        nb = nb + 1ul;
      } else {
        nb = nb >> (-1022 - expofinal);
      }
      res = nb;
    }
  }

  if (X_SGN < 0) {
    *result = gpga_double_neg(res);
  } else {
    *result = res;
  }
}

inline void scs_get_d_minf(thread gpga_double* result, scs_ptr x) {
  scs_get_d_directed(result, x, (int)(X_SGN < 0));
}

inline void scs_get_d_pinf(thread gpga_double* result, scs_ptr x) {
  scs_get_d_directed(result, x, (int)(X_SGN >= 0));
}

inline void scs_get_d_zero(thread gpga_double* result, scs_ptr x) {
  scs_get_d_directed(result, x, 0);
}

inline void scs_renorm(scs_ptr result) {
  for (int i = SCS_NB_WORDS - 1; i > 0; --i) {
    uint c = R_HW[i] & ~SCS_MASK_RADIX;
    R_HW[i - 1] += c >> SCS_NB_BITS;
    R_HW[i] = R_HW[i] & SCS_MASK_RADIX;
  }
  if (R_HW[0] >= SCS_RADIX) {
    uint c = R_HW[0] >> SCS_NB_BITS;
    for (int i = SCS_NB_WORDS - 1; i > 1; --i) {
      R_HW[i] = R_HW[i - 1];
    }
    R_HW[1] = R_HW[0] & SCS_MASK_RADIX;
    R_HW[0] = c;
    R_IND += 1;
  } else if (R_HW[0] == 0u) {
    int k = 1;
    while (k < SCS_NB_WORDS && R_HW[k] == 0u) {
      ++k;
    }
    R_IND -= k;
    int i = 0;
    for (int j = k; j < SCS_NB_WORDS; ++j, ++i) {
      R_HW[i] = R_HW[j];
    }
    for (; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = 0u;
    }
  }
}

inline void scs_renorm_no_cancel_check(scs_ptr result) {
  for (int i = SCS_NB_WORDS - 1; i > 0; --i) {
    uint carry = R_HW[i] >> SCS_NB_BITS;
    R_HW[i - 1] += carry;
    R_HW[i] &= SCS_MASK_RADIX;
  }
  if (R_HW[0] >= SCS_RADIX) {
    uint c0 = R_HW[0] >> SCS_NB_BITS;
    for (int i = SCS_NB_WORDS - 1; i > 1; --i) {
      R_HW[i] = R_HW[i - 1];
    }
    R_HW[1] = R_HW[0] & SCS_MASK_RADIX;
    R_HW[0] = c0;
    R_IND += 1;
  }
}

inline void scs_add_no_renorm(scs_ptr result, scs_ptr x, scs_ptr y) {
  scs_ptr ax = x;
  scs_ptr ay = y;
  if (ax->index < ay->index) {
    scs_ptr tmp = ax;
    ax = ay;
    ay = tmp;
  }
  uint res[SCS_NB_WORDS];
  uint diff = (uint)(ax->index - ay->index);
  result->exception =
      gpga_double_sub(gpga_double_add(ax->exception, ay->exception),
                      gpga_double_from_u32(1u));
  result->index = ax->index;
  result->sign = ax->sign;
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    res[i] = ax->h_word[i];
  }
  for (uint i = diff, j = 0; i < SCS_NB_WORDS; ++i, ++j) {
    res[i] += ay->h_word[j];
  }
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    result->h_word[i] = res[i];
  }
}

inline void scs_do_add(scs_ptr result, scs_ptr x, scs_ptr y) {
  int diff = X_IND - Y_IND;
  R_EXP = gpga_double_sub(gpga_double_add(X_EXP, Y_EXP),
                          gpga_double_from_u32(1u));
  R_IND = X_IND;
  R_SGN = X_SGN;
  if (diff >= SCS_NB_WORDS) {
    scs_set(result, x);
    return;
  }
  int carry = 0;
  int res[SCS_NB_WORDS];
  for (int i = SCS_NB_WORDS - 1, j = (SCS_NB_WORDS - 1) - diff; i >= 0;
       --i, --j) {
    int s = (j >= 0) ? (int)X_HW[i] + (int)Y_HW[j] + carry
                     : (int)X_HW[i] + carry;
    carry = s >> SCS_NB_BITS;
    res[i] = s & SCS_MASK_RADIX;
  }
  if (carry) {
    for (int i = SCS_NB_WORDS - 1; i >= 1; --i) {
      R_HW[i] = (uint)res[i - 1];
    }
    R_HW[0] = 1u;
    R_IND += 1;
  } else {
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i];
    }
  }
}

inline void scs_do_sub(scs_ptr result, scs_ptr x, scs_ptr y) {
  int diff = X_IND - Y_IND;
  R_EXP = gpga_double_sub(gpga_double_add(X_EXP, Y_EXP),
                          gpga_double_from_u32(1u));
  R_IND = X_IND;

  if (diff >= SCS_NB_WORDS) {
    scs_set(result, x);
    return;
  }

  int res[SCS_NB_WORDS];
  int carry = 0;
  if (diff == 0) {
    int i = 0;
    while (i < SCS_NB_WORDS && X_HW[i] == Y_HW[i]) {
      ++i;
    }
    int cp = 0;
    if (i < SCS_NB_WORDS) {
      if (X_HW[i] > Y_HW[i]) {
        cp = 1;
      } else if (X_HW[i] < Y_HW[i]) {
        cp = -1;
      }
    }
    if (cp == 0) {
      scs_zero(result);
      return;
    }
    if (cp > 0) {
      R_SGN = X_SGN;
      for (int k = SCS_NB_WORDS - 1; k >= 0; --k) {
        int s = (int)X_HW[k] - (int)Y_HW[k] - carry;
        carry = (int)(((uint)s & SCS_RADIX) >> SCS_NB_BITS);
        res[k] = (int)(((uint)s & SCS_RADIX) + s);
      }
    } else {
      R_SGN = -X_SGN;
      for (int k = SCS_NB_WORDS - 1; k >= 0; --k) {
        int s = -(int)X_HW[k] + (int)Y_HW[k] - carry;
        carry = (int)(((uint)s & SCS_RADIX) >> SCS_NB_BITS);
        res[k] = (int)(((uint)s & SCS_RADIX) + s);
      }
    }
  } else {
    R_SGN = X_SGN;
    for (int i = SCS_NB_WORDS - 1, j = (SCS_NB_WORDS - 1) - diff; i >= 0;
         --i, --j) {
      int s = (j >= 0) ? (int)X_HW[i] - (int)Y_HW[j] - carry
                       : (int)X_HW[i] - carry;
      carry = (int)(((uint)s & SCS_RADIX) >> SCS_NB_BITS);
      res[i] = (int)(((uint)s & SCS_RADIX) + s);
    }
  }

  int shift = 0;
  while (shift < SCS_NB_WORDS && res[shift] == 0) {
    ++shift;
  }
  if (shift > 0) {
    R_IND -= shift;
    int j = 0;
    for (int i = shift; i < SCS_NB_WORDS; ++i, ++j) {
      R_HW[j] = (uint)res[i];
    }
    for (; j < SCS_NB_WORDS; ++j) {
      R_HW[j] = 0u;
    }
  } else {
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i];
    }
  }
}

inline void scs_add(scs_ptr result, scs_ptr x, scs_ptr y) {
  if (X_EXP == gpga_double_zero(0u)) {
    scs_set(result, y);
    return;
  }
  if (Y_EXP == gpga_double_zero(0u)) {
    scs_set(result, x);
    return;
  }
  if (X_SGN == Y_SGN) {
    if (X_IND >= Y_IND) {
      scs_do_add(result, x, y);
    } else {
      scs_do_add(result, y, x);
    }
  } else {
    if (X_IND >= Y_IND) {
      scs_do_sub(result, x, y);
    } else {
      scs_do_sub(result, y, x);
    }
  }
}

inline void scs_sub(scs_ptr result, scs_ptr x, scs_ptr y) {
  if (X_EXP == gpga_double_zero(0u)) {
    scs_set(result, y);
    R_SGN = -R_SGN;
    return;
  }
  if (Y_EXP == gpga_double_zero(0u)) {
    scs_set(result, x);
    return;
  }
  if (X_SGN == Y_SGN) {
    if (X_IND >= Y_IND) {
      scs_do_sub(result, x, y);
    } else {
      scs_do_sub(result, y, x);
      R_SGN = -R_SGN;
    }
  } else {
    if (X_IND >= Y_IND) {
      scs_do_add(result, x, y);
    } else {
      scs_do_add(result, y, x);
      R_SGN = -R_SGN;
    }
  }
}

inline void scs_mul(scs_ptr result, scs_ptr x, scs_ptr y) {
  uint64_t res[SCS_NB_WORDS + 1];
  for (int i = 0; i <= SCS_NB_WORDS; ++i) {
    res[i] = 0;
  }
  R_EXP = gpga_double_mul(X_EXP, Y_EXP);
  R_SGN = X_SGN * Y_SGN;
  R_IND = X_IND + Y_IND;

  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    uint64_t tmp = X_HW[i];
    for (int j = 0; j < SCS_NB_WORDS - i; ++j) {
      res[i + j] += tmp * (uint64_t)Y_HW[j];
    }
    if (i > 0) {
      res[SCS_NB_WORDS] += tmp * (uint64_t)Y_HW[SCS_NB_WORDS - i];
    }
  }

  uint64_t val = 0;
  uint64_t tmp = 0;
  for (int i = SCS_NB_WORDS; i > 0; --i) {
    SCS_CARRY_PROPAGATE(res[i], res[i - 1], tmp);
  }
  SCS_CARRY_PROPAGATE(res[0], val, tmp);

  if (val != 0u) {
    R_HW[0] = (uint)val;
    for (int i = 1; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i - 1];
    }
    R_IND += 1;
  } else {
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i];
    }
  }
}

inline void scs_add(scs_ptr result, scs_const_ptr x, scs_ptr y) {
  scs tmp = *x;
  scs_add(result, (scs_ptr)&tmp, y);
}

inline void scs_add(scs_ptr result, scs_ptr x, scs_const_ptr y) {
  scs tmp = *y;
  scs_add(result, x, (scs_ptr)&tmp);
}

inline void scs_add(scs_ptr result, scs_const_ptr x, scs_const_ptr y) {
  scs tmp_x = *x;
  scs tmp_y = *y;
  scs_add(result, (scs_ptr)&tmp_x, (scs_ptr)&tmp_y);
}

inline void scs_sub(scs_ptr result, scs_const_ptr x, scs_ptr y) {
  scs tmp = *x;
  scs_sub(result, (scs_ptr)&tmp, y);
}

inline void scs_sub(scs_ptr result, scs_ptr x, scs_const_ptr y) {
  scs tmp = *y;
  scs_sub(result, x, (scs_ptr)&tmp);
}

inline void scs_sub(scs_ptr result, scs_const_ptr x, scs_const_ptr y) {
  scs tmp_x = *x;
  scs tmp_y = *y;
  scs_sub(result, (scs_ptr)&tmp_x, (scs_ptr)&tmp_y);
}

inline void scs_mul(scs_ptr result, scs_const_ptr x, scs_ptr y) {
  scs tmp = *x;
  scs_mul(result, (scs_ptr)&tmp, y);
}

inline void scs_mul(scs_ptr result, scs_ptr x, scs_const_ptr y) {
  scs tmp = *y;
  scs_mul(result, x, (scs_ptr)&tmp);
}

inline void scs_mul(scs_ptr result, scs_const_ptr x, scs_const_ptr y) {
  scs tmp_x = *x;
  scs tmp_y = *y;
  scs_mul(result, (scs_ptr)&tmp_x, (scs_ptr)&tmp_y);
}

inline void scs_square(scs_ptr result, scs_ptr x) {
  uint64_t res[SCS_NB_WORDS + 1];
  for (int i = 0; i <= SCS_NB_WORDS; ++i) {
    res[i] = 0;
  }
  R_EXP = gpga_double_mul(X_EXP, X_EXP);
  R_SGN = 1;
  R_IND = X_IND + X_IND;

  uint64_t tmp = X_HW[0];
  for (int j = 1; j < SCS_NB_WORDS; ++j) {
    res[j] += tmp * (uint64_t)X_HW[j];
  }
  for (int i = 1; i < (SCS_NB_WORDS + 1) / 2; ++i) {
    tmp = X_HW[i];
    for (int j = i + 1; j < (SCS_NB_WORDS - i); ++j) {
      res[i + j] += tmp * (uint64_t)X_HW[j];
    }
    res[SCS_NB_WORDS] += tmp * (uint64_t)X_HW[SCS_NB_WORDS - i];
  }
  for (int i = 0; i <= SCS_NB_WORDS; ++i) {
    res[i] *= 2u;
  }
  for (int i = 0, j = 0; i <= SCS_NB_WORDS; i += 2, ++j) {
    res[i] += (uint64_t)X_HW[j] * (uint64_t)X_HW[j];
  }

  uint64_t val = 0;
  for (int i = SCS_NB_WORDS; i > 0; --i) {
    uint64_t tmp2 = 0;
    SCS_CARRY_PROPAGATE(res[i], res[i - 1], tmp2);
  }
  uint64_t tmp3 = 0;
  SCS_CARRY_PROPAGATE(res[0], val, tmp3);
  if (val != 0u) {
    R_HW[0] = (uint)val;
    for (int i = 1; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i - 1];
    }
    R_IND += 1;
  } else {
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      R_HW[i] = (uint)res[i];
    }
  }
}

inline void scs_mul_ui(scs_ptr x, uint val_int) {
  if (val_int == 0u) {
    X_EXP = gpga_double_zero(0u);
    return;
  }
  uint64_t val = 0;
  uint64_t rr = 0;
  uint64_t vald = val_int;
  for (int i = SCS_NB_WORDS - 1; i >= 0; --i) {
    val += vald * (uint64_t)X_HW[i];
    uint64_t tmp = 0;
    SCS_CARRY_PROPAGATE(val, rr, tmp);
    X_HW[i] = (uint)val;
    val = rr;
    rr = 0;
  }
  if (val != 0u) {
    for (int i = SCS_NB_WORDS - 1; i > 0; --i) {
      X_HW[i] = X_HW[i - 1];
    }
    X_HW[0] = (uint)val;
    X_IND += 1;
  }
}

inline void scs_div_2(scs_ptr num) {
  uint carry = 0u;
  uint mask = (uint)((1u << SCS_NB_BITS) - 1u);
  if (num->exception == gpga_double_from_u32(1u)) {
    for (int i = 0; i < SCS_NB_WORDS; ++i) {
      uint old_value = num->h_word[i];
      num->h_word[i] = carry | ((old_value >> 1) & mask);
      carry = (old_value & 0x1u) << (SCS_NB_BITS - 1);
    }
    if (num->h_word[0] == 0u) {
      num->index -= 1;
      for (int i = 1; i < SCS_NB_WORDS; ++i) {
        num->h_word[i - 1] = num->h_word[i];
      }
      num->h_word[SCS_NB_WORDS - 1] = 0u;
    }
  } else {
    num->exception = gpga_double_mul(num->exception, gpga_double_const_inv2());
  }
}

inline void scs_inv(scs_ptr result, scs_ptr x) {
  scs_t tmp;
  scs_t res;
  scs_t res1;
  scs_t scstwo;
  gpga_double app_x = gpga_double_zero(0u);
  scs_set(tmp, x);
  tmp->index = 0;
  scs_get_d(&app_x, tmp);
  scs_set_si(scstwo, 2);
  gpga_double inv = gpga_double_div(gpga_double_from_u32(1u), app_x);
  scs_set_d(res, inv);
  res->index -= x->index;

  scs_mul(res1, x, res);
  scs_sub(res1, scstwo, res1);
  scs_mul(res, res, res1);

  scs_mul(res1, x, res);
  scs_sub(res1, scstwo, res1);
  scs_mul(result, res, res1);
}

inline void scs_div(scs_ptr result, scs_ptr x, scs_ptr y) {
  scs_t res;
  if (X_EXP != gpga_double_from_u32(1u)) {
    R_EXP = gpga_double_div(X_EXP, Y_EXP);
    return;
  }
  scs_inv(res, y);
  scs_mul(result, res, x);
}

inline void scs_fma(scs_ptr result, scs_ptr x, scs_ptr y, scs_ptr z) {
  uint64_t res[SCS_NB_WORDS * 2];
  for (int i = 0; i < (SCS_NB_WORDS * 2); ++i) {
    res[i] = 0;
  }
  int ind = X_IND + Y_IND;
  for (int i = 0; i < SCS_NB_WORDS; ++i) {
    for (int j = 0; j < SCS_NB_WORDS - i; ++j) {
      res[i + j] += (uint64_t)X_HW[i] * (uint64_t)Y_HW[j];
    }
  }

  if (z->sign == (X_SGN * Y_SGN)) {
    int diff = z->index - ind;
    if (diff >= 0) {
      for (int i = SCS_NB_WORDS - 1, j = SCS_NB_WORDS - diff; j >= 0;
           --i, --j) {
        res[i] = z->h_word[i] + res[j];
      }
      for (int i = (SCS_NB_WORDS - diff) - 1; i >= 0; --i) {
        res[i] = z->h_word[i];
      }
    } else {
      for (int i = (SCS_NB_WORDS + diff), j = SCS_NB_WORDS - 1; i >= 0;
           --i, --j) {
        res[j] = z->h_word[i] + res[j];
      }
    }
    res[SCS_NB_WORDS - 1] += (res[SCS_NB_WORDS] >> SCS_NB_BITS);
    for (int i = SCS_NB_WORDS - 1; i > 0; --i) {
      uint64_t tmp = res[i] >> SCS_NB_BITS;
      res[i - 1] += tmp;
      res[i] -= (tmp << SCS_NB_BITS);
    }

    uint64_t val = res[0] >> SCS_NB_BITS;
    R_IND = X_IND + Y_IND;
    if (val != 0u) {
      R_HW[0] = (uint)val;
      R_HW[1] = (uint)(res[0] - (val << SCS_NB_BITS));
      for (int i = 2; i < SCS_NB_WORDS; ++i) {
        R_HW[i] = (uint)res[i - 1];
      }
      R_IND += 1;
    } else {
      for (int i = 0; i < SCS_NB_WORDS; ++i) {
        R_HW[i] = (uint)res[i];
      }
    }
    R_EXP = gpga_double_sub(
        gpga_double_add(z->exception, gpga_double_mul(X_EXP, Y_EXP)),
        gpga_double_from_u32(1u));
    R_SGN = X_SGN * Y_SGN;
  } else {
    res[SCS_NB_WORDS - 1] += (res[SCS_NB_WORDS] >> SCS_NB_BITS);
    for (int i = SCS_NB_WORDS - 1; i > 0; --i) {
      uint64_t tmp = res[i] >> SCS_NB_BITS;
      res[i - 1] += tmp;
      res[i] -= (tmp << SCS_NB_BITS);
    }
    uint64_t val = res[0] >> SCS_NB_BITS;
    R_IND = X_IND + Y_IND;
    if (val != 0u) {
      R_HW[0] = (uint)val;
      R_HW[1] = (uint)(res[0] - (val << SCS_NB_BITS));
      for (int i = 2; i < SCS_NB_WORDS; ++i) {
        R_HW[i] = (uint)res[i - 1];
      }
      R_IND += 1;
    } else {
      for (int i = 0; i < SCS_NB_WORDS; ++i) {
        R_HW[i] = (uint)res[i];
      }
    }
    R_EXP = gpga_double_mul(X_EXP, Y_EXP);
    R_SGN = X_SGN * Y_SGN;
    scs_add(result, result, z);
  }
}

inline void gpga_atan_quick(thread gpga_double* atanhi,
                            thread gpga_double* atanlo,
                            thread int* index_of_e, gpga_double x) {
  gpga_double tmphi = gpga_double_zero(0u);
  gpga_double tmplo = gpga_double_zero(0u);
  gpga_double x0hi = gpga_double_zero(0u);
  gpga_double x0lo = gpga_double_zero(0u);
  gpga_double xmBihi = gpga_double_zero(0u);
  gpga_double xmBilo = gpga_double_zero(0u);
  gpga_double Xredhi = gpga_double_zero(0u);
  gpga_double Xredlo = gpga_double_zero(0u);
  gpga_double tmphi2 = gpga_double_zero(0u);
  gpga_double tmplo2 = gpga_double_zero(0u);
  gpga_double atanlolo = gpga_double_zero(0u);
  gpga_double q = gpga_double_zero(0u);
  gpga_double Xred2 = gpga_double_zero(0u);
  gpga_double x2 = gpga_double_zero(0u);
  gpga_double one = gpga_double_from_u32(1u);
  gpga_double zero = gpga_double_zero(0u);

  int i = 0;
  if (gpga_double_gt(x, atan_fast_min_reduction_needed)) {
    if (gpga_double_gt(x, atan_fast_table[61][1])) {
      i = 61;
      Add12(&xmBihi, &xmBilo, x, gpga_double_neg(atan_fast_table[61][1]));
    } else {
      i = 31;
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 16;
      } else {
        i += 16;
      }
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 8;
      } else {
        i += 8;
      }
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 4;
      } else {
        i += 4;
      }
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 2;
      } else {
        i += 2;
      }
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 1;
      } else {
        i += 1;
      }
      if (gpga_double_lt(x, atan_fast_table[i][0])) {
        i -= 1;
      }
      xmBihi = gpga_double_sub(x, atan_fast_table[i][1]);
      xmBilo = zero;
    }

    Mul12(&tmphi, &tmplo, x, atan_fast_table[i][1]);
    if (gpga_double_gt(x, one)) {
      Add22(&x0hi, &x0lo, tmphi, tmplo, one, zero);
    } else {
      Add22(&x0hi, &x0lo, one, zero, tmphi, tmplo);
    }
    Div22(&Xredhi, &Xredlo, xmBihi, xmBilo, x0hi, x0lo);

    Xred2 = gpga_double_mul(Xredhi, Xredhi);
    q = gpga_double_add(
        atan_fast_coef_poly[3],
        gpga_double_mul(
            Xred2,
            gpga_double_add(
                atan_fast_coef_poly[2],
                gpga_double_mul(
                    Xred2,
                    gpga_double_add(
                        atan_fast_coef_poly[1],
                        gpga_double_mul(Xred2, atan_fast_coef_poly[0]))))));
    q = gpga_double_mul(Xred2, q);

    atanlolo = gpga_double_add(Xredlo, atan_fast_table[i][3]);
    atanlolo = gpga_double_add(atanlolo, gpga_double_mul(Xredhi, q));
    Add12(&tmphi2, &tmplo2, atan_fast_table[i][2], Xredhi);
    Add12(atanhi, atanlo, tmphi2, gpga_double_add(tmplo2, atanlolo));

    if (i < 10) {
      *index_of_e = 0;
    } else {
      *index_of_e = 1;
    }
  } else {
    x2 = gpga_double_mul(x, x);
    q = gpga_double_add(
        atan_fast_coef_poly[3],
        gpga_double_mul(
            x2,
            gpga_double_add(
                atan_fast_coef_poly[2],
                gpga_double_mul(
                    x2,
                    gpga_double_add(
                        atan_fast_coef_poly[1],
                        gpga_double_mul(x2, atan_fast_coef_poly[0]))))));
    q = gpga_double_mul(x2, q);
    Add12(atanhi, atanlo, x, gpga_double_mul(x, q));
    *index_of_e = 2;
  }
}

inline void scs_atan(scs_ptr res_scs, scs_ptr x) {
  scs_t X_scs, denom1_scs, denom2_scs, poly_scs, X2;
  scs_t atanbhihi, atanbhilo, atanblo, atanbhi, atanb;
  scs_t bsc, coeff, one;
  gpga_double db = gpga_double_zero(0u);
  int k = 0;
  int i = 31;

  scs_get_d(&db, x);

  if (gpga_double_gt(db, atan_fast_min_reduction_needed)) {
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 16;
    } else {
      i += 16;
    }
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 8;
    } else {
      i += 8;
    }
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 4;
    } else {
      i += 4;
    }
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 2;
    } else {
      i += 2;
    }
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 1;
    } else if (i < 61) {
      i += 1;
    }
    if (gpga_double_lt(db, atan_fast_table[i][0])) {
      i -= 1;
    }

    scs_set_d(bsc, atan_fast_table[i][1]);
    scs_mul(denom1_scs, bsc, x);
    scs_set_const(one, SCS_ONE);
    scs_add(denom2_scs, denom1_scs, one);
    scs_sub(X_scs, x, bsc);
    scs_div(X_scs, X_scs, denom2_scs);

    scs_square(X2, X_scs);
    scs_set_const(coeff, atan_constant_poly_ptr);
    scs_set(res_scs, coeff);
    for (k = 1; k < 10; ++k) {
      scs_mul(res_scs, res_scs, X2);
      scs_set_const(coeff, atan_constant_poly_ptr + k);
      scs_add(res_scs, coeff, res_scs);
    }
    scs_mul(poly_scs, res_scs, X_scs);

    scs_set_d(atanbhihi, atan_fast_table[i][2]);
    scs_set_d(atanbhilo, atan_fast_table[i][3]);
    scs_set_d(atanblo, atan_blolo[i]);
    scs_add(atanbhi, atanbhihi, atanbhilo);
    scs_add(atanb, atanbhi, atanblo);
    scs_add(res_scs, atanb, poly_scs);
  } else {
    scs_square(X2, x);
    scs_set_const(coeff, atan_constant_poly_ptr);
    scs_set(res_scs, coeff);
    for (k = 1; k < 10; ++k) {
      scs_mul(res_scs, res_scs, X2);
      scs_set_const(coeff, atan_constant_poly_ptr + k);
      scs_add(res_scs, coeff, res_scs);
    }
    scs_mul(res_scs, res_scs, x);
  }
}

inline void scs_atanpi(scs_ptr res, scs_ptr x) {
  scs_t at, inv_pi;
  scs_atan(at, x);
  scs_set_const(inv_pi, atan_inv_pi_scs_ptr);
  scs_mul(res, at, inv_pi);
}

inline gpga_double scs_atan_rn(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atan(res_scs, sc1);
  scs_get_d(&res, res_scs);
  if (sign < 0) {
    res = gpga_double_neg(res);
  }
  return res;
}

inline gpga_double scs_atan_rd(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atan(res_scs, sc1);
  if (sign < 0) {
    scs_get_d_pinf(&res, res_scs);
    res = gpga_double_neg(res);
  } else {
    scs_get_d_minf(&res, res_scs);
  }
  return res;
}

inline gpga_double scs_atan_ru(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atan(res_scs, sc1);
  if (sign < 0) {
    scs_get_d_minf(&res, res_scs);
    res = gpga_double_neg(res);
  } else {
    scs_get_d_pinf(&res, res_scs);
  }
  return res;
}

inline gpga_double scs_atanpi_rn(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atanpi(res_scs, sc1);
  scs_get_d(&res, res_scs);
  if (sign < 0) {
    res = gpga_double_neg(res);
  }
  return res;
}

inline gpga_double scs_atanpi_rd(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atanpi(res_scs, sc1);
  if (sign < 0) {
    scs_get_d_pinf(&res, res_scs);
    res = gpga_double_neg(res);
  } else {
    scs_get_d_minf(&res, res_scs);
  }
  return res;
}

inline gpga_double scs_atanpi_ru(gpga_double x) {
  scs_t sc1, res_scs;
  gpga_double res = x;
  int sign = 1;

  if (gpga_double_sign(x) != 0u) {
    sign = -1;
    x = gpga_double_neg(x);
  }
  scs_set_d(sc1, x);
  scs_atanpi(res_scs, sc1);
  if (sign < 0) {
    scs_get_d_minf(&res, res_scs);
    res = gpga_double_neg(res);
  } else {
    scs_get_d_pinf(&res, res_scs);
  }
  return res;
}

inline gpga_double gpga_atan_rn(gpga_double x) {
  uint absxhi = gpga_u64_hi(x) & 0x7fffffff;
  uint xlo = gpga_u64_lo(x);

  if (absxhi >= 0x43500000u) {
    if (absxhi > 0x7ff00000u ||
        (absxhi == 0x7ff00000u && xlo != 0u)) {
      return gpga_double_add(x, x);
    }
    gpga_double result = atan_fast_halfpi;
    if (gpga_double_sign(x) != 0u) {
      result = gpga_double_neg(result);
    }
    return result;
  }

  if (absxhi < 0x3e400000u) {
    return x;
  }

  gpga_double ax = gpga_double_abs(x);
  gpga_double atanhi = gpga_double_zero(0u);
  gpga_double atanlo = gpga_double_zero(0u);
  int index_of_e = 0;
  gpga_atan_quick(&atanhi, &atanlo, &index_of_e, ax);

  gpga_double test =
      gpga_double_add(atanhi,
                      gpga_double_mul(atanlo, atan_fast_rncst[index_of_e]));
  gpga_double result =
      gpga_double_eq(atanhi, test) ? atanhi : scs_atan_rn(ax);
  if (gpga_double_sign(x) != 0u) {
    result = gpga_double_neg(result);
  }
  return result;
}

inline gpga_double gpga_atan_rd(gpga_double x) {
  return scs_atan_rd(x);
}

inline gpga_double gpga_atan_ru(gpga_double x) {
  return scs_atan_ru(x);
}

inline gpga_double gpga_atan_rz(gpga_double x) {
  if (gpga_double_sign(x) != 0u) {
    return scs_atan_ru(x);
  }
  return scs_atan_rd(x);
}

inline gpga_double gpga_atanpi_rn(gpga_double x) {
  return scs_atanpi_rn(x);
}

inline gpga_double gpga_atanpi_rd(gpga_double x) {
  return scs_atanpi_rd(x);
}

inline gpga_double gpga_atanpi_ru(gpga_double x) {
  return scs_atanpi_ru(x);
}

inline gpga_double gpga_atanpi_rz(gpga_double x) {
  if (gpga_double_sign(x) != 0u) {
    return scs_atanpi_ru(x);
  }
  return scs_atanpi_rd(x);
}

// CRLIBM_LOG_TD
// LOG_TD_CONSTANTS_BEGIN
// CRLIBM_LOG_TD_CONSTANTS
GPGA_CONST uint log_L = 7u;
GPGA_CONST uint log_MAXINDEX = 53u;
GPGA_CONST uint log_INDEXMASK = 127u;
GPGA_CONST gpga_double log_two52 = 0x4330000000000000ul;
GPGA_CONST gpga_double log_log2h = 0x3fe62e42fefa3800ul;
GPGA_CONST gpga_double log_log2m = 0x3d2ef35793c76800ul;
GPGA_CONST gpga_double log_log2l = 0xba59ff0342542fc3ul;
GPGA_CONST gpga_double log_ROUNDCST1 = 0x3ff00b5baade1dbcul;
GPGA_CONST gpga_double log_ROUNDCST2 = 0x3ff00b5baade1dbcul;
GPGA_CONST gpga_double log_RDROUNDCST1 = 0x3c06a09e667f3bcdul;
GPGA_CONST gpga_double log_RDROUNDCST2 = 0x3c06a09e667f3bcdul;
GPGA_CONST gpga_double log_c3 = 0x3fd5555555555556ul;
GPGA_CONST gpga_double log_c4 = 0xbfcffffffffafffaul;
GPGA_CONST gpga_double log_c5 = 0x3fc99999998e0b4dul;
GPGA_CONST gpga_double log_c6 = 0xbfc55569556623b2ul;
GPGA_CONST gpga_double log_c7 = 0x3fc2493d75f51811ul;
GPGA_CONST gpga_double log_accPolyC3h = 0x3fd5555555555555ul;
GPGA_CONST gpga_double log_accPolyC3l = 0x3c75555555555555ul;
GPGA_CONST gpga_double log_accPolyC4h = 0xbfd0000000000000ul;
GPGA_CONST gpga_double log_accPolyC4l = 0x3937ffadc266bcb8ul;
GPGA_CONST gpga_double log_accPolyC5h = 0x3fc999999999999aul;
GPGA_CONST gpga_double log_accPolyC5l = 0xbc69999999866631ul;
GPGA_CONST gpga_double log_accPolyC6h = 0xbfc5555555555555ul;
GPGA_CONST gpga_double log_accPolyC6l = 0xbc655555559e546aul;
GPGA_CONST gpga_double log_accPolyC7h = 0x3fc2492492492492ul;
GPGA_CONST gpga_double log_accPolyC7l = 0x3c6248448ff5ae97ul;
GPGA_CONST gpga_double log_accPolyC8h = 0xbfc0000000000000ul;
GPGA_CONST gpga_double log_accPolyC8l = 0x3bb02fd4be5cfcddul;
GPGA_CONST gpga_double log_accPolyC9h = 0x3fbc71c71c71c73aul;
GPGA_CONST gpga_double log_accPolyC9l = 0x3c53fbe1792ad51cul;
GPGA_CONST gpga_double log_accPolyC10 = 0xbfb99999999999ccul;
GPGA_CONST gpga_double log_accPolyC11 = 0x3fb745d174237d2cul;
GPGA_CONST gpga_double log_accPolyC12 = 0xbfb5555555095594ul;
GPGA_CONST gpga_double log_accPolyC13 = 0x3fb3b16e4739debbul;
GPGA_CONST gpga_double log_accPolyC14 = 0xbfb2495c92506ce8ul;
struct GpgaLogArgRedEntry {
  gpga_double ri;
  gpga_double logih;
  gpga_double logim;
  gpga_double logil;
};
GPGA_CONST GpgaLogArgRedEntry log_argredtable[128] = {
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3fefc07f00000000ul, 0x3f7fe02b6b106791ul, 0xbbce44b538c673f4ul, 0x38440499da63c12aul },
  { 0x3fef81f800000000ul, 0x3f8fc0b0b0fc07e4ul, 0xbc182f3d703fed4cul, 0x38591ce31c70dc4dul },
  { 0x3fef446580000000ul, 0x3f97b91ee7d5b2fbul, 0x3c249495cf8e7bf1ul, 0x38cb24d6d8f90e7aul },
  { 0x3fef07c200000000ul, 0x3f9f82990e783380ul, 0x3c333e345a474878ul, 0xb8a5fb2a92c49977ul },
  { 0x3feecc07c0000000ul, 0x3fa39e86e1febd8dul, 0x3c3c80a727d55e91ul, 0x38db9d2be408babcul },
  { 0x3fee9131c0000000ul, 0x3fa77457a632dd6bul, 0xbc4e72cf6684677bul, 0x38eb46cc139d7031ul },
  { 0x3fee573ac0000000ul, 0x3fab42de091971d5ul, 0x3c44a3464fc1289eul, 0xb8df47f3e39e0bc0ul },
  { 0x3fee1e1e00000000ul, 0x3faf0a32c01163a6ul, 0x3c485f5d07068577ul, 0x38e9c42b7a000cd5ul },
  { 0x3fede5d700000000ul, 0x3fb16535fea37b51ul, 0x3c5a188571cf8126ul, 0xb8fdbb77006dd61dul },
  { 0x3fedae6080000000ul, 0x3fb341d7461bd1ddul, 0x3c329980db65a305ul, 0x38a4ae90a83f75f4ul },
  { 0x3fed77b640000000ul, 0x3fb51b07f306187ful, 0xbc53b614fb757033ul, 0x38fdc94965249c3aul },
  { 0x3fed41d400000000ul, 0x3fb6f0d38ae56bccul, 0xbc5906c43c2f543dul, 0xb8da6a49ad5759f1ul },
  { 0x3fed0cb580000000ul, 0x3fb8c3465e319b45ul, 0x3c35acc0f5bb481aul, 0xb8d8d45f6a2e1045ul },
  { 0x3fecd85680000000ul, 0x3fba926d8a4ad570ul, 0xbc3af42b3ab91a14ul, 0xb8d36ea3b75cc715ul },
  { 0x3feca4b300000000ul, 0x3fbc5e54bf5bc748ul, 0xbc5a8a79e01fa78ful, 0x38fb69d997492765ul },
  { 0x3fec71c700000000ul, 0x3fbe27086e2af366ul, 0xbc361522aac86c0dul, 0x38dab99075dbdcd3ul },
  { 0x3fec3f8f00000000ul, 0x3fbfec9141dbeabbul, 0x3c451728cfa743d2ul, 0xb8c7560c625e7b2cul },
  { 0x3fec0e0700000000ul, 0x3fc0d77e8cd08e5aul, 0x3c69a5dc63e58601ul, 0x390e3797be3074f1ul },
  { 0x3febdd2b80000000ul, 0x3fc1b72b012f67a8ul, 0xbc61be7e76dbee7ful, 0x38e2f9a482ef12c5ul },
  { 0x3febacf900000000ul, 0x3fc29553581ff547ul, 0x3c63017b9c408047ul, 0xb909774eee845791ul },
  { 0x3feb7d6c40000000ul, 0x3fc371fc161e8f75ul, 0xbc680c9a4ff5c905ul, 0xb906ff45bc6ca01bul },
  { 0x3feb4e81c0000000ul, 0x3fc44d2b38cb7d29ul, 0xbc30585316b9acb0ul, 0xb8d38a1c2e99745aul },
  { 0x3feb203640000000ul, 0x3fc526e5e5a1b438ul, 0xbc6646ff8a44628ful, 0x38e4699dc3981556ul },
  { 0x3feaf286c0000000ul, 0x3fc5ff3060a793d5ul, 0xbc5bc60f05a71a18ul, 0x38e96a45496c489ful },
  { 0x3feac57000000000ul, 0x3fc6d6106719d25dul, 0xbc6caad7be421eceul, 0xb908e407570a1acaul },
  { 0x3fea98ef80000000ul, 0x3fc7ab886a10d963ul, 0x3c66f8c8eeafe6c0ul, 0xb8f093e406880cc5ul },
  { 0x3fea6d01c0000000ul, 0x3fc87f9feb20c94bul, 0xbc62f806fc0331abul, 0x3907a33ddd608496ul },
  { 0x3fea41a400000000ul, 0x3fc9525b1cf456f4ul, 0x3c6d9056c7f8e0d0ul, 0x39020ea94a3c3815ul },
  { 0x3fea16d400000000ul, 0x3fca23bbffe2b567ul, 0x3c49371105cfef01ul, 0x38ef4e5029571edeul },
  { 0x3fe9ec8e80000000ul, 0x3fcaf3c9b680c01dul, 0x3c48ce76fbe28593ul, 0xb8ea6623f9096499ul },
  { 0x3fe9c2d140000000ul, 0x3fcbc286be2d8cecul, 0xbc6c818a4e19ccc6ul, 0xb8e5543a8eb05407ul },
  { 0x3fe9999980000000ul, 0x3fcc8ff8479a9a62ul, 0xbc64f67f4d988d67ul, 0x38e343c9fdd3b4e9ul },
  { 0x3fe970e500000000ul, 0x3fcd5c21434fbb98ul, 0xbc691bbcf9d70802ul, 0x3903b9c4ac1340f9ul },
  { 0x3fe948b100000000ul, 0x3fce27075e2af2e7ul, 0xbc461578157356b5ul, 0xb8dffbce1ce62436ul },
  { 0x3fe920fb40000000ul, 0x3fcef0adfddc5940ul, 0x3c4618e0df41b39bul, 0x38eef16e328dfff9ul },
  { 0x3fe8f9c180000000ul, 0x3fcfb918bd5e3e44ul, 0xbc6caaabca476ee8ul, 0xb8f1693885263c9ful },
  { 0x3fe8d30180000000ul, 0x3fd04025b6b4d04aul, 0xbc5d1d80fc74adbful, 0x38cecdfb2bea9f81ul },
  { 0x3fe8acb900000000ul, 0x3fd0a3250a7390f0ul, 0xbc60460195491c17ul, 0x3901bf40bc583cf3ul },
  { 0x3fe886e600000000ul, 0x3fd1058bd1ae4ae2ul, 0xbc79d819228227f2ul, 0x39045f9571b24937ul },
  { 0x3fe8618600000000ul, 0x3fd1675cebaba62eul, 0x3c2ce6e9563361c2ul, 0xb8c41c387d241448ul },
  { 0x3fe83c9780000000ul, 0x3fd1c898b36999fdul, 0xbc7f0e5c70fa9c6dul, 0x391a72435bce3608ul },
  { 0x3fe8181800000000ul, 0x3fd229423bcf7986ul, 0xbc776f595b40cf5aul, 0x390095b919af56d6ul },
  { 0x3fe7f40600000000ul, 0x3fd2895a0bde86a4ul, 0xbc60a5b682d74d38ul, 0xb8f1f501c65e17aful },
  { 0x3fe7d05f40000000ul, 0x3fd2e8e2bee11d31ul, 0xbc70f4cdb90968a4ul, 0x39044686682fc1d5ul },
  { 0x3fe7ad2200000000ul, 0x3fd347ddb2987d59ul, 0x3c75915a1bfb7318ul, 0x391f7289d78d2720ul },
  { 0x3fe78a4c80000000ul, 0x3fd3a64c596945eaul, 0xbc58d0ca31369da2ul, 0x38f12c49cf65848eul },
  { 0x3fe767dd00000000ul, 0x3fd404303a86a811ul, 0xbc617a089db0379dul, 0x38dbe576e58bd6f3ul },
  { 0x3fe745d180000000ul, 0x3fd4618ba21c5ecaul, 0x3c7f42de234224b2ul, 0x39032d5974470934ul },
  { 0x3fe7242880000000ul, 0x3fd4be5f937778a1ul, 0xbc5cb366b633ad24ul, 0xb8f87f157653dd07ul },
  { 0x3fe702e040000000ul, 0x3fd51aadd52df85dul, 0xbc7b8da7002c0c70ul, 0xb9195f31a91378bdul },
  { 0x3fe6e1f780000000ul, 0x3fd57676dd455a87ul, 0xbc78d95645b1ec97ul, 0x39077c20b5a07803ul },
  { 0x3fe6c16c00000000ul, 0x3fd5d1bdff5809eaul, 0x3c742368d931d936ul, 0x391abdf024f5679dul },
  { 0x3ff6a13cc0000000ul, 0xbfd63002da3aac36ul, 0xbc7e60c40dcea917ul, 0xb91cdd0f61651af2ul },
  { 0x3ff6816800000000ul, 0xbfd5d5bd9f595f10ul, 0x3c7654169e2111f8ul, 0xb90a3fc653eb9dfdul },
  { 0x3ff661ec80000000ul, 0xbfd57bf791c8d1ddul, 0x3c64908363633d1aul, 0xb90ee5ff7a0cae08ul },
  { 0x3ff642c840000000ul, 0xbfd522adbf38a3aful, 0xbc7384038e3ac4dbul, 0xb8f10a81f78453edul },
  { 0x3ff623fa80000000ul, 0xbfd4c9e0b8172c37ul, 0x3c7648d7fb3a7409ul, 0xb91d1eeae5e123a5ul },
  { 0x3ff6058180000000ul, 0xbfd4718e1e71c3d9ul, 0x3c59c0ed9c243decul, 0xb8f2b7e252642fa1ul },
  { 0x3ff5e75bc0000000ul, 0xbfd419b438d5e8c4ul, 0x3c741226ae02c643ul, 0x38f7e80d3465e0deul },
  { 0x3ff5c98840000000ul, 0xbfd3c252b3333168ul, 0x3c7aad23800a91ddul, 0x3907e95b887adc2bul },
  { 0x3ff5ac0580000000ul, 0xbfd36b67b4be10f9ul, 0x3c5b24dd8358e8faul, 0x38ffa1724eadcbdcul },
  { 0x3ff58ed240000000ul, 0xbfd314f20fd35cd3ul, 0xbc6452d1e21f20cful, 0xb8f0d7bcbb8819caul },
  { 0x3ff571ed40000000ul, 0xbfd2bef087dc9353ul, 0x3c74adad78e9b5deul, 0x38e1017dbf228b90ul },
  { 0x3ff5555540000000ul, 0xbfd26961d134db72ul, 0xbc7e0ef5884856d5ul, 0xb9146c80ed264100ul },
  { 0x3ff5390940000000ul, 0xbfd21445520eb8cful, 0x3c7cc28bd90e2d1cul, 0xb91d1feb8f8351e2ul },
  { 0x3ff51d0800000000ul, 0xbfd1bf99a35a6b75ul, 0x3c612ae0d979ef79ul, 0xb90682b54d9596eful },
  { 0x3ff5015000000000ul, 0xbfd16b5c8bacfb53ul, 0xbc766fb7d35eafe0ul, 0x39047ddd7dd17282ul },
  { 0x3ff4e5e0c0000000ul, 0xbfd1178ece27e44ful, 0x3c707314412b95e7ul, 0x39138a8b58997d92ul },
  { 0x3ff4cab880000000ul, 0xbfd0c42d516162dful, 0xbc7258b1afe1ef18ul, 0x38f11c6c6f671faaul },
  { 0x3ff4afd6c0000000ul, 0xbfd07138c24d5817ul, 0xbc7adb2a135925e5ul, 0xb91de54f159334daul },
  { 0x3ff4953a00000000ul, 0xbfd01eaeae26c654ul, 0xbc7dcfbbc5b020adul, 0x38fbeb4de379ef62ul },
  { 0x3ff47ae140000000ul, 0xbfcf991c3cb3b370ul, 0xbc6f664fd6f98079ul, 0xb8db60d43d0ebb2ful },
  { 0x3ff460cbc0000000ul, 0xbfcef5adb2dcffdcul, 0xbc4aea97b9674356ul, 0x38dcca692be9eee8ul },
  { 0x3ff446f880000000ul, 0xbfce530fa7e70fa4ul, 0x3c56ec1fa306175aul, 0x38f1a1408aa59b38ul },
  { 0x3ff42d6640000000ul, 0xbfcdb13e56d488d5ul, 0x3c6baf5a16e60af0ul, 0xb9059c29567868aaul },
  { 0x3ff4141400000000ul, 0xbfcd103772655e3bul, 0xbc66061e7979bef7ul, 0xb90d143b40121864ul },
  { 0x3ff3fb0140000000ul, 0xbfcc6ffbc8f00f71ul, 0x3c69e58b2c54f9faul, 0x38e19c6e2eb2b026ul },
  { 0x3ff3e22cc0000000ul, 0xbfcbd0874c3bd8abul, 0xbc6fba6ac93f4d84ul, 0xb9069fffaf976eaeul },
  { 0x3ff3c995c0000000ul, 0xbfcb31d9095bcdc1ul, 0x3c4cd4781065c78bul, 0x38b0e466c9499b78ul },
  { 0x3ff3b13b00000000ul, 0xbfca93ecbc8ad9a3ul, 0xbc6bcaeff33ebf59ul, 0x38fbbceac26874f6ul },
  { 0x3ff3991c40000000ul, 0xbfc9f6c489089622ul, 0xbc3eb48c26273671ul, 0x38c281846bd28c37ul },
  { 0x3ff3813800000000ul, 0xbfc95a5a5cf7013ful, 0xbc5142afb2a614e8ul, 0x38f25ae15785f251ul },
  { 0x3ff3698e00000000ul, 0xbfc8beb03b38fe73ul, 0xbc555aadebeecd25ul, 0x38ffad9bf54a8b5dul },
  { 0x3ff3521d00000000ul, 0xbfc823c18551a3beul, 0x3c61232cbc613cdful, 0xb900bcb2bc4ec7ceul },
  { 0x3ff33ae440000000ul, 0xbfc7898ccf444bf2ul, 0x3c5307466795e4baul, 0xb8e2c9e7e8e451baul },
  { 0x3ff323e340000000ul, 0xbfc6f01247756aaaul, 0x3c6cde5b5b88c1baul, 0x38f155fe4db1868cul },
  { 0x3ff30d1900000000ul, 0xbfc6574eb68c133aul, 0x3c63a69e1f36ee28ul, 0x3906b5ba9bc4fc42ul },
  { 0x3ff2f684c0000000ul, 0xbfc5bf407b543db1ul, 0x3c21f5b3f6b8a29aul, 0xb897d6d65261106ful },
  { 0x3ff2e025c0000000ul, 0xbfc527e5e2a1b58dul, 0x3c338d4b41320354ul, 0x3886eb72813ab795ul },
  { 0x3ff2c9fb40000000ul, 0xbfc4913d2733b540ul, 0x3c58d56835064acful, 0x38ea9e294f6bcdc2ul },
  { 0x3ff2b404c0000000ul, 0xbfc3fb462799288aul, 0x3c6e87db76a78384ul, 0x38d03f62a10290abul },
  { 0x3ff29e4140000000ul, 0xbfc365fd48158fbcul, 0x3c40579541e30b9eul, 0x38d21bef2b886417ul },
  { 0x3ff288b000000000ul, 0xbfc2d1608c8680faul, 0x3c5499b947b05eb5ul, 0xb8e9731012456fa5ul },
  { 0x3ff27350c0000000ul, 0xbfc23d715e49c1f7ul, 0xbc4471fd5840ded1ul, 0xb899280ef4cea833ul },
  { 0x3ff25e2280000000ul, 0xbfc1aa2bea23f6fcul, 0xbc64e449f1d34012ul, 0xb90b5a2eced36a0dul },
  { 0x3ff2492480000000ul, 0xbfc1178e0227e43cul, 0x3c50e64fb4572be6ul, 0x38d47044792e090bul },
  { 0x3ff2345680000000ul, 0xbfc08598e99e39fcul, 0x3c2d6ffe1ed6a14bul, 0xb8cf689a4654a24aul },
  { 0x3ff21fb780000000ul, 0xbfbfe89129dbd565ul, 0xbc34d82f752c5c5dul, 0x38d0c5f45144605cul },
  { 0x3ff20b4700000000ul, 0xbfbec738d30a10e3ul, 0xbc52e9fc48994b23ul, 0x38ef8952ef61bd79ul },
  { 0x3ff1f70480000000ul, 0xbfbda727838446a0ul, 0xbc5401fa7c1ddac2ul, 0xb8fa5569986e12e3ul },
  { 0x3ff1e2ef40000000ul, 0xbfbc885845bc4b1aul, 0xbc5838cbbbf5119cul, 0xb8e681a01a74ba0ful },
  { 0x3ff1cf06c0000000ul, 0xbfbb6ac995ad5a94ul, 0x3c5002a8110e64e8ul, 0x38cbaad6884dd07bul },
  { 0x3ff1bb4a40000000ul, 0xbfba4e763cb1bc38ul, 0x3c57b5ca204397aful, 0xb8f0e837253c3a01ul },
  { 0x3ff1a7b980000000ul, 0xbfb933601d594801ul, 0x3c54783601b00d71ul, 0x38f9ecb1b92aaa0cul },
  { 0x3ff1945380000000ul, 0xbfb8197e2740e3f0ul, 0x3c11834803aef5a0ul, 0x38bf1123cab580b0ul },
  { 0x3ff1811800000000ul, 0xbfb700d20aeac061ul, 0x3c272610cbd807b0ul, 0xb8c769cd455e3844ul },
  { 0x3ff16e0680000000ul, 0xbfb5e959c59791a7ul, 0xbc5738712986ee6ful, 0x38d6cf73270c9feeul },
  { 0x3ff15b1e40000000ul, 0xbfb4d30f8d207d08ul, 0x3c412d15b4662918ul, 0xb8de2e3e7386444bul },
  { 0x3ff1485f00000000ul, 0xbfb3bdf4d7d1ee10ul, 0x3c542b50077a821ful, 0xb8f43d9e39a8e97ful },
  { 0x3ff135c800000000ul, 0xbfb2aa03a4471725ul, 0x3c5d15e8e285094cul, 0x38fca59e52c0557aul },
  { 0x3ff1235900000000ul, 0xbfb1973d4146545eul, 0xbc54557daad70fdeul, 0x38f09f0bb0d2bda2ul },
  { 0x3ff1111100000000ul, 0xbfb08597b59e3987ul, 0x3c5dd715ee582488ul, 0xb8fd68a73e3a5cbful },
  { 0x3ff0fef000000000ul, 0xbfaeea2fc006b77cul, 0x3c43e5273e628117ul, 0xb8ca85807ae67a61ul },
  { 0x3ff0ecf580000000ul, 0xbfaccb762dddb163ul, 0x3c4e48b3925684cbul, 0x38b43168155bdc55ul },
  { 0x3ff0db20c0000000ul, 0xbfaaaef598fb0f0dul, 0xbc146e0d56030305ul, 0x389a76fd5fdf00bful },
  { 0x3ff0c97140000000ul, 0xbfa894a8349fb262ul, 0xbc3a8ba3266070cdul, 0x38deb910ab9aea81ul },
  { 0x3ff0b7e700000000ul, 0xbfa67c9752d4b9eful, 0xbc404185d7b0510cul, 0x38c39c4ec624ec58ul },
  { 0x3ff0a68100000000ul, 0xbfa466ad942de386ul, 0x3c4cdd79e9f4c30aul, 0x38de30ecf6d630b9ul },
  { 0x3ff0953f40000000ul, 0xbfa252f4078d1811ul, 0xbc15c05d0df52f35ul, 0x38b88249d1f21625ul },
  { 0x3ff0842100000000ul, 0xbfa0415c89e74404ul, 0xbc4c05c9c81fdecdul, 0x38d6eef4f8c44cb2ul },
  { 0x3ff0732600000000ul, 0xbf9c63d06c14aa2aul, 0x3c3ce0457bdc1ca0ul, 0xb8cc6056239dea62ul },
  { 0x3ff0624dc0000000ul, 0xbf98492088c8c812ul, 0xbc07327889d74456ul, 0x38989a3d95001e30ul },
  { 0x3ff0519800000000ul, 0xbf9432ab25980c41ul, 0x3c38cda48e559ae8ul, 0xb8d407c947350292ul },
  { 0x3ff0410400000000ul, 0xbf90205258935647ul, 0xbc327c392ec151caul, 0x38d9c7b2ccdd1911ul },
  { 0x3ff03091c0000000ul, 0xbf88244e0388a0dcul, 0x3c0f6904cc57aa6bul, 0xb8a392904788cd79ul },
  { 0x3ff0204080000000ul, 0xbf801014f588de6dul, 0xbc146662bec2797aul, 0x38b36c3e559046feul },
  { 0x3ff0101000000000ul, 0xbf7007f559588335ul, 0xbc1f950e379fe121ul, 0xb8bdf1a09c589873ul },
};
// LOG_TD_CONSTANTS_END

inline void gpga_log_td_accurate(thread gpga_double* logh,
                                 thread gpga_double* logm,
                                 thread gpga_double* logl, int E,
                                 gpga_double ed, int index, gpga_double zh,
                                 gpga_double zl, gpga_double logih,
                                 gpga_double logim) {
  (void)E;
  gpga_double highPoly = gpga_double_add(
      log_accPolyC13, gpga_double_mul(zh, log_accPolyC14));
  highPoly = gpga_double_add(log_accPolyC12, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log_accPolyC11, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log_accPolyC10, gpga_double_mul(zh, highPoly));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  gpga_double t5h = gpga_double_zero(0u);
  gpga_double t5l = gpga_double_zero(0u);
  gpga_double t6h = gpga_double_zero(0u);
  gpga_double t6l = gpga_double_zero(0u);
  gpga_double t7h = gpga_double_zero(0u);
  gpga_double t7l = gpga_double_zero(0u);
  gpga_double t8h = gpga_double_zero(0u);
  gpga_double t8l = gpga_double_zero(0u);
  gpga_double t9h = gpga_double_zero(0u);
  gpga_double t9l = gpga_double_zero(0u);
  gpga_double t10h = gpga_double_zero(0u);
  gpga_double t10l = gpga_double_zero(0u);
  gpga_double t11h = gpga_double_zero(0u);
  gpga_double t11l = gpga_double_zero(0u);
  gpga_double t12h = gpga_double_zero(0u);
  gpga_double t12l = gpga_double_zero(0u);
  gpga_double t13h = gpga_double_zero(0u);
  gpga_double t13l = gpga_double_zero(0u);
  gpga_double t14h = gpga_double_zero(0u);
  gpga_double t14l = gpga_double_zero(0u);

  Mul12(&t1h, &t1l, zh, highPoly);
  Add22(&t2h, &t2l, log_accPolyC9h, log_accPolyC9l, t1h, t1l);
  Mul22(&t3h, &t3l, zh, zl, t2h, t2l);
  Add22(&t4h, &t4l, log_accPolyC8h, log_accPolyC8l, t3h, t3l);
  Mul22(&t5h, &t5l, zh, zl, t4h, t4l);
  Add22(&t6h, &t6l, log_accPolyC7h, log_accPolyC7l, t5h, t5l);
  Mul22(&t7h, &t7l, zh, zl, t6h, t6l);
  Add22(&t8h, &t8l, log_accPolyC6h, log_accPolyC6l, t7h, t7l);
  Mul22(&t9h, &t9l, zh, zl, t8h, t8l);
  Add22(&t10h, &t10l, log_accPolyC5h, log_accPolyC5l, t9h, t9l);
  Mul22(&t11h, &t11l, zh, zl, t10h, t10l);
  Add22(&t12h, &t12l, log_accPolyC4h, log_accPolyC4l, t11h, t11l);
  Mul22(&t13h, &t13l, zh, zl, t12h, t12l);
  Add22(&t14h, &t14l, log_accPolyC3h, log_accPolyC3l, t13h, t13l);

  gpga_double zSquareh = gpga_double_zero(0u);
  gpga_double zSquarem = gpga_double_zero(0u);
  gpga_double zSquarel = gpga_double_zero(0u);
  gpga_double zCubeh = gpga_double_zero(0u);
  gpga_double zCubem = gpga_double_zero(0u);
  gpga_double zCubel = gpga_double_zero(0u);
  Mul23(&zSquareh, &zSquarem, &zSquarel, zh, zl, zh, zl);
  Mul233(&zCubeh, &zCubem, &zCubel, zh, zl, zSquareh, zSquarem, zSquarel);

  gpga_double higherPolyMultZh = gpga_double_zero(0u);
  gpga_double higherPolyMultZm = gpga_double_zero(0u);
  gpga_double higherPolyMultZl = gpga_double_zero(0u);
  Mul233(&higherPolyMultZh, &higherPolyMultZm, &higherPolyMultZl, t14h, t14l,
         zCubeh, zCubem, zCubel);

  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  gpga_double zSquareHalfh = gpga_double_mul(zSquareh, neg_half);
  gpga_double zSquareHalfm = gpga_double_mul(zSquarem, neg_half);
  gpga_double zSquareHalfl = gpga_double_mul(zSquarel, neg_half);

  gpga_double polyWithSquareh = gpga_double_zero(0u);
  gpga_double polyWithSquarem = gpga_double_zero(0u);
  gpga_double polyWithSquarel = gpga_double_zero(0u);
  Add33(&polyWithSquareh, &polyWithSquarem, &polyWithSquarel, zSquareHalfh,
        zSquareHalfm, zSquareHalfl, higherPolyMultZh, higherPolyMultZm,
        higherPolyMultZl);

  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polym = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  Add233(&polyh, &polym, &polyl, zh, zl, polyWithSquareh, polyWithSquarem,
         polyWithSquarel);

  gpga_double logil = log_argredtable[index].logil;
  gpga_double logyh = gpga_double_zero(0u);
  gpga_double logym = gpga_double_zero(0u);
  gpga_double logyl = gpga_double_zero(0u);
  Add33(&logyh, &logym, &logyl, logih, logim, logil, polyh, polym, polyl);

  gpga_double log2edh = gpga_double_mul(log_log2h, ed);
  gpga_double log2edm = gpga_double_mul(log_log2m, ed);
  gpga_double log2edl = gpga_double_mul(log_log2l, ed);

  gpga_double loghover = gpga_double_zero(0u);
  gpga_double logmover = gpga_double_zero(0u);
  gpga_double loglover = gpga_double_zero(0u);
  Add33(&loghover, &logmover, &loglover, log2edh, log2edm, log2edl, logyh,
        logym, logyl);

  Renormalize3(logh, logm, logl, loghover, logmover, loglover);
}

inline gpga_double gpga_log_rn(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
  if (index >= (int)log_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_ROUNDCST1 : log_ROUNDCST2;
  gpga_double test = gpga_double_add(logh, gpga_double_mul(logm, roundcst));
  if (gpga_double_eq(logh, test)) {
    return logh;
  }

  gpga_log_td_accurate(&logh, &logm, &logl, E, ed, index, zh, zl, logih,
                       logim);
  return ReturnRoundToNearest3(logh, logm, logl);
}

inline gpga_double gpga_log_ru(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  if (gpga_double_eq(x, one)) {
    return gpga_double_zero(0u);
  }

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
  if (index >= (int)log_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, one);
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_ru(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log_td_accurate(&logh, &logm, &logl, E, ed, index, zh, zl, logih,
                       logim);
  return ReturnRoundUpwards3(logh, logm, logl);
}

inline gpga_double gpga_log_rd(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  if (gpga_double_eq(x, one)) {
    return gpga_double_zero(0u);
  }

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
  if (index >= (int)log_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, one);
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_rd(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log_td_accurate(&logh, &logm, &logl, E, ed, index, zh, zl, logih,
                       logim);
  return ReturnRoundDownwards3(logh, logm, logl);
}

inline gpga_double gpga_log_rz(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  if (gpga_double_eq(x, one)) {
    return gpga_double_zero(0u);
  }

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
  if (index >= (int)log_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, one);
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_rz(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log_td_accurate(&logh, &logm, &logl, E, ed, index, zh, zl, logih,
                       logim);
  return ReturnRoundTowardsZero3(logh, logm, logl);
}

// CRLIBM_LOG1P_TD
inline void gpga_log1p_td_accurate(thread gpga_double* logh,
                                   thread gpga_double* logm,
                                   thread gpga_double* logl, gpga_double ed,
                                   int index, gpga_double zh, gpga_double zm,
                                   gpga_double zl, gpga_double logih,
                                   gpga_double logim) {
  gpga_double highPoly = gpga_double_add(
      log_accPolyC13, gpga_double_mul(zh, log_accPolyC14));
  highPoly = gpga_double_add(log_accPolyC12, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log_accPolyC11, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log_accPolyC10, gpga_double_mul(zh, highPoly));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  gpga_double t5h = gpga_double_zero(0u);
  gpga_double t5l = gpga_double_zero(0u);
  gpga_double t6h = gpga_double_zero(0u);
  gpga_double t6l = gpga_double_zero(0u);
  gpga_double t7h = gpga_double_zero(0u);
  gpga_double t7l = gpga_double_zero(0u);
  gpga_double t8h = gpga_double_zero(0u);
  gpga_double t8l = gpga_double_zero(0u);
  gpga_double t9h = gpga_double_zero(0u);
  gpga_double t9l = gpga_double_zero(0u);
  gpga_double t10h = gpga_double_zero(0u);
  gpga_double t10l = gpga_double_zero(0u);
  gpga_double t11h = gpga_double_zero(0u);
  gpga_double t11l = gpga_double_zero(0u);
  gpga_double t12h = gpga_double_zero(0u);
  gpga_double t12l = gpga_double_zero(0u);
  gpga_double t13h = gpga_double_zero(0u);
  gpga_double t13l = gpga_double_zero(0u);
  gpga_double t14h = gpga_double_zero(0u);
  gpga_double t14l = gpga_double_zero(0u);

  Mul12(&t1h, &t1l, zh, highPoly);
  Add22(&t2h, &t2l, log_accPolyC9h, log_accPolyC9l, t1h, t1l);
  Mul22(&t3h, &t3l, zh, zm, t2h, t2l);
  Add22(&t4h, &t4l, log_accPolyC8h, log_accPolyC8l, t3h, t3l);
  Mul22(&t5h, &t5l, zh, zm, t4h, t4l);
  Add22(&t6h, &t6l, log_accPolyC7h, log_accPolyC7l, t5h, t5l);
  Mul22(&t7h, &t7l, zh, zm, t6h, t6l);
  Add22(&t8h, &t8l, log_accPolyC6h, log_accPolyC6l, t7h, t7l);
  Mul22(&t9h, &t9l, zh, zm, t8h, t8l);
  Add22(&t10h, &t10l, log_accPolyC5h, log_accPolyC5l, t9h, t9l);
  Mul22(&t11h, &t11l, zh, zm, t10h, t10l);
  Add22(&t12h, &t12l, log_accPolyC4h, log_accPolyC4l, t11h, t11l);
  Mul22(&t13h, &t13l, zh, zm, t12h, t12l);
  Add22(&t14h, &t14l, log_accPolyC3h, log_accPolyC3l, t13h, t13l);

  gpga_double zSquareh = gpga_double_zero(0u);
  gpga_double zSquarem = gpga_double_zero(0u);
  gpga_double zSquarel = gpga_double_zero(0u);
  gpga_double zCubeh = gpga_double_zero(0u);
  gpga_double zCubem = gpga_double_zero(0u);
  gpga_double zCubel = gpga_double_zero(0u);
  Mul33(&zSquareh, &zSquarem, &zSquarel, zh, zm, zl, zh, zm, zl);
  Mul33(&zCubeh, &zCubem, &zCubel, zh, zm, zl, zSquareh, zSquarem, zSquarel);

  gpga_double higherPolyMultZh = gpga_double_zero(0u);
  gpga_double higherPolyMultZm = gpga_double_zero(0u);
  gpga_double higherPolyMultZl = gpga_double_zero(0u);
  Mul233(&higherPolyMultZh, &higherPolyMultZm, &higherPolyMultZl, t14h, t14l,
         zCubeh, zCubem, zCubel);

  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  gpga_double zSquareHalfh = gpga_double_mul(zSquareh, neg_half);
  gpga_double zSquareHalfm = gpga_double_mul(zSquarem, neg_half);
  gpga_double zSquareHalfl = gpga_double_mul(zSquarel, neg_half);

  gpga_double polyWithSquareh = gpga_double_zero(0u);
  gpga_double polyWithSquarem = gpga_double_zero(0u);
  gpga_double polyWithSquarel = gpga_double_zero(0u);
  Add33(&polyWithSquareh, &polyWithSquarem, &polyWithSquarel, zSquareHalfh,
        zSquareHalfm, zSquareHalfl, higherPolyMultZh, higherPolyMultZm,
        higherPolyMultZl);

  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polym = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  Add33(&polyh, &polym, &polyl, zh, zm, zl, polyWithSquareh, polyWithSquarem,
        polyWithSquarel);

  gpga_double logil = log_argredtable[index].logil;
  gpga_double logyh = gpga_double_zero(0u);
  gpga_double logym = gpga_double_zero(0u);
  gpga_double logyl = gpga_double_zero(0u);
  Add33(&logyh, &logym, &logyl, logih, logim, logil, polyh, polym, polyl);

  gpga_double log2edhover = gpga_double_mul(log_log2h, ed);
  gpga_double log2edmover = gpga_double_mul(log_log2m, ed);
  gpga_double log2edlover = gpga_double_mul(log_log2l, ed);

  gpga_double log2edh = log2edhover;
  gpga_double log2edm = log2edmover;
  gpga_double log2edl = log2edlover;

  gpga_double loghover = gpga_double_zero(0u);
  gpga_double logmover = gpga_double_zero(0u);
  gpga_double loglover = gpga_double_zero(0u);
  Add33(&loghover, &logmover, &loglover, log2edh, log2edm, log2edl, logyh,
        logym, logyl);

  Renormalize3(logh, logm, logl, loghover, logmover, loglover);
}

inline gpga_double gpga_log1p_rn(gpga_double x) {
  gpga_double yh = gpga_double_zero(0u);
  gpga_double yl = gpga_double_zero(0u);
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yhrih = gpga_double_zero(0u);
  gpga_double yhril = gpga_double_zero(0u);
  gpga_double ylri = gpga_double_zero(0u);
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (abs_hi < 0x3c900000u) {
    return x;
  }

  if ((xhi & 0x80000000u) != 0u && abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if ((xhi & 0x7ff00000u) == 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  if (abs_hi < 0x3f700000u) {
    logih = gpga_double_zero(0u);
    logim = gpga_double_zero(0u);
    index = 0;
    ed = gpga_double_zero(0u);
    zh = x;
    zm = gpga_double_zero(0u);
    zl = gpga_double_zero(0u);
  } else {
    Add12Cond(&sh, &sl, one, x);
    uint sh_hi = gpga_u64_hi(sh);
    uint sh_lo = gpga_u64_lo(sh);
    E += (int)((sh_hi >> 20) & 0x7ffu) - 1023;
    index = (int)(sh_hi & 0x000fffffU);
    sh_hi = (uint)index | 0x3ff00000u;
    index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
    if (index >= (int)log_MAXINDEX) {
      sh_hi -= 0x00100000u;
      E += 1;
    }
    yh = gpga_u64_from_words(sh_hi, sh_lo);
    index &= (int)log_INDEXMASK;
    ed = gpga_double_from_s32(E);

    ri = log_argredtable[index].ri;
    logih = log_argredtable[index].logih;
    logim = log_argredtable[index].logim;

    if (gpga_double_is_zero(sl) || E > 125) {
      Mul12(&yhrih, &yhril, yh, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&zh, &zm, t1, yhril);
      zl = gpga_double_zero(0u);
    } else {
      int scale_exp = -E + 1023;
      gpga_double scale =
          gpga_u64_from_words((uint)scale_exp << 20, 0u);
      yl = gpga_double_mul(sl, scale);
      Mul12(&yhrih, &yhril, yh, ri);
      ylri = gpga_double_mul(yl, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&t2, &t3, yhril, ylri);
      Add12Cond(&t4, &t5, t1, t2);
      Add12Cond(&t6, &zl, t3, t5);
      Add12Cond(&zh, &zm, t4, t6);
    }
  }

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zm)));
  Add22(&t2h, &t2l, zh, zm, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));
  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_ROUNDCST1 : log_ROUNDCST2;
  gpga_double test = gpga_double_add(logh, gpga_double_mul(logm, roundcst));
  if (gpga_double_eq(logh, test)) {
    return logh;
  }

  gpga_log1p_td_accurate(&logh, &logm, &logl, ed, index, zh, zm, zl, logih,
                         logim);
  return ReturnRoundToNearest3(logh, logm, logl);
}

inline gpga_double gpga_log1p_ru(gpga_double x) {
  gpga_double yh = gpga_double_zero(0u);
  gpga_double yl = gpga_double_zero(0u);
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yhrih = gpga_double_zero(0u);
  gpga_double yhril = gpga_double_zero(0u);
  gpga_double ylri = gpga_double_zero(0u);
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (abs_hi < 0x3c900000u) {
    return x;
  }

  if ((xhi & 0x80000000u) != 0u && abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if ((xhi & 0x7ff00000u) == 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  if (abs_hi < 0x3f700000u) {
    logih = gpga_double_zero(0u);
    logim = gpga_double_zero(0u);
    index = 0;
    ed = gpga_double_zero(0u);
    zh = x;
    zm = gpga_double_zero(0u);
    zl = gpga_double_zero(0u);
  } else {
    Add12Cond(&sh, &sl, one, x);
    uint sh_hi = gpga_u64_hi(sh);
    uint sh_lo = gpga_u64_lo(sh);
    E += (int)((sh_hi >> 20) & 0x7ffu) - 1023;
    index = (int)(sh_hi & 0x000fffffU);
    sh_hi = (uint)index | 0x3ff00000u;
    index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
    if (index >= (int)log_MAXINDEX) {
      sh_hi -= 0x00100000u;
      E += 1;
    }
    yh = gpga_u64_from_words(sh_hi, sh_lo);
    index &= (int)log_INDEXMASK;
    ed = gpga_double_from_s32(E);

    ri = log_argredtable[index].ri;
    logih = log_argredtable[index].logih;
    logim = log_argredtable[index].logim;

    if (gpga_double_is_zero(sl) || E > 125) {
      Mul12(&yhrih, &yhril, yh, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&zh, &zm, t1, yhril);
      zl = gpga_double_zero(0u);
    } else {
      int scale_exp = -E + 1023;
      gpga_double scale =
          gpga_u64_from_words((uint)scale_exp << 20, 0u);
      yl = gpga_double_mul(sl, scale);
      Mul12(&yhrih, &yhril, yh, ri);
      ylri = gpga_double_mul(yl, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&t2, &t3, yhril, ylri);
      Add12Cond(&t4, &t5, t1, t2);
      Add12Cond(&t6, &zl, t3, t5);
      Add12Cond(&zh, &zm, t4, t6);
    }
  }

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zm)));
  Add22(&t2h, &t2l, zh, zm, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));
  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_ru(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log1p_td_accurate(&logh, &logm, &logl, ed, index, zh, zm, zl, logih,
                         logim);
  return ReturnRoundUpwards3(logh, logm, logl);
}

inline gpga_double gpga_log1p_rd(gpga_double x) {
  gpga_double yh = gpga_double_zero(0u);
  gpga_double yl = gpga_double_zero(0u);
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yhrih = gpga_double_zero(0u);
  gpga_double yhril = gpga_double_zero(0u);
  gpga_double ylri = gpga_double_zero(0u);
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (abs_hi < 0x3c900000u) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    return gpga_double_next_down(x);
  }

  if ((xhi & 0x80000000u) != 0u && abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if ((xhi & 0x7ff00000u) == 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  if (abs_hi < 0x3f700000u) {
    logih = gpga_double_zero(0u);
    logim = gpga_double_zero(0u);
    index = 0;
    ed = gpga_double_zero(0u);
    zh = x;
    zm = gpga_double_zero(0u);
    zl = gpga_double_zero(0u);
  } else {
    Add12Cond(&sh, &sl, one, x);
    uint sh_hi = gpga_u64_hi(sh);
    uint sh_lo = gpga_u64_lo(sh);
    E += (int)((sh_hi >> 20) & 0x7ffu) - 1023;
    index = (int)(sh_hi & 0x000fffffU);
    sh_hi = (uint)index | 0x3ff00000u;
    index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
    if (index >= (int)log_MAXINDEX) {
      sh_hi -= 0x00100000u;
      E += 1;
    }
    yh = gpga_u64_from_words(sh_hi, sh_lo);
    index &= (int)log_INDEXMASK;
    ed = gpga_double_from_s32(E);

    ri = log_argredtable[index].ri;
    logih = log_argredtable[index].logih;
    logim = log_argredtable[index].logim;

    if (gpga_double_is_zero(sl) || E > 125) {
      Mul12(&yhrih, &yhril, yh, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&zh, &zm, t1, yhril);
      zl = gpga_double_zero(0u);
    } else {
      int scale_exp = -E + 1023;
      gpga_double scale =
          gpga_u64_from_words((uint)scale_exp << 20, 0u);
      yl = gpga_double_mul(sl, scale);
      Mul12(&yhrih, &yhril, yh, ri);
      ylri = gpga_double_mul(yl, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&t2, &t3, yhril, ylri);
      Add12Cond(&t4, &t5, t1, t2);
      Add12Cond(&t6, &zl, t3, t5);
      Add12Cond(&zh, &zm, t4, t6);
    }
  }

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zm)));
  Add22(&t2h, &t2l, zh, zm, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));
  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_rd(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log1p_td_accurate(&logh, &logm, &logl, ed, index, zh, zm, zl, logih,
                         logim);
  return ReturnRoundDownwards3(logh, logm, logl);
}

inline gpga_double gpga_log1p_rz(gpga_double x) {
  gpga_double yh = gpga_double_zero(0u);
  gpga_double yl = gpga_double_zero(0u);
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yhrih = gpga_double_zero(0u);
  gpga_double yhril = gpga_double_zero(0u);
  gpga_double ylri = gpga_double_zero(0u);
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  gpga_double t4 = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logl = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  gpga_double sh = gpga_double_zero(0u);
  gpga_double sl = gpga_double_zero(0u);
  int E = 0;
  int index = 0;
  gpga_double one = gpga_double_from_u32(1u);

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (abs_hi < 0x3c900000u) {
    if (gpga_double_sign(x) == 0u && !gpga_double_is_zero(x)) {
      return gpga_double_next_down(x);
    }
    return x;
  }

  if ((xhi & 0x80000000u) != 0u && abs_hi >= 0x3ff00000u) {
    if (gpga_double_eq(x, gpga_double_neg(one))) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if ((xhi & 0x7ff00000u) == 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  if (abs_hi < 0x3f700000u) {
    logih = gpga_double_zero(0u);
    logim = gpga_double_zero(0u);
    index = 0;
    ed = gpga_double_zero(0u);
    zh = x;
    zm = gpga_double_zero(0u);
    zl = gpga_double_zero(0u);
  } else {
    Add12Cond(&sh, &sl, one, x);
    uint sh_hi = gpga_u64_hi(sh);
    uint sh_lo = gpga_u64_lo(sh);
    E += (int)((sh_hi >> 20) & 0x7ffu) - 1023;
    index = (int)(sh_hi & 0x000fffffU);
    sh_hi = (uint)index | 0x3ff00000u;
    index = (index + (1 << (20 - log_L - 1))) >> (20 - log_L);
    if (index >= (int)log_MAXINDEX) {
      sh_hi -= 0x00100000u;
      E += 1;
    }
    yh = gpga_u64_from_words(sh_hi, sh_lo);
    index &= (int)log_INDEXMASK;
    ed = gpga_double_from_s32(E);

    ri = log_argredtable[index].ri;
    logih = log_argredtable[index].logih;
    logim = log_argredtable[index].logim;

    if (gpga_double_is_zero(sl) || E > 125) {
      Mul12(&yhrih, &yhril, yh, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&zh, &zm, t1, yhril);
      zl = gpga_double_zero(0u);
    } else {
      int scale_exp = -E + 1023;
      gpga_double scale =
          gpga_u64_from_words((uint)scale_exp << 20, 0u);
      yl = gpga_double_mul(sl, scale);
      Mul12(&yhrih, &yhril, yh, ri);
      ylri = gpga_double_mul(yl, ri);
      t1 = gpga_double_sub(yhrih, one);
      Add12Cond(&t2, &t3, yhril, ylri);
      Add12Cond(&t4, &t5, t1, t2);
      Add12Cond(&t6, &zl, t3, t5);
      Add12Cond(&zh, &zm, t4, t6);
    }
  }

  polyHorner = gpga_double_add(log_c6, gpga_double_mul(zh, log_c7));
  polyHorner = gpga_double_add(log_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zm)));
  Add22(&t2h, &t2l, zh, zm, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log_log2h, ed),
        gpga_double_mul(log_log2m, ed));
  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  roundcst = (E == 0) ? log_RDROUNDCST1 : log_RDROUNDCST2;
  if (gpga_test_and_return_rz(logh, logm, roundcst, &res)) {
    return res;
  }

  gpga_log1p_td_accurate(&logh, &logm, &logl, ed, index, zh, zm, zl, logih,
                         logim);
  return ReturnRoundTowardsZero3(logh, logm, logl);
}

// CRLIBM_LOG2_TD
// LOG2_TD_CONSTANTS_BEGIN
// CRLIBM_LOG2_TD_CONSTANTS
GPGA_CONST uint log2_L = 7u;
GPGA_CONST uint log2_MAXINDEX = 53u;
GPGA_CONST uint log2_INDEXMASK = 127u;
GPGA_CONST gpga_double log2_two52 = 0x4330000000000000ul;
GPGA_CONST gpga_double log2_log2h = 0x3fe62e42fefa3800ul;
GPGA_CONST gpga_double log2_log2m = 0x3d2ef35793c76800ul;
GPGA_CONST gpga_double log2_log2l = 0xba59ff0342542fc3ul;
GPGA_CONST gpga_double log2_invh = 0x3ff71547652b82feul;
GPGA_CONST gpga_double log2_invl = 0x3c7777d0ffda0d24ul;
GPGA_CONST gpga_double log2_ROUNDCST1 = 0x3ff0204081020409ul;
GPGA_CONST gpga_double log2_ROUNDCST2 = 0x3ff0204081020409ul;
GPGA_CONST gpga_double log2_RDROUNDCST1 = 0x3c20000000000000ul;
GPGA_CONST gpga_double log2_RDROUNDCST2 = 0x3c20000000000000ul;
GPGA_CONST gpga_double log2_c3 = 0x3fd5555555555556ul;
GPGA_CONST gpga_double log2_c4 = 0xbfcffffffffafffaul;
GPGA_CONST gpga_double log2_c5 = 0x3fc99999998e0b4dul;
GPGA_CONST gpga_double log2_c6 = 0xbfc55569556623b2ul;
GPGA_CONST gpga_double log2_c7 = 0x3fc2493d75f51811ul;
GPGA_CONST gpga_double log2_accPolyC3h = 0x3fd5555555555555ul;
GPGA_CONST gpga_double log2_accPolyC3l = 0x3c75555555555555ul;
GPGA_CONST gpga_double log2_accPolyC4h = 0xbfd0000000000000ul;
GPGA_CONST gpga_double log2_accPolyC4l = 0x3937ffadc266bcb8ul;
GPGA_CONST gpga_double log2_accPolyC5h = 0x3fc999999999999aul;
GPGA_CONST gpga_double log2_accPolyC5l = 0xbc69999999866631ul;
GPGA_CONST gpga_double log2_accPolyC6h = 0xbfc5555555555555ul;
GPGA_CONST gpga_double log2_accPolyC6l = 0xbc655555559e546aul;
GPGA_CONST gpga_double log2_accPolyC7h = 0x3fc2492492492492ul;
GPGA_CONST gpga_double log2_accPolyC7l = 0x3c6248448ff5ae97ul;
GPGA_CONST gpga_double log2_accPolyC8h = 0xbfc0000000000000ul;
GPGA_CONST gpga_double log2_accPolyC8l = 0x3bb02fd4be5cfcddul;
GPGA_CONST gpga_double log2_accPolyC9h = 0x3fbc71c71c71c73aul;
GPGA_CONST gpga_double log2_accPolyC9l = 0x3c53fbe1792ad51cul;
GPGA_CONST gpga_double log2_accPolyC10 = 0xbfb99999999999ccul;
GPGA_CONST gpga_double log2_accPolyC11 = 0x3fb745d174237d2cul;
GPGA_CONST gpga_double log2_accPolyC12 = 0xbfb5555555095594ul;
GPGA_CONST gpga_double log2_accPolyC13 = 0x3fb3b16e4739debbul;
GPGA_CONST gpga_double log2_accPolyC14 = 0xbfb2495c92506ce8ul;
// LOG2_TD_CONSTANTS_END

inline void gpga_log2_td_accurate(thread gpga_double* logb2h,
                                  thread gpga_double* logb2m,
                                  thread gpga_double* logb2l, int E,
                                  gpga_double ed, int index, gpga_double zh,
                                  gpga_double zl, gpga_double logih,
                                  gpga_double logim) {
  (void)E;
  gpga_double highPoly = gpga_double_add(
      log2_accPolyC13, gpga_double_mul(zh, log2_accPolyC14));
  highPoly = gpga_double_add(log2_accPolyC12, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log2_accPolyC11, gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log2_accPolyC10, gpga_double_mul(zh, highPoly));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  gpga_double t5h = gpga_double_zero(0u);
  gpga_double t5l = gpga_double_zero(0u);
  gpga_double t6h = gpga_double_zero(0u);
  gpga_double t6l = gpga_double_zero(0u);
  gpga_double t7h = gpga_double_zero(0u);
  gpga_double t7l = gpga_double_zero(0u);
  gpga_double t8h = gpga_double_zero(0u);
  gpga_double t8l = gpga_double_zero(0u);
  gpga_double t9h = gpga_double_zero(0u);
  gpga_double t9l = gpga_double_zero(0u);
  gpga_double t10h = gpga_double_zero(0u);
  gpga_double t10l = gpga_double_zero(0u);
  gpga_double t11h = gpga_double_zero(0u);
  gpga_double t11l = gpga_double_zero(0u);
  gpga_double t12h = gpga_double_zero(0u);
  gpga_double t12l = gpga_double_zero(0u);
  gpga_double t13h = gpga_double_zero(0u);
  gpga_double t13l = gpga_double_zero(0u);
  gpga_double t14h = gpga_double_zero(0u);
  gpga_double t14l = gpga_double_zero(0u);

  Mul12(&t1h, &t1l, zh, highPoly);
  Add22(&t2h, &t2l, log2_accPolyC9h, log2_accPolyC9l, t1h, t1l);
  Mul22(&t3h, &t3l, zh, zl, t2h, t2l);
  Add22(&t4h, &t4l, log2_accPolyC8h, log2_accPolyC8l, t3h, t3l);
  Mul22(&t5h, &t5l, zh, zl, t4h, t4l);
  Add22(&t6h, &t6l, log2_accPolyC7h, log2_accPolyC7l, t5h, t5l);
  Mul22(&t7h, &t7l, zh, zl, t6h, t6l);
  Add22(&t8h, &t8l, log2_accPolyC6h, log2_accPolyC6l, t7h, t7l);
  Mul22(&t9h, &t9l, zh, zl, t8h, t8l);
  Add22(&t10h, &t10l, log2_accPolyC5h, log2_accPolyC5l, t9h, t9l);
  Mul22(&t11h, &t11l, zh, zl, t10h, t10l);
  Add22(&t12h, &t12l, log2_accPolyC4h, log2_accPolyC4l, t11h, t11l);
  Mul22(&t13h, &t13l, zh, zl, t12h, t12l);
  Add22(&t14h, &t14l, log2_accPolyC3h, log2_accPolyC3l, t13h, t13l);

  gpga_double zSquareh = gpga_double_zero(0u);
  gpga_double zSquarem = gpga_double_zero(0u);
  gpga_double zSquarel = gpga_double_zero(0u);
  gpga_double zCubeh = gpga_double_zero(0u);
  gpga_double zCubem = gpga_double_zero(0u);
  gpga_double zCubel = gpga_double_zero(0u);
  Mul23(&zSquareh, &zSquarem, &zSquarel, zh, zl, zh, zl);
  Mul233(&zCubeh, &zCubem, &zCubel, zh, zl, zSquareh, zSquarem, zSquarel);

  gpga_double higherPolyMultZh = gpga_double_zero(0u);
  gpga_double higherPolyMultZm = gpga_double_zero(0u);
  gpga_double higherPolyMultZl = gpga_double_zero(0u);
  Mul233(&higherPolyMultZh, &higherPolyMultZm, &higherPolyMultZl, t14h, t14l,
         zCubeh, zCubem, zCubel);

  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  gpga_double zSquareHalfh = gpga_double_mul(zSquareh, neg_half);
  gpga_double zSquareHalfm = gpga_double_mul(zSquarem, neg_half);
  gpga_double zSquareHalfl = gpga_double_mul(zSquarel, neg_half);

  gpga_double polyWithSquareh = gpga_double_zero(0u);
  gpga_double polyWithSquarem = gpga_double_zero(0u);
  gpga_double polyWithSquarel = gpga_double_zero(0u);
  Add33(&polyWithSquareh, &polyWithSquarem, &polyWithSquarel, zSquareHalfh,
        zSquareHalfm, zSquareHalfl, higherPolyMultZh, higherPolyMultZm,
        higherPolyMultZl);

  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polym = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  Add233(&polyh, &polym, &polyl, zh, zl, polyWithSquareh, polyWithSquarem,
         polyWithSquarel);

  gpga_double logil = log_argredtable[index].logil;
  gpga_double logyh = gpga_double_zero(0u);
  gpga_double logym = gpga_double_zero(0u);
  gpga_double logyl = gpga_double_zero(0u);
  Add33(&logyh, &logym, &logyl, logih, logim, logil, polyh, polym, polyl);

  gpga_double log2edh = gpga_double_mul(log2_log2h, ed);
  gpga_double log2edm = gpga_double_mul(log2_log2m, ed);
  gpga_double log2edl = gpga_double_mul(log2_log2l, ed);

  gpga_double loghover = gpga_double_zero(0u);
  gpga_double logmover = gpga_double_zero(0u);
  gpga_double loglover = gpga_double_zero(0u);
  Add33(&loghover, &logmover, &loglover, log2edh, log2edm, log2edl, logyh,
        logym, logyl);

  gpga_double logb2hover = gpga_double_zero(0u);
  gpga_double logb2mover = gpga_double_zero(0u);
  gpga_double logb2lover = gpga_double_zero(0u);
  Mul233(&logb2hover, &logb2mover, &logb2lover, log2_invh, log2_invl,
         loghover, logmover, loglover);

  Renormalize3(logb2h, logb2m, logb2l, logb2hover, logb2mover, logb2lover);
}

inline gpga_double gpga_log2_rn(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb2h = gpga_double_zero(0u);
  gpga_double logb2m = gpga_double_zero(0u);
  gpga_double logb2l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log2_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log2_L - 1))) >> (20 - log2_L);
  if (index >= (int)log2_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log2_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log2_c6, gpga_double_mul(zh, log2_c7));
  polyHorner = gpga_double_add(log2_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log2_log2h, ed),
        gpga_double_mul(log2_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb2h, &logb2m, log2_invh, log2_invl, logh, logm);

  roundcst = (E == 0) ? log2_ROUNDCST1 : log2_ROUNDCST2;
  gpga_double test = gpga_double_add(logb2h, gpga_double_mul(logb2m, roundcst));
  if (gpga_double_eq(logb2h, test)) {
    return logb2h;
  }

  gpga_log2_td_accurate(&logb2h, &logb2m, &logb2l, E, ed, index, zh, zl,
                        logih, logim);
  return ReturnRoundToNearest3(logb2h, logb2m, logb2l);
}

inline gpga_double gpga_log2_ru(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb2h = gpga_double_zero(0u);
  gpga_double logb2m = gpga_double_zero(0u);
  gpga_double logb2l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log2_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log2_L - 1))) >> (20 - log2_L);
  if (index >= (int)log2_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log2_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log2_c6, gpga_double_mul(zh, log2_c7));
  polyHorner = gpga_double_add(log2_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log2_log2h, ed),
        gpga_double_mul(log2_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb2h, &logb2m, log2_invh, log2_invl, logh, logm);

  roundcst = (E == 0) ? log2_RDROUNDCST1 : log2_RDROUNDCST2;
  if (gpga_test_and_return_ru(logb2h, logb2m, roundcst, &res)) {
    return res;
  }

  gpga_log2_td_accurate(&logb2h, &logb2m, &logb2l, E, ed, index, zh, zl,
                        logih, logim);
  return ReturnRoundUpwards3(logb2h, logb2m, logb2l);
}

inline gpga_double gpga_log2_rd(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb2h = gpga_double_zero(0u);
  gpga_double logb2m = gpga_double_zero(0u);
  gpga_double logb2l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log2_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log2_L - 1))) >> (20 - log2_L);
  if (index >= (int)log2_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log2_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log2_c6, gpga_double_mul(zh, log2_c7));
  polyHorner = gpga_double_add(log2_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log2_log2h, ed),
        gpga_double_mul(log2_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb2h, &logb2m, log2_invh, log2_invl, logh, logm);

  roundcst = (E == 0) ? log2_RDROUNDCST1 : log2_RDROUNDCST2;
  if (gpga_test_and_return_rd(logb2h, logb2m, roundcst, &res)) {
    return res;
  }

  gpga_log2_td_accurate(&logb2h, &logb2m, &logb2l, E, ed, index, zh, zl,
                        logih, logim);
  return ReturnRoundDownwards3(logb2h, logb2m, logb2l);
}

inline gpga_double gpga_log2_rz(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb2h = gpga_double_zero(0u);
  gpga_double logb2m = gpga_double_zero(0u);
  gpga_double logb2l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log2_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log2_L - 1))) >> (20 - log2_L);
  if (index >= (int)log2_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log2_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log2_c6, gpga_double_mul(zh, log2_c7));
  polyHorner = gpga_double_add(log2_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log2_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log2_log2h, ed),
        gpga_double_mul(log2_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb2h, &logb2m, log2_invh, log2_invl, logh, logm);

  roundcst = (E == 0) ? log2_RDROUNDCST1 : log2_RDROUNDCST2;
  if (gpga_test_and_return_rz(logb2h, logb2m, roundcst, &res)) {
    return res;
  }

  gpga_log2_td_accurate(&logb2h, &logb2m, &logb2l, E, ed, index, zh, zl,
                        logih, logim);
  return ReturnRoundTowardsZero3(logb2h, logb2m, logb2l);
}

// CRLIBM_LOG10_TD
// LOG10_TD_CONSTANTS_BEGIN
// CRLIBM_LOG10_TD_CONSTANTS
GPGA_CONST uint log10_L = 7u;
GPGA_CONST uint log10_MAXINDEX = 53u;
GPGA_CONST uint log10_INDEXMASK = 127u;
GPGA_CONST gpga_double log10_two52 = 0x4330000000000000ul;
GPGA_CONST gpga_double log10_log2h = 0x3fe62e42fefa3800ul;
GPGA_CONST gpga_double log10_log2m = 0x3d2ef35793c76800ul;
GPGA_CONST gpga_double log10_log2l = 0xba59ff0342542fc3ul;
GPGA_CONST gpga_double log10_invh = 0x3fdbcb7b1526e50eul;
GPGA_CONST gpga_double log10_invm = 0x3c695355baaafad3ul;
GPGA_CONST gpga_double log10_invl = 0x38fee191f71a3012ul;
GPGA_CONST gpga_double log10_worstcase = 0x4790000000000000ul;
GPGA_CONST gpga_double log10_ROUNDCST1 = 0x3ff0204081020409ul;
GPGA_CONST gpga_double log10_ROUNDCST2 = 0x3ff0204081020409ul;
GPGA_CONST gpga_double log10_RDROUNDCST1 = 0x3c20000000000000ul;
GPGA_CONST gpga_double log10_RDROUNDCST2 = 0x3c20000000000000ul;
GPGA_CONST gpga_double log10_c3 = 0x3fd5555555555556ul;
GPGA_CONST gpga_double log10_c4 = 0xbfcffffffffafffaul;
GPGA_CONST gpga_double log10_c5 = 0x3fc99999998e0b4dul;
GPGA_CONST gpga_double log10_c6 = 0xbfc55569556623b2ul;
GPGA_CONST gpga_double log10_c7 = 0x3fc2493d75f51811ul;
GPGA_CONST gpga_double log10_accPolyC3h = 0x3fd5555555555555ul;
GPGA_CONST gpga_double log10_accPolyC3l = 0x3c75555555555555ul;
GPGA_CONST gpga_double log10_accPolyC4h = 0xbfd0000000000000ul;
GPGA_CONST gpga_double log10_accPolyC4l = 0x3937ffadc266bcb8ul;
GPGA_CONST gpga_double log10_accPolyC5h = 0x3fc999999999999aul;
GPGA_CONST gpga_double log10_accPolyC5l = 0xbc69999999866631ul;
GPGA_CONST gpga_double log10_accPolyC6h = 0xbfc5555555555555ul;
GPGA_CONST gpga_double log10_accPolyC6l = 0xbc655555559e546aul;
GPGA_CONST gpga_double log10_accPolyC7h = 0x3fc2492492492492ul;
GPGA_CONST gpga_double log10_accPolyC7l = 0x3c6248448ff5ae97ul;
GPGA_CONST gpga_double log10_accPolyC8h = 0xbfc0000000000000ul;
GPGA_CONST gpga_double log10_accPolyC8l = 0x3bb02fd4be5cfcddul;
GPGA_CONST gpga_double log10_accPolyC9h = 0x3fbc71c71c71c73aul;
GPGA_CONST gpga_double log10_accPolyC9l = 0x3c53fbe1792ad51cul;
GPGA_CONST gpga_double log10_accPolyC10 = 0xbfb99999999999ccul;
GPGA_CONST gpga_double log10_accPolyC11 = 0x3fb745d174237d2cul;
GPGA_CONST gpga_double log10_accPolyC12 = 0xbfb5555555095594ul;
GPGA_CONST gpga_double log10_accPolyC13 = 0x3fb3b16e4739debbul;
GPGA_CONST gpga_double log10_accPolyC14 = 0xbfb2495c92506ce8ul;
// LOG10_TD_CONSTANTS_END

inline void gpga_log10_td_accurate(thread gpga_double* logb10h,
                                   thread gpga_double* logb10m,
                                   thread gpga_double* logb10l, int E,
                                   gpga_double ed, int index, gpga_double zh,
                                   gpga_double zl, gpga_double logih,
                                   gpga_double logim) {
  (void)E;
  gpga_double highPoly = gpga_double_add(
      log10_accPolyC13, gpga_double_mul(zh, log10_accPolyC14));
  highPoly = gpga_double_add(log10_accPolyC12,
                             gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log10_accPolyC11,
                             gpga_double_mul(zh, highPoly));
  highPoly = gpga_double_add(log10_accPolyC10,
                             gpga_double_mul(zh, highPoly));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  gpga_double t5h = gpga_double_zero(0u);
  gpga_double t5l = gpga_double_zero(0u);
  gpga_double t6h = gpga_double_zero(0u);
  gpga_double t6l = gpga_double_zero(0u);
  gpga_double t7h = gpga_double_zero(0u);
  gpga_double t7l = gpga_double_zero(0u);
  gpga_double t8h = gpga_double_zero(0u);
  gpga_double t8l = gpga_double_zero(0u);
  gpga_double t9h = gpga_double_zero(0u);
  gpga_double t9l = gpga_double_zero(0u);
  gpga_double t10h = gpga_double_zero(0u);
  gpga_double t10l = gpga_double_zero(0u);
  gpga_double t11h = gpga_double_zero(0u);
  gpga_double t11l = gpga_double_zero(0u);
  gpga_double t12h = gpga_double_zero(0u);
  gpga_double t12l = gpga_double_zero(0u);
  gpga_double t13h = gpga_double_zero(0u);
  gpga_double t13l = gpga_double_zero(0u);
  gpga_double t14h = gpga_double_zero(0u);
  gpga_double t14l = gpga_double_zero(0u);

  Mul12(&t1h, &t1l, zh, highPoly);
  Add22(&t2h, &t2l, log10_accPolyC9h, log10_accPolyC9l, t1h, t1l);
  Mul22(&t3h, &t3l, zh, zl, t2h, t2l);
  Add22(&t4h, &t4l, log10_accPolyC8h, log10_accPolyC8l, t3h, t3l);
  Mul22(&t5h, &t5l, zh, zl, t4h, t4l);
  Add22(&t6h, &t6l, log10_accPolyC7h, log10_accPolyC7l, t5h, t5l);
  Mul22(&t7h, &t7l, zh, zl, t6h, t6l);
  Add22(&t8h, &t8l, log10_accPolyC6h, log10_accPolyC6l, t7h, t7l);
  Mul22(&t9h, &t9l, zh, zl, t8h, t8l);
  Add22(&t10h, &t10l, log10_accPolyC5h, log10_accPolyC5l, t9h, t9l);
  Mul22(&t11h, &t11l, zh, zl, t10h, t10l);
  Add22(&t12h, &t12l, log10_accPolyC4h, log10_accPolyC4l, t11h, t11l);
  Mul22(&t13h, &t13l, zh, zl, t12h, t12l);
  Add22(&t14h, &t14l, log10_accPolyC3h, log10_accPolyC3l, t13h, t13l);

  gpga_double zSquareh = gpga_double_zero(0u);
  gpga_double zSquarem = gpga_double_zero(0u);
  gpga_double zSquarel = gpga_double_zero(0u);
  gpga_double zCubeh = gpga_double_zero(0u);
  gpga_double zCubem = gpga_double_zero(0u);
  gpga_double zCubel = gpga_double_zero(0u);
  Mul23(&zSquareh, &zSquarem, &zSquarel, zh, zl, zh, zl);
  Mul233(&zCubeh, &zCubem, &zCubel, zh, zl, zSquareh, zSquarem, zSquarel);

  gpga_double higherPolyMultZh = gpga_double_zero(0u);
  gpga_double higherPolyMultZm = gpga_double_zero(0u);
  gpga_double higherPolyMultZl = gpga_double_zero(0u);
  Mul233(&higherPolyMultZh, &higherPolyMultZm, &higherPolyMultZl, t14h, t14l,
         zCubeh, zCubem, zCubel);

  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  gpga_double zSquareHalfh = gpga_double_mul(zSquareh, neg_half);
  gpga_double zSquareHalfm = gpga_double_mul(zSquarem, neg_half);
  gpga_double zSquareHalfl = gpga_double_mul(zSquarel, neg_half);

  gpga_double polyWithSquareh = gpga_double_zero(0u);
  gpga_double polyWithSquarem = gpga_double_zero(0u);
  gpga_double polyWithSquarel = gpga_double_zero(0u);
  Add33(&polyWithSquareh, &polyWithSquarem, &polyWithSquarel, zSquareHalfh,
        zSquareHalfm, zSquareHalfl, higherPolyMultZh, higherPolyMultZm,
        higherPolyMultZl);

  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polym = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  Add233(&polyh, &polym, &polyl, zh, zl, polyWithSquareh, polyWithSquarem,
         polyWithSquarel);

  gpga_double logil = log_argredtable[index].logil;
  gpga_double logyh = gpga_double_zero(0u);
  gpga_double logym = gpga_double_zero(0u);
  gpga_double logyl = gpga_double_zero(0u);
  Add33(&logyh, &logym, &logyl, logih, logim, logil, polyh, polym, polyl);

  gpga_double logyhnorm = gpga_double_zero(0u);
  gpga_double logymnorm = gpga_double_zero(0u);
  gpga_double logylnorm = gpga_double_zero(0u);
  Renormalize3(&logyhnorm, &logymnorm, &logylnorm, logyh, logym, logyl);

  gpga_double log2edh = gpga_double_mul(log10_log2h, ed);
  gpga_double log2edm = gpga_double_mul(log10_log2m, ed);
  gpga_double log2edl = gpga_double_mul(log10_log2l, ed);

  gpga_double loghover = gpga_double_zero(0u);
  gpga_double logmover = gpga_double_zero(0u);
  gpga_double loglover = gpga_double_zero(0u);
  Add33(&loghover, &logmover, &loglover, log2edh, log2edm, log2edl, logyhnorm,
        logymnorm, logylnorm);

  gpga_double logb10hover = gpga_double_zero(0u);
  gpga_double logb10mover = gpga_double_zero(0u);
  gpga_double logb10lover = gpga_double_zero(0u);
  Mul33(&logb10hover, &logb10mover, &logb10lover, log10_invh, log10_invm,
        log10_invl, loghover, logmover, loglover);

  Renormalize3(logb10h, logb10m, logb10l, logb10hover, logb10mover,
               logb10lover);
}

inline gpga_double gpga_log10_rn(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb10h = gpga_double_zero(0u);
  gpga_double logb10m = gpga_double_zero(0u);
  gpga_double logb10l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log10_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log10_L - 1))) >> (20 - log10_L);
  if (index >= (int)log10_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log10_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log10_c6, gpga_double_mul(zh, log10_c7));
  polyHorner = gpga_double_add(log10_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log10_log2h, ed),
        gpga_double_mul(log10_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb10h, &logb10m, log10_invh, log10_invm, logh, logm);

  roundcst = (E == 0) ? log10_ROUNDCST1 : log10_ROUNDCST2;
  gpga_double test = gpga_double_add(logb10h,
                                     gpga_double_mul(logb10m, roundcst));
  if (gpga_double_eq(logb10h, test)) {
    return logb10h;
  }

  gpga_log10_td_accurate(&logb10h, &logb10m, &logb10l, E, ed, index, zh, zl,
                         logih, logim);
  return ReturnRoundToNearest3(logb10h, logb10m, logb10l);
}

inline gpga_double gpga_log10_ru(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb10h = gpga_double_zero(0u);
  gpga_double logb10m = gpga_double_zero(0u);
  gpga_double logb10l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log10_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log10_L - 1))) >> (20 - log10_L);
  if (index >= (int)log10_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log10_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log10_c6, gpga_double_mul(zh, log10_c7));
  polyHorner = gpga_double_add(log10_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log10_log2h, ed),
        gpga_double_mul(log10_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb10h, &logb10m, log10_invh, log10_invm, logh, logm);

  roundcst = (E == 0) ? log10_RDROUNDCST1 : log10_RDROUNDCST2;
  if (gpga_test_and_return_ru(logb10h, logb10m, roundcst, &res)) {
    return res;
  }

  gpga_log10_td_accurate(&logb10h, &logb10m, &logb10l, E, ed, index, zh, zl,
                         logih, logim);
  return ReturnRoundUpwards3Unfiltered(logb10h, logb10m, logb10l,
                                       log10_worstcase);
}

inline gpga_double gpga_log10_rd(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb10h = gpga_double_zero(0u);
  gpga_double logb10m = gpga_double_zero(0u);
  gpga_double logb10l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log10_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log10_L - 1))) >> (20 - log10_L);
  if (index >= (int)log10_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log10_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log10_c6, gpga_double_mul(zh, log10_c7));
  polyHorner = gpga_double_add(log10_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log10_log2h, ed),
        gpga_double_mul(log10_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb10h, &logb10m, log10_invh, log10_invm, logh, logm);

  roundcst = (E == 0) ? log10_RDROUNDCST1 : log10_RDROUNDCST2;
  if (gpga_test_and_return_rd(logb10h, logb10m, roundcst, &res)) {
    return res;
  }

  gpga_log10_td_accurate(&logb10h, &logb10m, &logb10l, E, ed, index, zh, zl,
                         logih, logim);
  return ReturnRoundDownwards3Unfiltered(logb10h, logb10m, logb10l,
                                         log10_worstcase);
}

inline gpga_double gpga_log10_rz(gpga_double x) {
  gpga_double y = x;
  gpga_double ed = gpga_double_zero(0u);
  gpga_double ri = gpga_double_zero(0u);
  gpga_double logih = gpga_double_zero(0u);
  gpga_double logim = gpga_double_zero(0u);
  gpga_double yrih = gpga_double_zero(0u);
  gpga_double yril = gpga_double_zero(0u);
  gpga_double th = gpga_double_zero(0u);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zl = gpga_double_zero(0u);
  gpga_double polyHorner = gpga_double_zero(0u);
  gpga_double zhSquareh = gpga_double_zero(0u);
  gpga_double zhSquarel = gpga_double_zero(0u);
  gpga_double polyUpper = gpga_double_zero(0u);
  gpga_double zhSquareHalfh = gpga_double_zero(0u);
  gpga_double zhSquareHalfl = gpga_double_zero(0u);
  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  gpga_double ph = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  gpga_double log2edh = gpga_double_zero(0u);
  gpga_double log2edl = gpga_double_zero(0u);
  gpga_double logTabPolyh = gpga_double_zero(0u);
  gpga_double logTabPolyl = gpga_double_zero(0u);
  gpga_double logh = gpga_double_zero(0u);
  gpga_double logm = gpga_double_zero(0u);
  gpga_double logb10h = gpga_double_zero(0u);
  gpga_double logb10m = gpga_double_zero(0u);
  gpga_double logb10l = gpga_double_zero(0u);
  gpga_double roundcst = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int E = 0;
  int index = 0;

  uint xhi = gpga_u64_hi(x);
  uint xlo = gpga_u64_lo(x);
  uint abs_hi = xhi & 0x7fffffffU;

  if (gpga_double_sign(x) != 0u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    return gpga_double_nan();
  }

  if (abs_hi < 0x00100000u) {
    if ((abs_hi | xlo) == 0u) {
      return gpga_double_inf(1u);
    }
    E = -52;
    x = gpga_double_mul(x, log10_two52);
    xhi = gpga_u64_hi(x);
    xlo = gpga_u64_lo(x);
    abs_hi = xhi & 0x7fffffffU;
  }

  if (abs_hi >= 0x7ff00000u) {
    return gpga_double_add(x, x);
  }

  E += (int)((xhi >> 20) & 0x7ffu) - 1023;
  index = (int)(xhi & 0x000fffffU);
  xhi = (uint)index | 0x3ff00000u;
  index = (index + (1 << (20 - log10_L - 1))) >> (20 - log10_L);
  if (index >= (int)log10_MAXINDEX) {
    xhi -= 0x00100000u;
    E += 1;
  }
  y = gpga_u64_from_words(xhi, xlo);
  index &= (int)log10_INDEXMASK;
  ed = gpga_double_from_s32(E);

  ri = log_argredtable[index].ri;
  logih = log_argredtable[index].logih;
  logim = log_argredtable[index].logim;

  Mul12(&yrih, &yril, y, ri);
  th = gpga_double_sub(yrih, gpga_double_from_u32(1u));
  Add12Cond(&zh, &zl, th, yril);

  polyHorner = gpga_double_add(log10_c6, gpga_double_mul(zh, log10_c7));
  polyHorner = gpga_double_add(log10_c5, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c4, gpga_double_mul(zh, polyHorner));
  polyHorner = gpga_double_add(log10_c3, gpga_double_mul(zh, polyHorner));

  Mul12(&zhSquareh, &zhSquarel, zh, zh);
  polyUpper = gpga_double_mul(polyHorner, gpga_double_mul(zh, zhSquareh));
  gpga_double neg_half = gpga_double_neg(gpga_double_const_inv2());
  zhSquareHalfh = gpga_double_mul(zhSquareh, neg_half);
  zhSquareHalfl = gpga_double_mul(zhSquarel, neg_half);
  Add12(&t1h, &t1l, polyUpper, gpga_double_neg(gpga_double_mul(zh, zl)));
  Add22(&t2h, &t2l, zh, zl, zhSquareHalfh, zhSquareHalfl);
  Add22(&ph, &pl, t2h, t2l, t1h, t1l);

  Add12(&log2edh, &log2edl, gpga_double_mul(log10_log2h, ed),
        gpga_double_mul(log10_log2m, ed));

  Add22Cond(&logTabPolyh, &logTabPolyl, logih, logim, ph, pl);
  Add22Cond(&logh, &logm, log2edh, log2edl, logTabPolyh, logTabPolyl);

  Mul22(&logb10h, &logb10m, log10_invh, log10_invm, logh, logm);

  roundcst = (E == 0) ? log10_RDROUNDCST1 : log10_RDROUNDCST2;
  if (gpga_test_and_return_rz(logb10h, logb10m, roundcst, &res)) {
    return res;
  }

  gpga_log10_td_accurate(&logb10h, &logb10m, &logb10l, E, ed, index, zh, zl,
                         logih, logim);
  return ReturnRoundTowardsZero3Unfiltered(logb10h, logb10m, logb10l,
                                           log10_worstcase);
}

// CRLIBM_EXP_TD
struct GpgaExpTableEntry {
  gpga_double hi;
  gpga_double mi;
  gpga_double lo;
};
// EXP_TD_CONSTANTS_BEGIN
// CRLIBM_EXP_TD_CONSTANTS
GPGA_CONST uint exp_L = 12u;
GPGA_CONST uint exp_LHALF = 6u;
GPGA_CONST gpga_double exp_log2InvMult2L = 0x40b71547652b82feul;
GPGA_CONST gpga_double exp_msLog2Div2Lh = 0xbf262e42fefa39eful;
GPGA_CONST gpga_double exp_msLog2Div2Lm = 0xbbbabc9e3b39803ful;
GPGA_CONST gpga_double exp_msLog2Div2Ll = 0xb847b57a079a1934ul;
GPGA_CONST gpga_double exp_shiftConst = 0x4338000000000000ul;
GPGA_CONST uint exp_INDEXMASK1 = 0x0000003fu;
GPGA_CONST uint exp_INDEXMASK2 = 0x00000fc0u;
GPGA_CONST uint exp_OVRUDRFLWSMPLBOUND = 0x4086232bu;
GPGA_CONST gpga_double exp_OVRFLWBOUND = 0x40862e42fefa39eful;
GPGA_CONST gpga_double exp_LARGEST = 0x7feffffffffffffful;
GPGA_CONST gpga_double exp_SMALLEST = 0x0000000000000001ul;
GPGA_CONST gpga_double exp_DENORMBOUND = 0xc086232bdd7abcd2ul;
GPGA_CONST gpga_double exp_UNDERFLWBOUND = 0xc0874910d52d3052ul;
GPGA_CONST gpga_double exp_twoPowerM1000 = 0x0170000000000000ul;
GPGA_CONST gpga_double exp_twoPower1000 = 0x7e70000000000000ul;
GPGA_CONST gpga_double exp_ROUNDCST = 0x3ff0040100401005ul;
GPGA_CONST gpga_double exp_RDROUNDCST = 0x3bf0000000000000ul;
GPGA_CONST gpga_double exp_twoM52 = 0x3cb0000000000000ul;
GPGA_CONST gpga_double exp_mTwoM53 = 0xbca0000000000000ul;
GPGA_CONST gpga_double exp_c3 = 0x3fc555555565bb99ul;
GPGA_CONST gpga_double exp_c4 = 0x3fa55555556b3304ul;
GPGA_CONST gpga_double exp_accPolyC3h = 0x3fc5555555555555ul;
GPGA_CONST gpga_double exp_accPolyC3l = 0x3c6555555555557eul;
GPGA_CONST gpga_double exp_accPolyC4h = 0x3fa5555555555555ul;
GPGA_CONST gpga_double exp_accPolyC4l = 0x3c45546534ca3666ul;
GPGA_CONST gpga_double exp_accPolyC5 = 0x3f81111111111111ul;
GPGA_CONST gpga_double exp_accPolyC6 = 0x3f56c16c16d10a6ful;
GPGA_CONST gpga_double exp_accPolyC7 = 0x3f2a01a01a150cf9ul;
GPGA_CONST GpgaExpTableEntry exp_twoPowerIndex1[64] = {
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3ff000b175effdc7ul, 0x3c9ae8e38c59c72aul, 0x39339726694630e3ul },
  { 0x3ff00162f3904052ul, 0xbc57b5d0d58ea8f4ul, 0x38fe5e06ddd31156ul },
  { 0x3ff0021478e11ce6ul, 0x3c94115cb6b16a8eul, 0x3905a0768b51f609ul },
  { 0x3ff002c605e2e8cful, 0xbc8d7c96f201bb2ful, 0x390d008403605217ul },
  { 0x3ff003779a95f959ul, 0x3c984711d4c35e9ful, 0x39289bc16f765708ul },
  { 0x3ff0042936faa3d8ul, 0xbc80484245243777ul, 0xb924535b7f8c1e2dul },
  { 0x3ff004dadb113da0ul, 0xbc94b237da2025f9ul, 0xb938ba92f6b25456ul },
  { 0x3ff0058c86da1c0aul, 0xbc75e00e62d6b30dul, 0xb8e30c72e81f4294ul },
  { 0x3ff0063e3a559473ul, 0x3c9a1d6cedbb9481ul, 0xb9134a5384e6f0b9ul },
  { 0x3ff006eff583fc3dul, 0xbc94acf197a00142ul, 0x393f8d0580865d2eul },
  { 0x3ff007a1b865a8caul, 0xbc6eaf2ea42391a5ul, 0xb90002bcb3ae9a99ul },
  { 0x3ff0085382faef83ul, 0x3c7da93f90835f75ul, 0x390c3c5aedee9851ul },
  { 0x3ff00905554425d4ul, 0xbc86a79084ab093cul, 0x3927217851d1ec6eul },
  { 0x3ff009b72f41a12bul, 0x3c986364f8fbe8f8ul, 0xb9180cbca335a7c3ul },
  { 0x3ff00a6910f3b6fdul, 0xbc882e8e14e3110eul, 0xb91706bd4eb22595ul },
  { 0x3ff00b1afa5abcbful, 0xbc84f6b2a7609f71ul, 0xb90b55dd523f3c08ul },
  { 0x3ff00bcceb7707ecul, 0xbc7e1a258ea8f71bul, 0x39190a1e207cced1ul },
  { 0x3ff00c7ee448ee02ul, 0x3c74362ca5bc26f1ul, 0x39178d0472db37c5ul },
  { 0x3ff00d30e4d0c483ul, 0x3c9095a56c919d02ul, 0xb92bcd4db3cb52feul },
  { 0x3ff00de2ed0ee0f5ul, 0xbc6406ac4e81a645ul, 0xb8fcf1b131575ec2ul },
  { 0x3ff00e94fd0398e0ul, 0x3c9b5a6902767e09ul, 0xb8f6aaa1fa7ff913ul },
  { 0x3ff00f4714af41d3ul, 0xbc991b2060859321ul, 0x39168f236dff3218ul },
  { 0x3ff00ff93412315cul, 0x3c8427068ab22306ul, 0xb92e8bb58067e60aul },
  { 0x3ff010ab5b2cbd11ul, 0x3c9c1d0660524e08ul, 0x393d4cd5e1d71fdful },
  { 0x3ff0115d89ff3a8bul, 0xbc9e7bdfb3204be8ul, 0x393e4ecf350ebe88ul },
  { 0x3ff0120fc089ff63ul, 0x3c8843aa8b9cbbc6ul, 0x3926a2aa2c89c4f8ul },
  { 0x3ff012c1fecd613bul, 0xbc734104ee7edae9ul, 0x3911ca368a20ed05ul },
  { 0x3ff0137444c9b5b5ul, 0xbc72b6aeb6176892ul, 0x38dedb1095d925cful },
  { 0x3ff01426927f5278ul, 0x3c7a8cd33b8a1bb3ul, 0xb90488c78eded75ful },
  { 0x3ff014d8e7ee8d2ful, 0x3c72edc08e5da99aul, 0xb8e7480f5ea1b3c9ul },
  { 0x3ff0158b4517bb88ul, 0x3c857ba2dc7e0c73ul, 0xb90ae45989a04dd5ul },
  { 0x3ff0163da9fb3335ul, 0x3c9b61299ab8cdb7ul, 0x392bf48007d80987ul },
  { 0x3ff016f0169949edul, 0xbc990565902c5f44ul, 0x3921aa91a059292cul },
  { 0x3ff017a28af25567ul, 0x3c870fc41c5c2d53ul, 0x391b6663292855f5ul },
  { 0x3ff018550706ab62ul, 0x3c94b9a6e145d76cul, 0x393e7fbca6793d94ul },
  { 0x3ff019078ad6a19ful, 0xbc7008eff5142bf9ul, 0xb915b9f5c7de3b93ul },
  { 0x3ff019ba16628de2ul, 0xbc977669f033c7deul, 0x3914638bf2f6acabul },
  { 0x3ff01a6ca9aac5f3ul, 0xbc909bb78eeead0aul, 0xb92ab237b9a069c5ul },
  { 0x3ff01b1f44af9f9eul, 0x3c9371231477ece5ul, 0x3933ab358be97ceful },
  { 0x3ff01bd1e77170b4ul, 0x3c75e7626621eb5bul, 0xb914027b2294bb64ul },
  { 0x3ff01c8491f08f08ul, 0xbc9bc72b100828a5ul, 0x390656394426c990ul },
  { 0x3ff01d37442d5070ul, 0xbc6ce39cbbab8bbeul, 0x390bf9785189bdd8ul },
  { 0x3ff01de9fe280ac8ul, 0x3c816996709da2e2ul, 0x3927c12f86114fe3ul },
  { 0x3ff01e9cbfe113eful, 0xbc8c11f5239bf535ul, 0xb92653d5d24b5d28ul },
  { 0x3ff01f4f8958c1c6ul, 0x3c8e1d4eb5edc6b3ul, 0x39204a0cdc1d86d7ul },
  { 0x3ff020025a8f6a35ul, 0xbc9afb99946ee3f0ul, 0x392c678c46149782ul },
  { 0x3ff020b533856324ul, 0xbc98f06d8a148a32ul, 0x39348524e1e9df70ul },
  { 0x3ff02168143b0281ul, 0xbc82bf310fc54eb6ul, 0x3929953ea727ff0bul },
  { 0x3ff0221afcb09e3eul, 0xbc9c95a035eb4175ul, 0xb93ccfbbec22d28eul },
  { 0x3ff022cdece68c4ful, 0xbc9491793e46834dul, 0x3939e2bb6e181de1ul },
  { 0x3ff02380e4dd22adul, 0xbc73e8d0d9c49091ul, 0x391f17609ae29308ul },
  { 0x3ff02433e494b755ul, 0xbc9314aa16278aa3ul, 0xb91c7dc2c476bfb8ul },
  { 0x3ff024e6ec0da046ul, 0x3c848daf888e9651ul, 0xb92fab994971d4a3ul },
  { 0x3ff02599fb483385ul, 0x3c856dc8046821f4ul, 0x392848b62cbdd0aful },
  { 0x3ff0264d1244c719ul, 0x3c945b42356b9d47ul, 0xb92bf603ba715d0cul },
  { 0x3ff027003103b10eul, 0xbc7082ef51b61d7eul, 0x39189434e751e1aaul },
  { 0x3ff027b357854772ul, 0x3c72106ed0920a34ul, 0xb9103b54fd64e8acul },
  { 0x3ff0286685c9e059ul, 0xbc9fd4cf26ea5d0ful, 0x3927785ea0acc486ul },
  { 0x3ff02919bbd1d1d8ul, 0xbc909f8775e78084ul, 0xb92ce447fdb35ff9ul },
  { 0x3ff029ccf99d720aul, 0x3c564cbba902ca27ul, 0x38f5b884aab5642aul },
  { 0x3ff02a803f2d170dul, 0x3c94383ef231d207ul, 0xb93cfb3e46d7c1c0ul },
  { 0x3ff02b338c811703ul, 0x3c94a47a505b3a47ul, 0xb8f0d40cee4b81aful },
  { 0x3ff02be6e199c811ul, 0x3c9e47120223467ful, 0x3926ae7d36d7c1f7ul },
};

GPGA_CONST GpgaExpTableEntry exp_twoPowerIndex2[64] = {
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3ff02c9a3e778061ul, 0xbc719083535b085dul, 0xb919085b0a3d74d5ul },
  { 0x3ff059b0d3158574ul, 0x3c8d73e2a475b465ul, 0x39105ff94f8d257eul },
  { 0x3ff0874518759bc8ul, 0x3c6186be4bb284fful, 0x39015820d96b414ful },
  { 0x3ff0b5586cf9890ful, 0x3c98a62e4adc610bul, 0xb9367c9bd6ebf74cul },
  { 0x3ff0e3ec32d3d1a2ul, 0x3c403a1727c57b53ul, 0xb8e5aa76994e9ddbul },
  { 0x3ff11301d0125b51ul, 0xbc96c51039449b3aul, 0x3929d58b988f562dul },
  { 0x3ff1429aaea92de0ul, 0xbc932fbf9af1369eul, 0xb932fe7bb4c76416ul },
  { 0x3ff172b83c7d517bul, 0xbc819041b9d78a76ul, 0x3924f2406aa13ff0ul },
  { 0x3ff1a35beb6fcb75ul, 0x3c8e5b4c7b4968e4ul, 0x390ad36183926ae8ul },
  { 0x3ff1d4873168b9aaul, 0x3c9e016e00a2643cul, 0x391ea62d0881b918ul },
  { 0x3ff2063b88628cd6ul, 0x3c8dc775814a8495ul, 0xb90781dbc16f1ea4ul },
  { 0x3ff2387a6e756238ul, 0x3c99b07eb6c70573ul, 0xb924d89f9af532e0ul },
  { 0x3ff26b4565e27cddul, 0x3c82bd339940e9d9ul, 0x391277393a461b77ul },
  { 0x3ff29e9df51fdee1ul, 0x3c8612e8afad1255ul, 0x390de54485604690ul },
  { 0x3ff2d285a6e4030bul, 0x3c90024754db41d5ul, 0xb91ee9d8f8cb9307ul },
  { 0x3ff306fe0a31b715ul, 0x3c86f46ad23182e4ul, 0x3917b7b2f09cd0d9ul },
  { 0x3ff33c08b26416fful, 0x3c932721843659a6ul, 0xb93406a2ea6cfc6bul },
  { 0x3ff371a7373aa9cbul, 0xbc963aeabf42eae2ul, 0x39387e3e12516bfaul },
  { 0x3ff3a7db34e59ff7ul, 0xbc75e436d661f5e3ul, 0x3909b0b1ff17c296ul },
  { 0x3ff3dea64c123422ul, 0x3c8ada0911f09ebcul, 0xb92808ba68fa8fb7ul },
  { 0x3ff4160a21f72e2aul, 0xbc5ef3691c309278ul, 0xb8d32b43eafc6518ul },
  { 0x3ff44e086061892dul, 0x3c489b7a04ef80d0ul, 0xb8d0ac312de3d922ul },
  { 0x3ff486a2b5c13cd0ul, 0x3c73c1a3b69062f0ul, 0x390e1eebae743ac0ul },
  { 0x3ff4bfdad5362a27ul, 0x3c7d4397afec42e2ul, 0x38ec06c7745c2b39ul },
  { 0x3ff4f9b2769d2ca7ul, 0xbc94b309d25957e3ul, 0xb8f1aa1fd7b685cdul },
  { 0x3ff5342b569d4f82ul, 0xbc807abe1db13cadul, 0x390fa733951f214cul },
  { 0x3ff56f4736b527daul, 0x3c99bb2c011d93adul, 0xb90ff86852a613fful },
  { 0x3ff5ab07dd485429ul, 0x3c96324c054647adul, 0xb92744ee506fdafeul },
  { 0x3ff5e76f15ad2148ul, 0x3c9ba6f93080e65eul, 0xb9395f9ab75fa7d6ul },
  { 0x3ff6247eb03a5585ul, 0xbc9383c17e40b497ul, 0x3905d8e757cfb991ul },
  { 0x3ff6623882552225ul, 0xbc9bb60987591c34ul, 0x3934a337f4dc0a3bul },
  { 0x3ff6a09e667f3bcdul, 0xbc9bdd3413b26456ul, 0x39357d3e3adec175ul },
  { 0x3ff6dfb23c651a2ful, 0xbc6bbe3a683c88abul, 0x38ca59f88abbe778ul },
  { 0x3ff71f75e8ec5f74ul, 0xbc816e4786887a99ul, 0xb92269796953a4c3ul },
  { 0x3ff75feb564267c9ul, 0xbc90245957316dd3ul, 0xb938f8e7fa19e5e8ul },
  { 0x3ff7a11473eb0187ul, 0xbc841577ee04992ful, 0xb8e4217a932d10d4ul },
  { 0x3ff7e2f336cf4e62ul, 0x3c705d02ba15797eul, 0x38f70a1427f8fcdful },
  { 0x3ff82589994cce13ul, 0xbc9d4c1dd41532d8ul, 0x38f0f6ad65cbbac1ul },
  { 0x3ff868d99b4492edul, 0xbc9fc6f89bd4f6baul, 0xb92f16f65181d921ul },
  { 0x3ff8ace5422aa0dbul, 0x3c96e9f156864b27ul, 0xb9130644a7836333ul },
  { 0x3ff8f1ae99157736ul, 0x3c85cc13a2e3976cul, 0x38d3bf26d2b85163ul },
  { 0x3ff93737b0cdc5e5ul, 0xbc675fc781b57ebcul, 0x390697e257ac0db2ul },
  { 0x3ff97d829fde4e50ul, 0xbc9d185b7c1b85d1ul, 0x3937edb9d7144b6ful },
  { 0x3ff9c49182a3f090ul, 0x3c7c7c46b071f2beul, 0x3916376b7943085cul },
  { 0x3ffa0c667b5de565ul, 0xbc9359495d1cd533ul, 0x392354084551b4fbul },
  { 0x3ffa5503b23e255dul, 0xbc9d2f6edb8d41e1ul, 0xb90bfd7adfd63f48ul },
  { 0x3ffa9e6b5579fdbful, 0x3c90fac90ef7fd31ul, 0x3928b16ae39e8cb9ul },
  { 0x3ffae89f995ad3adul, 0x3c97a1cd345dcc81ul, 0x393a7fbc3ae675eaul },
  { 0x3ffb33a2b84f15fbul, 0xbc62805e3084d708ul, 0x3902babc0edda4d9ul },
  { 0x3ffb7f76f2fb5e47ul, 0xbc75584f7e54ac3bul, 0x390aa64481e1ab72ul },
  { 0x3ffbcc1e904bc1d2ul, 0x3c823dd07a2d9e84ul, 0x3929a164050e1258ul },
  { 0x3ffc199bdd85529cul, 0x3c811065895048ddul, 0x39199e51125928daul },
  { 0x3ffc67f12e57d14bul, 0x3c92884dff483cadul, 0xb92fc44c329d5cb2ul },
  { 0x3ffcb720dcef9069ul, 0x3c7503cbd1e949dbul, 0x391d8765566b032eul },
  { 0x3ffd072d4a07897cul, 0xbc9cbc3743797a9cul, 0xb93e7044039da0f6ul },
  { 0x3ffd5818dcfba487ul, 0x3c82ed02d75b3707ul, 0xb90ab053b05531fcul },
  { 0x3ffda9e603db3285ul, 0x3c9c2300696db532ul, 0x3937f6246f0ec615ul },
  { 0x3ffdfc97337b9b5ful, 0xbc91a5cd4f184b5cul, 0x393b7225a944efd6ul },
  { 0x3ffe502ee78b3ff6ul, 0x3c839e8980a9cc8ful, 0x3921e92cb3c2d278ul },
  { 0x3ffea4afa2a490daul, 0xbc9e9c23179c2893ul, 0xb92fc0f242bbf3deul },
  { 0x3ffefa1bee615a27ul, 0x3c9dc7f486a4b6b0ul, 0x393f6dd5d229ff69ul },
  { 0x3fff50765b6e4540ul, 0x3c99d3e12dd8a18bul, 0xb914019bffc80ef3ul },
  { 0x3fffa7c1819e90d8ul, 0x3c874853f3a5931eul, 0x38fdc060c36f7651ul },
};
// EXP_TD_CONSTANTS_END
GPGA_CONST gpga_double exp_Log2h = 0x3f262e42ff000000ul;
GPGA_CONST gpga_double exp_Log2l = 0xbd0718432a1b0e26ul;

inline gpga_double gpga_exp_adjust_exponent(gpga_double value, int M) {
  uint hi = gpga_u64_hi(value);
  hi += ((uint)M) << 20;
  return gpga_u64_from_words(hi, gpga_u64_lo(value));
}

inline gpga_double gpga_exp_make_pow2(int exp_bits) {
  return gpga_u64_from_words(((uint)exp_bits) << 20, 0u);
}

inline void gpga_exp_td_accurate(thread gpga_double* polyTblh,
                                 thread gpga_double* polyTblm,
                                 thread gpga_double* polyTbll, gpga_double rh,
                                 gpga_double rm, gpga_double rl,
                                 gpga_double tbl1h, gpga_double tbl1m,
                                 gpga_double tbl1l, gpga_double tbl2h,
                                 gpga_double tbl2m, gpga_double tbl2l) {
  gpga_double highPoly = gpga_double_add(
      exp_accPolyC5,
      gpga_double_mul(rh, gpga_double_add(exp_accPolyC6,
                                          gpga_double_mul(rh, exp_accPolyC7))));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  Mul12(&t1h, &t1l, rh, highPoly);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  Add22(&t2h, &t2l, exp_accPolyC4h, exp_accPolyC4l, t1h, t1l);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  Mul22(&t3h, &t3l, rh, gpga_double_zero(0u), t2h, t2l);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  Add22(&t4h, &t4l, exp_accPolyC3h, exp_accPolyC3l, t3h, t3l);

  gpga_double rhSquareh = gpga_double_zero(0u);
  gpga_double rhSquarel = gpga_double_zero(0u);
  Mul12(&rhSquareh, &rhSquarel, rh, rh);
  gpga_double rhCubeh = gpga_double_zero(0u);
  gpga_double rhCubem = gpga_double_zero(0u);
  gpga_double rhCubel = gpga_double_zero(0u);
  Mul23(&rhCubeh, &rhCubem, &rhCubel, rh, gpga_double_zero(0u), rhSquareh,
        rhSquarel);

  gpga_double rhSquareHalfh =
      gpga_double_mul(rhSquareh, gpga_double_const_inv2());
  gpga_double rhSquareHalfl =
      gpga_double_mul(rhSquarel, gpga_double_const_inv2());

  gpga_double lowPolyh = gpga_double_zero(0u);
  gpga_double lowPolym = gpga_double_zero(0u);
  gpga_double lowPolyl = gpga_double_zero(0u);
  Renormalize3(&lowPolyh, &lowPolym, &lowPolyl, rh, rhSquareHalfh,
               rhSquareHalfl);

  gpga_double highPolyMulth = gpga_double_zero(0u);
  gpga_double highPolyMultm = gpga_double_zero(0u);
  gpga_double highPolyMultl = gpga_double_zero(0u);
  Mul233(&highPolyMulth, &highPolyMultm, &highPolyMultl, t4h, t4l, rhCubeh,
         rhCubem, rhCubel);

  gpga_double ph = gpga_double_zero(0u);
  gpga_double pm = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  Add33(&ph, &pm, &pl, lowPolyh, lowPolym, lowPolyl, highPolyMulth,
        highPolyMultm, highPolyMultl);

  gpga_double phnorm = gpga_double_zero(0u);
  gpga_double pmnorm = gpga_double_zero(0u);
  Add12(&phnorm, &pmnorm, ph, pm);

  gpga_double rmlMultPh = gpga_double_zero(0u);
  gpga_double rmlMultPl = gpga_double_zero(0u);
  Mul22(&rmlMultPh, &rmlMultPl, rm, rl, phnorm, pmnorm);
  gpga_double qh = gpga_double_zero(0u);
  gpga_double ql = gpga_double_zero(0u);
  Add22(&qh, &ql, rm, rl, rmlMultPh, rmlMultPl);

  gpga_double fullPolyh = gpga_double_zero(0u);
  gpga_double fullPolym = gpga_double_zero(0u);
  gpga_double fullPolyl = gpga_double_zero(0u);
  Add233Cond(&fullPolyh, &fullPolym, &fullPolyl, qh, ql, ph, pm, pl);

  gpga_double polyAddOneh = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  Add12(&polyAddOneh, &t5, gpga_double_from_u32(1u), fullPolyh);
  gpga_double polyAddOnem = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  Add12Cond(&polyAddOnem, &t6, t5, fullPolym);
  gpga_double polyAddOnel = gpga_double_add(t6, fullPolyl);

  gpga_double polyWithTbl1h = gpga_double_zero(0u);
  gpga_double polyWithTbl1m = gpga_double_zero(0u);
  gpga_double polyWithTbl1l = gpga_double_zero(0u);
  Mul33(&polyWithTbl1h, &polyWithTbl1m, &polyWithTbl1l, tbl1h, tbl1m, tbl1l,
        polyAddOneh, polyAddOnem, polyAddOnel);
  gpga_double polyWithTablesh = gpga_double_zero(0u);
  gpga_double polyWithTablesm = gpga_double_zero(0u);
  gpga_double polyWithTablesl = gpga_double_zero(0u);
  Mul33(&polyWithTablesh, &polyWithTablesm, &polyWithTablesl, tbl2h, tbl2m,
        tbl2l, polyWithTbl1h, polyWithTbl1m, polyWithTbl1l);

  Renormalize3(polyTblh, polyTblm, polyTbll, polyWithTablesh, polyWithTablesm,
               polyWithTablesl);
}

inline gpga_double gpga_exp_rn(gpga_double x) {
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  gpga_double polyTbll = gpga_double_zero(0u);
  int k = 0;
  int M = 0;
  int index1 = 0;
  int index2 = 0;
  int mightBeDenorm = 0;

  gpga_double xMultLog2InvMult2L = gpga_double_mul(x, exp_log2InvMult2L);
  gpga_double shiftedXMult = gpga_double_add(xMultLog2InvMult2L, exp_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, exp_shiftConst);

  uint xIntHi = gpga_u64_hi(x);
  uint xIntLo = gpga_u64_lo(x);
  if ((xIntHi & 0x7ff00000u) == 0u) {
    return gpga_double_from_u32(1u);
  }

  if ((xIntHi & 0x7fffffffU) >= exp_OVRUDRFLWSMPLBOUND) {
    if ((xIntHi & 0x7fffffffU) >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xIntLo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xIntHi & 0x80000000u) == 0u) {
        return x;
      }
      return gpga_double_zero(0u);
    }
    if (gpga_double_gt(x, exp_OVRFLWBOUND)) {
      return gpga_double_mul(exp_LARGEST, exp_LARGEST);
    }
    if (gpga_double_le(x, exp_UNDERFLWBOUND)) {
      return gpga_double_mul(exp_SMALLEST, exp_SMALLEST);
    }
    if (gpga_double_le(x, exp_DENORMBOUND)) {
      mightBeDenorm = 1;
    }
  }

  gpga_double t_log2h = gpga_double_mul(kd, exp_Log2h);
  gpga_double t_log2l = gpga_double_mul(kd, exp_Log2l);
  Add12Cond(&rh, &rm, gpga_double_sub(x, t_log2h), gpga_double_neg(t_log2l));

  k = (int)gpga_u64_lo(shiftedXMult);
  M = k >> exp_L;
  index1 = k & (int)exp_INDEXMASK1;
  index2 = (k & (int)exp_INDEXMASK2) >> exp_LHALF;

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  if (mightBeDenorm == 1) {
    gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
    Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
           exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
    gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
    gpga_double t2 = gpga_double_zero(0u);
    Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
    Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

    tbl1l = exp_twoPowerIndex1[index1].lo;
    tbl2l = exp_twoPowerIndex2[index2].lo;
    gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h,
                         tbl1m, tbl1l, tbl2h, tbl2m, tbl2l);

    gpga_double t3 = gpga_double_mul(polyTblh, exp_twoPowerM1000);
    gpga_double twoPowerM = gpga_exp_make_pow2(M + 2023);
    gpga_double t4 = gpga_double_mul(t3, twoPowerM);
    gpga_double t4_store = t4;

    M *= -1;
    twoPowerM = gpga_exp_make_pow2(M + 23);
    gpga_double t5 = gpga_double_mul(t4, twoPowerM);
    gpga_double t6 = gpga_double_mul(t5, exp_twoPower1000);
    gpga_double t7 = gpga_double_sub(polyTblh, t6);

    gpga_double round_check = gpga_exp_make_pow2(M - 52);
    if (!gpga_double_eq(gpga_double_abs(t7), round_check)) {
      return t4;
    }

    polyTblm = gpga_double_add(polyTblm, polyTbll);

    ulong t4_bits = t4_store;
    if (gpga_double_gt(t7, gpga_double_zero(0u))) {
      if (gpga_double_gt(polyTblm, gpga_double_zero(0u))) {
        t4_bits += 1ull;
        return t4_bits;
      }
      return t4;
    }
    if (gpga_double_lt(polyTblm, gpga_double_zero(0u))) {
      t4_bits -= 1ull;
      return t4_bits;
    }
    return t4;
  }

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(exp_c3, rh);
  gpga_double rhSquareHalf =
      gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(exp_c4, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  Add12(&polyTblh, &polyTblm, t11, t13);

  gpga_double test =
      gpga_double_add(polyTblh, gpga_double_mul(polyTblm, exp_ROUNDCST));
  if (gpga_double_eq(polyTblh, test)) {
    return gpga_exp_adjust_exponent(polyTblh, M);
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
  gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l);

  gpga_double res = gpga_double_zero(0u);
  RoundToNearest3(&res, polyTblh, polyTblm, polyTbll);
  return gpga_exp_adjust_exponent(res, M);
}

inline gpga_double gpga_exp_ru(gpga_double x) {
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  gpga_double polyTbll = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int k = 0;
  int M = 0;
  int index1 = 0;
  int index2 = 0;
  int mightBeDenorm = 0;

  gpga_double xMultLog2InvMult2L = gpga_double_mul(x, exp_log2InvMult2L);
  gpga_double shiftedXMult = gpga_double_add(xMultLog2InvMult2L, exp_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, exp_shiftConst);

  uint xIntHi = gpga_u64_hi(x);
  uint xIntLo = gpga_u64_lo(x);
  if ((xIntHi & 0x7ff00000u) == 0u) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    if (gpga_double_sign(x) != 0u) {
      return gpga_double_add(gpga_double_from_u32(1u), exp_SMALLEST);
    }
    return gpga_double_add(gpga_double_from_u32(1u), exp_twoM52);
  }

  if ((xIntHi & 0x7fffffffU) >= exp_OVRUDRFLWSMPLBOUND) {
    if ((xIntHi & 0x7fffffffU) >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xIntLo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xIntHi & 0x80000000u) == 0u) {
        return x;
      }
      return gpga_double_zero(0u);
    }
    if (gpga_double_gt(x, exp_OVRFLWBOUND)) {
      return gpga_double_mul(exp_LARGEST, exp_LARGEST);
    }
    if (gpga_double_le(x, exp_UNDERFLWBOUND)) {
      return exp_SMALLEST;
    }
    if (gpga_double_le(x, exp_DENORMBOUND)) {
      mightBeDenorm = 1;
    }
  }

  gpga_double t_log2h = gpga_double_mul(kd, exp_Log2h);
  gpga_double t_log2l = gpga_double_mul(kd, exp_Log2l);
  Add12Cond(&rh, &rm, gpga_double_sub(x, t_log2h), gpga_double_neg(t_log2l));

  k = (int)gpga_u64_lo(shiftedXMult);
  M = k >> exp_L;
  index1 = k & (int)exp_INDEXMASK1;
  index2 = (k & (int)exp_INDEXMASK2) >> exp_LHALF;

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  if (mightBeDenorm == 1) {
    gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
    Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
           exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
    gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
    gpga_double t2 = gpga_double_zero(0u);
    Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
    Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

    tbl1l = exp_twoPowerIndex1[index1].lo;
    tbl2l = exp_twoPowerIndex2[index2].lo;
    gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h,
                         tbl1m, tbl1l, tbl2h, tbl2m, tbl2l);

    gpga_double t3 = gpga_double_mul(polyTblh, exp_twoPowerM1000);
    gpga_double twoPowerM = gpga_exp_make_pow2(M + 2023);
    gpga_double t4 = gpga_double_mul(t3, twoPowerM);
    gpga_double t4_store = t4;

    M *= -1;
    twoPowerM = gpga_exp_make_pow2(M + 23);
    gpga_double t5 = gpga_double_mul(t4, twoPowerM);
    gpga_double t6 = gpga_double_mul(t5, exp_twoPower1000);
    gpga_double t7 = gpga_double_sub(polyTblh, t6);

    polyTblm = gpga_double_add(polyTblm, polyTbll);
    gpga_double t8 = gpga_double_add(t7, polyTblm);
    if (gpga_double_lt(t8, gpga_double_zero(0u))) {
      return t4;
    }

    ulong t4_bits = t4_store;
    t4_bits += 1ull;
    return t4_bits;
  }

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(exp_c3, rh);
  gpga_double rhSquareHalf =
      gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(exp_c4, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  Add12(&polyTblh, &polyTblm, t11, t13);

  bool roundable = gpga_test_and_return_ru(polyTblh, polyTblm, exp_RDROUNDCST,
                                           &res);
  if (roundable) {
    return gpga_exp_adjust_exponent(res, M);
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
  gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l);

  RoundUpwards3(&res, polyTblh, polyTblm, polyTbll);
  return gpga_exp_adjust_exponent(res, M);
}

inline gpga_double gpga_exp_rd(gpga_double x) {
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  gpga_double polyTbll = gpga_double_zero(0u);
  gpga_double res = gpga_double_zero(0u);
  int k = 0;
  int M = 0;
  int index1 = 0;
  int index2 = 0;
  int mightBeDenorm = 0;

  gpga_double xMultLog2InvMult2L = gpga_double_mul(x, exp_log2InvMult2L);
  gpga_double shiftedXMult = gpga_double_add(xMultLog2InvMult2L, exp_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, exp_shiftConst);

  uint xIntHi = gpga_u64_hi(x);
  uint xIntLo = gpga_u64_lo(x);
  if ((xIntHi & 0x7ff00000u) == 0u) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    if (gpga_double_sign(x) == 0u) {
      return gpga_double_add(gpga_double_from_u32(1u), exp_SMALLEST);
    }
    return gpga_double_add(gpga_double_from_u32(1u), exp_mTwoM53);
  }

  if ((xIntHi & 0x7fffffffU) >= exp_OVRUDRFLWSMPLBOUND) {
    if ((xIntHi & 0x7fffffffU) >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xIntLo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xIntHi & 0x80000000u) == 0u) {
        return x;
      }
      return gpga_double_zero(0u);
    }
    if (gpga_double_gt(x, exp_OVRFLWBOUND)) {
      return gpga_double_mul(exp_LARGEST,
                             gpga_double_add(gpga_double_from_u32(1u),
                                             exp_SMALLEST));
    }
    if (gpga_double_le(x, exp_UNDERFLWBOUND)) {
      return gpga_double_mul(exp_SMALLEST, exp_SMALLEST);
    }
    if (gpga_double_le(x, exp_DENORMBOUND)) {
      mightBeDenorm = 1;
    }
  }

  gpga_double t_log2h = gpga_double_mul(kd, exp_Log2h);
  gpga_double t_log2l = gpga_double_mul(kd, exp_Log2l);
  Add12Cond(&rh, &rm, gpga_double_sub(x, t_log2h), gpga_double_neg(t_log2l));

  k = (int)gpga_u64_lo(shiftedXMult);
  M = k >> exp_L;
  index1 = k & (int)exp_INDEXMASK1;
  index2 = (k & (int)exp_INDEXMASK2) >> exp_LHALF;

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  if (mightBeDenorm == 1) {
    gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
    gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
    Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
           exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
    gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
    gpga_double t2 = gpga_double_zero(0u);
    Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
    Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

    tbl1l = exp_twoPowerIndex1[index1].lo;
    tbl2l = exp_twoPowerIndex2[index2].lo;
    gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h,
                         tbl1m, tbl1l, tbl2h, tbl2m, tbl2l);

    gpga_double t3 = gpga_double_mul(polyTblh, exp_twoPowerM1000);
    gpga_double twoPowerM = gpga_exp_make_pow2(M + 2023);
    gpga_double t4 = gpga_double_mul(t3, twoPowerM);
    gpga_double t4_store = t4;

    M *= -1;
    twoPowerM = gpga_exp_make_pow2(M + 23);
    gpga_double t5 = gpga_double_mul(t4, twoPowerM);
    gpga_double t6 = gpga_double_mul(t5, exp_twoPower1000);
    gpga_double t7 = gpga_double_sub(polyTblh, t6);

    polyTblm = gpga_double_add(polyTblm, polyTbll);
    gpga_double t8 = gpga_double_add(t7, polyTblm);
    if (gpga_double_gt(t8, gpga_double_zero(0u))) {
      return t4;
    }

    ulong t4_bits = t4_store;
    t4_bits -= 1ull;
    return t4_bits;
  }

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(exp_c3, rh);
  gpga_double rhSquareHalf =
      gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(exp_c4, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  Add12(&polyTblh, &polyTblm, t11, t13);

  bool roundable = gpga_test_and_return_rd(polyTblh, polyTblm, exp_RDROUNDCST,
                                           &res);
  if (roundable) {
    return gpga_exp_adjust_exponent(res, M);
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
  gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_exp_td_accurate(&polyTblh, &polyTblm, &polyTbll, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l);

  RoundDownwards3(&res, polyTblh, polyTblm, polyTbll);
  return gpga_exp_adjust_exponent(res, M);
}

inline gpga_double gpga_exp_rz(gpga_double x) {
  return gpga_exp_rd(x);
}

inline gpga_double gpga_exp2_rn(gpga_double x) {
  if (gpga_double_is_nan(x)) {
    return x;
  }
  if (gpga_double_is_inf(x)) {
    return gpga_double_sign(x) ? gpga_double_zero(0u) : x;
  }
  if (gpga_double_ge(x, gpga_double_from_s32(1024))) {
    return gpga_double_inf(0u);
  }
  if (gpga_double_lt(x, gpga_double_from_s32(-1075))) {
    return gpga_double_zero(0u);
  }
  int H = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_pow_exp2_120_core(&H, &resh, &resm, &resl, x, gpga_double_zero(0u),
                         gpga_double_zero(0u));
  gpga_double res = gpga_double_zero(0u);
  RoundToNearest3(&res, resh, resm, resl);
  return gpga_double_ldexp(res, H);
}

inline gpga_double gpga_exp2_rd(gpga_double x) {
  if (gpga_double_is_nan(x)) {
    return x;
  }
  if (gpga_double_is_inf(x)) {
    return gpga_double_sign(x) ? gpga_double_zero(0u) : x;
  }
  if (gpga_double_ge(x, gpga_double_from_s32(1024))) {
    return exp_LARGEST;
  }
  if (gpga_double_lt(x, gpga_double_from_s32(-1075))) {
    return gpga_double_zero(0u);
  }
  int H = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_pow_exp2_120_core(&H, &resh, &resm, &resl, x, gpga_double_zero(0u),
                         gpga_double_zero(0u));
  gpga_double res = gpga_double_zero(0u);
  RoundDownwards3(&res, resh, resm, resl);
  return gpga_double_ldexp(res, H);
}

inline gpga_double gpga_exp2_ru(gpga_double x) {
  if (gpga_double_is_nan(x)) {
    return x;
  }
  if (gpga_double_is_inf(x)) {
    return gpga_double_sign(x) ? gpga_double_zero(0u) : x;
  }
  if (gpga_double_ge(x, gpga_double_from_s32(1024))) {
    return gpga_double_inf(0u);
  }
  if (gpga_double_lt(x, gpga_double_from_s32(-1075))) {
    return gpga_bits_to_real(0x1ul);
  }
  int H = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_pow_exp2_120_core(&H, &resh, &resm, &resl, x, gpga_double_zero(0u),
                         gpga_double_zero(0u));
  gpga_double res = gpga_double_zero(0u);
  RoundUpwards3(&res, resh, resm, resl);
  return gpga_double_ldexp(res, H);
}

inline gpga_double gpga_exp2_rz(gpga_double x) {
  return gpga_exp2_rd(x);
}

// CRLIBM_EXPM1
// EXPM1_CONSTANTS_BEGIN
// CRLIBM_EXPM1_CONSTANTS
GPGA_CONST gpga_double expm1_log2InvMult2L = 0x40b71547652b82feul;
GPGA_CONST gpga_double expm1_msLog2Div2Lh = 0xbf262e42fefa39eful;
GPGA_CONST gpga_double expm1_msLog2Div2Lm = 0xbbbabc9e3b39803ful;
GPGA_CONST gpga_double expm1_msLog2Div2Ll = 0xb847b57a079a1934ul;
GPGA_CONST gpga_double expm1_shiftConst = 0x4338000000000000ul;
GPGA_CONST uint expm1_INDEXMASK1 = 0x0000003fu;
GPGA_CONST uint expm1_INDEXMASK2 = 0x00000fc0u;
GPGA_CONST uint expm1_RETURNXBOUND = 0x3c900000u;
GPGA_CONST gpga_double expm1_OVERFLOWBOUND = 0x40862e42fefa39eful;
GPGA_CONST gpga_double expm1_LARGEST = 0x7feffffffffffffful;
GPGA_CONST gpga_double expm1_SMALLEST = 0x0000000000000001ul;
GPGA_CONST gpga_double expm1_MINUSONEBOUND = 0xc042b708872320e2ul;
GPGA_CONST uint expm1_SIMPLEOVERFLOWBOUND = 0x40862e42u;
GPGA_CONST uint expm1_DIRECTINTERVALBOUND = 0x3fd00000u;
GPGA_CONST uint expm1_SPECIALINTERVALBOUND = 0x3f300000u;
GPGA_CONST gpga_double expm1_ROUNDCSTDIRECTRN = 0x3ff0101010101011ul;
GPGA_CONST gpga_double expm1_ROUNDCSTDIRECTRD = 0x3c10000000000000ul;
GPGA_CONST gpga_double expm1_ROUNDCSTCOMMONRN = 0x3ff0101010101011ul;
GPGA_CONST gpga_double expm1_ROUNDCSTCOMMONRD = 0x3c10000000000000ul;
GPGA_CONST gpga_double expm1_MINUSONEPLUSONEULP = 0xbfeffffffffffffful;
GPGA_CONST gpga_double expm1_quickDirectpolyC3h = 0x3fc5555555555555ul;
GPGA_CONST gpga_double expm1_quickDirectpolyC4h = 0x3fa5555555555559ul;
GPGA_CONST gpga_double expm1_quickDirectpolyC5h = 0x3f8111111111bbbful;
GPGA_CONST gpga_double expm1_quickDirectpolyC6h = 0x3f56c16c16b1d8aeul;
GPGA_CONST gpga_double expm1_quickDirectpolyC7h = 0x3f2a019ec49aa7cful;
GPGA_CONST gpga_double expm1_quickDirectpolyC8h = 0x3efa01c0084c2c02ul;
GPGA_CONST gpga_double expm1_quickDirectpolyC9h = 0x3ec7e10b9cfb79faul;
GPGA_CONST gpga_double expm1_accuDirectpolyC3h = 0x3fc5555555555555ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC3m = 0x3c65555555555555ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC3l = 0x390555563cabe8e8ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC4h = 0x3fa5555555555555ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC4m = 0x3c45555555555554ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC5h = 0x3f81111111111111ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC5m = 0x3c01111111111110ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC6h = 0x3f56c16c16c16c17ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC6m = 0xbbef49f49f35a818ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC7h = 0x3f2a01a01a01a01aul;
GPGA_CONST gpga_double expm1_accuDirectpolyC7m = 0x3b6a01a01b129d80ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC8h = 0x3efa01a01a01a01aul;
GPGA_CONST gpga_double expm1_accuDirectpolyC8m = 0x3b39f327e395e980ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC9h = 0x3ec71de3a556c734ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC9m = 0xbb6c1569ffc6b4fcul;
GPGA_CONST gpga_double expm1_accuDirectpolyC10h = 0x3e927e4fb7789f5ful;
GPGA_CONST gpga_double expm1_accuDirectpolyC11h = 0x3e5ae64567f544e6ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC12h = 0x3e21eed8eff141b7ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC13h = 0x3de6124613a115cdul;
GPGA_CONST gpga_double expm1_accuDirectpolyC14h = 0x3da9398b61dd2859ul;
GPGA_CONST gpga_double expm1_accuDirectpolyC15h = 0x3d6ae809b431ab32ul;
GPGA_CONST gpga_double expm1_quickCommonpolyC3h = 0x3fc555555565bba1ul;
GPGA_CONST gpga_double expm1_quickCommonpolyC4h = 0x3fa55555556b330ful;
GPGA_CONST gpga_double expm1_accuCommonpolyC3h = 0x3fc5555555555555ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC3m = 0x3c655555555689e2ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC4h = 0x3fa5555555555555ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC4m = 0x3c45546533d76562ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC5h = 0x3f81111111111111ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC6h = 0x3f56c16c16d10a77ul;
GPGA_CONST gpga_double expm1_accuCommonpolyC7h = 0x3f2a01a03b7b3d5aul;
// EXPM1_CONSTANTS_END

inline void gpga_expm1_direct_td(thread gpga_double* expm1h,
                                 thread gpga_double* expm1m,
                                 thread gpga_double* expm1l, gpga_double x,
                                 gpga_double xSqHalfh, gpga_double xSqHalfl,
                                 gpga_double xSqh, gpga_double xSql,
                                 int expoX) {
  gpga_double highPoly = gpga_double_add(
      expm1_accuDirectpolyC11h,
      gpga_double_mul(
          x, gpga_double_add(
                 expm1_accuDirectpolyC12h,
                 gpga_double_mul(
                     x, gpga_double_add(
                            expm1_accuDirectpolyC13h,
                            gpga_double_mul(
                                x, gpga_double_add(expm1_accuDirectpolyC14h,
                                                   gpga_double_mul(
                                                       x,
                                                       expm1_accuDirectpolyC15h))))))));
  gpga_double tt1h = gpga_double_mul(x, highPoly);

  gpga_double lowPolyh = gpga_double_zero(0u);
  gpga_double lowPolym = gpga_double_zero(0u);
  gpga_double lowPolyl = gpga_double_zero(0u);
  Add123(&lowPolyh, &lowPolym, &lowPolyl, x, xSqHalfh, xSqHalfl);

  gpga_double xCubeh = gpga_double_zero(0u);
  gpga_double xCubem = gpga_double_zero(0u);
  gpga_double xCubel = gpga_double_zero(0u);
  Mul123(&xCubeh, &xCubem, &xCubel, x, xSqh, xSql);

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  Add12(&t1h, &t1l, expm1_accuDirectpolyC10h, tt1h);

  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  MulAdd212(&t2h, &t2l, expm1_accuDirectpolyC9h, expm1_accuDirectpolyC9m, x,
            t1h, t1l);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  MulAdd212(&t3h, &t3l, expm1_accuDirectpolyC8h, expm1_accuDirectpolyC8m, x,
            t2h, t2l);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  MulAdd212(&t4h, &t4l, expm1_accuDirectpolyC7h, expm1_accuDirectpolyC7m, x,
            t3h, t3l);
  gpga_double t5h = gpga_double_zero(0u);
  gpga_double t5l = gpga_double_zero(0u);
  MulAdd212(&t5h, &t5l, expm1_accuDirectpolyC6h, expm1_accuDirectpolyC6m, x,
            t4h, t4l);
  gpga_double t6h = gpga_double_zero(0u);
  gpga_double t6l = gpga_double_zero(0u);
  MulAdd212(&t6h, &t6l, expm1_accuDirectpolyC5h, expm1_accuDirectpolyC5m, x,
            t5h, t5l);

  gpga_double tt6h = gpga_double_zero(0u);
  gpga_double tt6m = gpga_double_zero(0u);
  gpga_double tt6l = gpga_double_zero(0u);
  Mul123(&tt6h, &tt6m, &tt6l, x, t6h, t6l);

  gpga_double t7h = gpga_double_zero(0u);
  gpga_double t7m = gpga_double_zero(0u);
  gpga_double t7l = gpga_double_zero(0u);
  Add233(&t7h, &t7m, &t7l, expm1_accuDirectpolyC4h,
         expm1_accuDirectpolyC4m, tt6h, tt6m, tt6l);

  gpga_double tt7h = gpga_double_zero(0u);
  gpga_double tt7m = gpga_double_zero(0u);
  gpga_double tt7l = gpga_double_zero(0u);
  Mul133(&tt7h, &tt7m, &tt7l, x, t7h, t7m, t7l);

  gpga_double t8h = gpga_double_zero(0u);
  gpga_double t8m = gpga_double_zero(0u);
  gpga_double t8l = gpga_double_zero(0u);
  Add33(&t8h, &t8m, &t8l, expm1_accuDirectpolyC3h,
        expm1_accuDirectpolyC3m, expm1_accuDirectpolyC3l, tt7h, tt7m, tt7l);

  gpga_double fullHighPolyhover = gpga_double_zero(0u);
  gpga_double fullHighPolymover = gpga_double_zero(0u);
  gpga_double fullHighPolylover = gpga_double_zero(0u);
  Mul33(&fullHighPolyhover, &fullHighPolymover, &fullHighPolylover, xCubeh,
        xCubem, xCubel, t8h, t8m, t8l);

  gpga_double fullHighPolyh = gpga_double_zero(0u);
  gpga_double fullHighPolym = gpga_double_zero(0u);
  gpga_double fullHighPolyl = gpga_double_zero(0u);
  Renormalize3(&fullHighPolyh, &fullHighPolym, &fullHighPolyl,
               fullHighPolyhover, fullHighPolymover, fullHighPolylover);

  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polym = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  Add33(&polyh, &polym, &polyl, lowPolyh, lowPolym, lowPolyl, fullHighPolyh,
        fullHighPolym, fullHighPolyl);

  gpga_double expm1hover = gpga_double_zero(0u);
  gpga_double expm1mover = gpga_double_zero(0u);
  gpga_double expm1lover = gpga_double_zero(0u);

  if (expoX >= 0) {
    gpga_double r1h = gpga_double_zero(0u);
    gpga_double r1m = gpga_double_zero(0u);
    gpga_double r1l = gpga_double_zero(0u);
    Add133(&r1h, &r1m, &r1l, gpga_double_from_u32(2u), polyh, polym, polyl);
    gpga_double rr1h = gpga_double_zero(0u);
    gpga_double rr1m = gpga_double_zero(0u);
    gpga_double rr1l = gpga_double_zero(0u);
    Mul33(&rr1h, &rr1m, &rr1l, r1h, r1m, r1l, polyh, polym, polyl);
    if (expoX >= 1) {
      gpga_double r2h = gpga_double_zero(0u);
      gpga_double r2m = gpga_double_zero(0u);
      gpga_double r2l = gpga_double_zero(0u);
      Add133(&r2h, &r2m, &r2l, gpga_double_from_u32(2u), rr1h, rr1m, rr1l);
      gpga_double rr2h = gpga_double_zero(0u);
      gpga_double rr2m = gpga_double_zero(0u);
      gpga_double rr2l = gpga_double_zero(0u);
      Mul33(&rr2h, &rr2m, &rr2l, r2h, r2m, r2l, rr1h, rr1m, rr1l);
      if (expoX >= 2) {
        gpga_double r3h = gpga_double_zero(0u);
        gpga_double r3m = gpga_double_zero(0u);
        gpga_double r3l = gpga_double_zero(0u);
        Add133(&r3h, &r3m, &r3l, gpga_double_from_u32(2u), rr2h, rr2m, rr2l);
        gpga_double rr3h = gpga_double_zero(0u);
        gpga_double rr3m = gpga_double_zero(0u);
        gpga_double rr3l = gpga_double_zero(0u);
        Mul33(&rr3h, &rr3m, &rr3l, r3h, r3m, r3l, rr2h, rr2m, rr2l);
        expm1hover = rr3h;
        expm1mover = rr3m;
        expm1lover = rr3l;
      } else {
        expm1hover = rr2h;
        expm1mover = rr2m;
        expm1lover = rr2l;
      }
    } else {
      expm1hover = rr1h;
      expm1mover = rr1m;
      expm1lover = rr1l;
    }
  } else {
    expm1hover = polyh;
    expm1mover = polym;
    expm1lover = polyl;
  }

  Renormalize3(expm1h, expm1m, expm1l, expm1hover, expm1mover, expm1lover);
}

inline void gpga_expm1_common_td(thread gpga_double* expm1h,
                                 thread gpga_double* expm1m,
                                 thread gpga_double* expm1l, gpga_double rh,
                                 gpga_double rm, gpga_double rl,
                                 gpga_double tbl1h, gpga_double tbl1m,
                                 gpga_double tbl1l, gpga_double tbl2h,
                                 gpga_double tbl2m, gpga_double tbl2l,
                                 int M) {
  gpga_double highPoly = gpga_double_add(
      expm1_accuCommonpolyC5h,
      gpga_double_mul(
          rh, gpga_double_add(expm1_accuCommonpolyC6h,
                              gpga_double_mul(rh, expm1_accuCommonpolyC7h))));

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  Mul12(&t1h, &t1l, rh, highPoly);
  gpga_double t2h = gpga_double_zero(0u);
  gpga_double t2l = gpga_double_zero(0u);
  Add22(&t2h, &t2l, expm1_accuCommonpolyC4h, expm1_accuCommonpolyC4m, t1h,
        t1l);
  gpga_double t3h = gpga_double_zero(0u);
  gpga_double t3l = gpga_double_zero(0u);
  Mul122(&t3h, &t3l, rh, t2h, t2l);
  gpga_double t4h = gpga_double_zero(0u);
  gpga_double t4l = gpga_double_zero(0u);
  Add22(&t4h, &t4l, expm1_accuCommonpolyC3h, expm1_accuCommonpolyC3m, t3h,
        t3l);

  gpga_double rhSquareh = gpga_double_zero(0u);
  gpga_double rhSquarel = gpga_double_zero(0u);
  Mul12(&rhSquareh, &rhSquarel, rh, rh);
  gpga_double rhCubeh = gpga_double_zero(0u);
  gpga_double rhCubem = gpga_double_zero(0u);
  gpga_double rhCubel = gpga_double_zero(0u);
  Mul123(&rhCubeh, &rhCubem, &rhCubel, rh, rhSquareh, rhSquarel);

  gpga_double rhSquareHalfh =
      gpga_double_mul(rhSquareh, gpga_double_const_inv2());
  gpga_double rhSquareHalfl =
      gpga_double_mul(rhSquarel, gpga_double_const_inv2());

  gpga_double lowPolyh = gpga_double_zero(0u);
  gpga_double lowPolym = gpga_double_zero(0u);
  gpga_double lowPolyl = gpga_double_zero(0u);
  Renormalize3(&lowPolyh, &lowPolym, &lowPolyl, rh, rhSquareHalfh,
               rhSquareHalfl);

  gpga_double highPolyMulth = gpga_double_zero(0u);
  gpga_double highPolyMultm = gpga_double_zero(0u);
  gpga_double highPolyMultl = gpga_double_zero(0u);
  Mul233(&highPolyMulth, &highPolyMultm, &highPolyMultl, t4h, t4l, rhCubeh,
         rhCubem, rhCubel);

  gpga_double ph = gpga_double_zero(0u);
  gpga_double pm = gpga_double_zero(0u);
  gpga_double pl = gpga_double_zero(0u);
  Add33(&ph, &pm, &pl, lowPolyh, lowPolym, lowPolyl, highPolyMulth,
        highPolyMultm, highPolyMultl);

  gpga_double phnorm = gpga_double_zero(0u);
  gpga_double pmnorm = gpga_double_zero(0u);
  Add12(&phnorm, &pmnorm, ph, pm);

  gpga_double rmlMultPh = gpga_double_zero(0u);
  gpga_double rmlMultPl = gpga_double_zero(0u);
  Mul22(&rmlMultPh, &rmlMultPl, rm, rl, phnorm, pmnorm);
  gpga_double qh = gpga_double_zero(0u);
  gpga_double ql = gpga_double_zero(0u);
  Add22(&qh, &ql, rm, rl, rmlMultPh, rmlMultPl);

  gpga_double fullPolyh = gpga_double_zero(0u);
  gpga_double fullPolym = gpga_double_zero(0u);
  gpga_double fullPolyl = gpga_double_zero(0u);
  Add233Cond(&fullPolyh, &fullPolym, &fullPolyl, qh, ql, ph, pm, pl);

  gpga_double polyAddOneh = gpga_double_zero(0u);
  gpga_double t5 = gpga_double_zero(0u);
  Add12(&polyAddOneh, &t5, gpga_double_from_u32(1u), fullPolyh);
  gpga_double polyAddOnem = gpga_double_zero(0u);
  gpga_double t6 = gpga_double_zero(0u);
  Add12Cond(&polyAddOnem, &t6, t5, fullPolym);
  gpga_double polyAddOnel = gpga_double_add(t6, fullPolyl);

  gpga_double polyWithTbl1h = gpga_double_zero(0u);
  gpga_double polyWithTbl1m = gpga_double_zero(0u);
  gpga_double polyWithTbl1l = gpga_double_zero(0u);
  Mul33(&polyWithTbl1h, &polyWithTbl1m, &polyWithTbl1l, tbl1h, tbl1m, tbl1l,
        polyAddOneh, polyAddOnem, polyAddOnel);
  gpga_double polyWithTablesh = gpga_double_zero(0u);
  gpga_double polyWithTablesm = gpga_double_zero(0u);
  gpga_double polyWithTablesl = gpga_double_zero(0u);
  Mul33(&polyWithTablesh, &polyWithTablesm, &polyWithTablesl, tbl2h, tbl2m,
        tbl2l, polyWithTbl1h, polyWithTbl1m, polyWithTbl1l);

  gpga_double exph = polyWithTablesh;
  gpga_double expm = polyWithTablesm;
  gpga_double expl = polyWithTablesl;
  int delta = M * (1 << 20);
  if (!gpga_double_is_zero(exph)) {
    uint hi = gpga_u64_hi(exph);
    hi = (uint)((int)hi + delta);
    exph = gpga_u64_from_words(hi, gpga_u64_lo(exph));
  }
  if (!gpga_double_is_zero(expm)) {
    uint hi = gpga_u64_hi(expm);
    hi = (uint)((int)hi + delta);
    expm = gpga_u64_from_words(hi, gpga_u64_lo(expm));
  }
  if (!gpga_double_is_zero(expl)) {
    uint hi = gpga_u64_hi(expl);
    hi = (uint)((int)hi + delta);
    expl = gpga_u64_from_words(hi, gpga_u64_lo(expl));
  }

  gpga_double expm1hover = gpga_double_zero(0u);
  gpga_double expm1mover = gpga_double_zero(0u);
  gpga_double expm1lover = gpga_double_zero(0u);
  Add133Cond(&expm1hover, &expm1mover, &expm1lover, gpga_double_from_s32(-1),
             exph, expm, expl);

  Renormalize3(expm1h, expm1m, expm1l, expm1hover, expm1mover, expm1lover);
}

inline gpga_double gpga_expm1_rn(gpga_double x) {
  gpga_double xSqh = gpga_double_zero(0u);
  gpga_double xSql = gpga_double_zero(0u);
  gpga_double xSqHalfh = gpga_double_zero(0u);
  gpga_double xSqHalfl = gpga_double_zero(0u);
  gpga_double xCubeh = gpga_double_zero(0u);
  gpga_double xCubel = gpga_double_zero(0u);
  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  gpga_double expm1h = gpga_double_zero(0u);
  gpga_double expm1m = gpga_double_zero(0u);
  gpga_double expm1l = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);

  uint xhi = gpga_u64_hi(x);
  uint xIntHi = xhi & 0x7fffffffU;
  uint xlo = gpga_u64_lo(x);

  if (xIntHi < expm1_RETURNXBOUND) {
    return x;
  }

  if (xIntHi >= expm1_SIMPLEOVERFLOWBOUND) {
    if (xIntHi >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xlo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xhi & 0x80000000u) == 0u) {
        return gpga_double_add(x, x);
      }
      return gpga_double_from_s32(-1);
    }
    if (gpga_double_gt(x, expm1_OVERFLOWBOUND)) {
      return gpga_double_mul(expm1_LARGEST, expm1_LARGEST);
    }
  }

  if (gpga_double_lt(x, expm1_MINUSONEBOUND)) {
    return gpga_double_from_s32(-1);
  }

  if (xIntHi < expm1_DIRECTINTERVALBOUND) {
    int expoX = (int)((xIntHi & 0x7ff00000u) >> 20) - (1023 - 5);
    if (expoX >= 0) {
      int delta = -((expoX + 1) << 20);
      xhi = (uint)((int)xhi + delta);
      x = gpga_u64_from_words(xhi, xlo);
      xIntHi = xhi & 0x7fffffffU;
    }

    Mul12(&xSqh, &xSql, x, x);
    gpga_double middlePoly =
        gpga_double_add(expm1_quickDirectpolyC4h,
                        gpga_double_mul(x, expm1_quickDirectpolyC5h));
    gpga_double doublePoly = middlePoly;

    if (xIntHi > expm1_SPECIALINTERVALBOUND) {
      gpga_double highPoly = gpga_double_add(
          expm1_quickDirectpolyC6h,
          gpga_double_mul(
              x, gpga_double_add(
                     expm1_quickDirectpolyC7h,
                     gpga_double_mul(
                         x, gpga_double_add(expm1_quickDirectpolyC8h,
                                            gpga_double_mul(
                                                x, expm1_quickDirectpolyC9h))))));
      gpga_double highPolyWithSquare = gpga_double_mul(xSqh, highPoly);
      doublePoly = gpga_double_add(middlePoly, highPolyWithSquare);
    }

    gpga_double tt1h = gpga_double_mul(x, doublePoly);
    xSqHalfh = gpga_double_mul(xSqh, gpga_double_const_inv2());
    xSqHalfl = gpga_double_mul(xSql, gpga_double_const_inv2());
    gpga_double t2h = gpga_double_zero(0u);
    gpga_double templ = gpga_double_zero(0u);
    Add12(&t2h, &templ, x, xSqHalfh);
    gpga_double t2l = gpga_double_add(templ, xSqHalfl);

    gpga_double t1h = gpga_double_zero(0u);
    gpga_double t1l = gpga_double_zero(0u);
    Add12(&t1h, &t1l, expm1_quickDirectpolyC3h, tt1h);
    Mul122(&xCubeh, &xCubel, x, xSqh, xSql);
    gpga_double tt3h = gpga_double_zero(0u);
    gpga_double tt3l = gpga_double_zero(0u);
    Mul22(&tt3h, &tt3l, xCubeh, xCubel, t1h, t1l);
    Add22(&polyh, &polyl, t2h, t2l, tt3h, tt3l);

    if (expoX >= 0) {
      gpga_double r1h = gpga_double_zero(0u);
      gpga_double r1t = gpga_double_zero(0u);
      Add12(&r1h, &r1t, gpga_double_from_u32(2u), polyh);
      gpga_double r1l = gpga_double_add(r1t, polyl);
      gpga_double rr1h = gpga_double_zero(0u);
      gpga_double rr1l = gpga_double_zero(0u);
      Mul22(&rr1h, &rr1l, r1h, r1l, polyh, polyl);
      if (expoX >= 1) {
        gpga_double r2h = gpga_double_zero(0u);
        gpga_double r2t = gpga_double_zero(0u);
        Add12(&r2h, &r2t, gpga_double_from_u32(2u), rr1h);
        gpga_double r2l = gpga_double_add(r2t, rr1l);
        gpga_double rr2h = gpga_double_zero(0u);
        gpga_double rr2l = gpga_double_zero(0u);
        Mul22(&rr2h, &rr2l, r2h, r2l, rr1h, rr1l);
        if (expoX >= 2) {
          gpga_double r3h = gpga_double_zero(0u);
          gpga_double r3t = gpga_double_zero(0u);
          Add12(&r3h, &r3t, gpga_double_from_u32(2u), rr2h);
          gpga_double r3l = gpga_double_add(r3t, rr2l);
          gpga_double rr3h = gpga_double_zero(0u);
          gpga_double rr3l = gpga_double_zero(0u);
          Mul22(&rr3h, &rr3l, r3h, r3l, rr2h, rr2l);
          expm1h = rr3h;
          expm1m = rr3l;
        } else {
          expm1h = rr2h;
          expm1m = rr2l;
        }
      } else {
        expm1h = rr1h;
        expm1m = rr1l;
      }
    } else {
      expm1h = polyh;
      expm1m = polyl;
    }

    gpga_double test =
        gpga_double_add(expm1h, gpga_double_mul(expm1m, expm1_ROUNDCSTDIRECTRN));
    if (gpga_double_eq(expm1h, test)) {
      return expm1h;
    }
    gpga_expm1_direct_td(&expm1h, &expm1m, &expm1l, x, xSqHalfh, xSqHalfl, xSqh,
                         xSql, expoX);
    return ReturnRoundToNearest3(expm1h, expm1m, expm1l);
  }

  gpga_double xMultLog2InvMult2L =
      gpga_double_mul(x, expm1_log2InvMult2L);
  gpga_double shiftedXMult =
      gpga_double_add(xMultLog2InvMult2L, expm1_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, expm1_shiftConst);
  int k = (int)gpga_u64_lo(shiftedXMult);
  int M = k >> 12;
  int index1 = k & (int)expm1_INDEXMASK1;
  int index2 = (k & (int)expm1_INDEXMASK2) >> 6;

  gpga_double s1 = gpga_double_zero(0u);
  gpga_double s2 = gpga_double_zero(0u);
  Mul12(&s1, &s2, expm1_msLog2Div2Lh, kd);
  gpga_double s3 = gpga_double_mul(kd, expm1_msLog2Div2Lm);
  gpga_double s4 = gpga_double_add(s2, s3);
  gpga_double s5 = gpga_double_add(x, s1);
  Add12Cond(&rh, &rm, s5, s4);

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(expm1_quickCommonpolyC3h, rh);
  gpga_double rhSquareHalf = gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(expm1_quickCommonpolyC4h, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  Add12(&polyTblh, &polyTblm, t11, t13);

  polyTblh = gpga_exp_adjust_exponent(polyTblh, M);
  if (!gpga_double_is_zero(polyTblm)) {
    polyTblm = gpga_exp_adjust_exponent(polyTblm, M);
  }

  gpga_double exph = polyTblh;
  gpga_double expm = polyTblm;
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&t1, &t2, gpga_double_from_s32(-1), exph);
  gpga_double t3 = gpga_double_add(t2, expm);
  Add12Cond(&expm1h, &expm1m, t1, t3);

  gpga_double test =
      gpga_double_add(expm1h, gpga_double_mul(expm1m, expm1_ROUNDCSTCOMMONRN));
  if (gpga_double_eq(expm1h, test)) {
    return expm1h;
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         expm1_msLog2Div2Lh, expm1_msLog2Div2Lm, expm1_msLog2Div2Ll);
  gpga_double t4 = gpga_double_add(x, msLog2Div2LMultKh);
  Add12Cond(&rh, &t2, t4, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_expm1_common_td(&expm1h, &expm1m, &expm1l, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l, M);
  return ReturnRoundToNearest3(expm1h, expm1m, expm1l);
}

inline gpga_double gpga_expm1_rd(gpga_double x) {
  gpga_double xSqh = gpga_double_zero(0u);
  gpga_double xSql = gpga_double_zero(0u);
  gpga_double xSqHalfh = gpga_double_zero(0u);
  gpga_double xSqHalfl = gpga_double_zero(0u);
  gpga_double xCubeh = gpga_double_zero(0u);
  gpga_double xCubel = gpga_double_zero(0u);
  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  gpga_double expm1h = gpga_double_zero(0u);
  gpga_double expm1m = gpga_double_zero(0u);
  gpga_double expm1l = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);

  uint xhi = gpga_u64_hi(x);
  uint xIntHi = xhi & 0x7fffffffU;
  uint xlo = gpga_u64_lo(x);

  if (xIntHi < expm1_RETURNXBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    return gpga_double_next_down(x);
  }

  if (xIntHi >= expm1_SIMPLEOVERFLOWBOUND) {
    if (xIntHi >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xlo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xhi & 0x80000000u) == 0u) {
        return gpga_double_add(x, x);
      }
      return gpga_double_from_s32(-1);
    }
    if (gpga_double_gt(x, expm1_OVERFLOWBOUND)) {
      gpga_double one = gpga_double_from_u32(1u);
      return gpga_double_mul(expm1_LARGEST,
                             gpga_double_add(one, expm1_SMALLEST));
    }
  }

  if (gpga_double_lt(x, expm1_MINUSONEBOUND)) {
    return gpga_double_from_s32(-1);
  }

  if (xIntHi < expm1_DIRECTINTERVALBOUND) {
    int expoX = (int)((xIntHi & 0x7ff00000u) >> 20) - (1023 - 5);
    if (expoX >= 0) {
      int delta = -((expoX + 1) << 20);
      xhi = (uint)((int)xhi + delta);
      x = gpga_u64_from_words(xhi, xlo);
      xIntHi = xhi & 0x7fffffffU;
    }

    Mul12(&xSqh, &xSql, x, x);
    gpga_double middlePoly =
        gpga_double_add(expm1_quickDirectpolyC4h,
                        gpga_double_mul(x, expm1_quickDirectpolyC5h));
    gpga_double doublePoly = middlePoly;

    if (xIntHi > expm1_SPECIALINTERVALBOUND) {
      gpga_double highPoly = gpga_double_add(
          expm1_quickDirectpolyC6h,
          gpga_double_mul(
              x, gpga_double_add(
                     expm1_quickDirectpolyC7h,
                     gpga_double_mul(
                         x, gpga_double_add(expm1_quickDirectpolyC8h,
                                            gpga_double_mul(
                                                x, expm1_quickDirectpolyC9h))))));
      gpga_double highPolyWithSquare = gpga_double_mul(xSqh, highPoly);
      doublePoly = gpga_double_add(middlePoly, highPolyWithSquare);
    }

    gpga_double tt1h = gpga_double_mul(x, doublePoly);
    xSqHalfh = gpga_double_mul(xSqh, gpga_double_const_inv2());
    xSqHalfl = gpga_double_mul(xSql, gpga_double_const_inv2());
    gpga_double t2h = gpga_double_zero(0u);
    gpga_double templ = gpga_double_zero(0u);
    Add12(&t2h, &templ, x, xSqHalfh);
    gpga_double t2l = gpga_double_add(templ, xSqHalfl);

    gpga_double t1h = gpga_double_zero(0u);
    gpga_double t1l = gpga_double_zero(0u);
    Add12(&t1h, &t1l, expm1_quickDirectpolyC3h, tt1h);
    Mul122(&xCubeh, &xCubel, x, xSqh, xSql);
    gpga_double tt3h = gpga_double_zero(0u);
    gpga_double tt3l = gpga_double_zero(0u);
    Mul22(&tt3h, &tt3l, xCubeh, xCubel, t1h, t1l);
    Add22(&polyh, &polyl, t2h, t2l, tt3h, tt3l);

    if (expoX >= 0) {
      gpga_double r1h = gpga_double_zero(0u);
      gpga_double r1t = gpga_double_zero(0u);
      Add12(&r1h, &r1t, gpga_double_from_u32(2u), polyh);
      gpga_double r1l = gpga_double_add(r1t, polyl);
      gpga_double rr1h = gpga_double_zero(0u);
      gpga_double rr1l = gpga_double_zero(0u);
      Mul22(&rr1h, &rr1l, r1h, r1l, polyh, polyl);
      if (expoX >= 1) {
        gpga_double r2h = gpga_double_zero(0u);
        gpga_double r2t = gpga_double_zero(0u);
        Add12(&r2h, &r2t, gpga_double_from_u32(2u), rr1h);
        gpga_double r2l = gpga_double_add(r2t, rr1l);
        gpga_double rr2h = gpga_double_zero(0u);
        gpga_double rr2l = gpga_double_zero(0u);
        Mul22(&rr2h, &rr2l, r2h, r2l, rr1h, rr1l);
        if (expoX >= 2) {
          gpga_double r3h = gpga_double_zero(0u);
          gpga_double r3t = gpga_double_zero(0u);
          Add12(&r3h, &r3t, gpga_double_from_u32(2u), rr2h);
          gpga_double r3l = gpga_double_add(r3t, rr2l);
          gpga_double rr3h = gpga_double_zero(0u);
          gpga_double rr3l = gpga_double_zero(0u);
          Mul22(&rr3h, &rr3l, r3h, r3l, rr2h, rr2l);
          expm1h = rr3h;
          expm1m = rr3l;
        } else {
          expm1h = rr2h;
          expm1m = rr2l;
        }
      } else {
        expm1h = rr1h;
        expm1m = rr1l;
      }
    } else {
      expm1h = polyh;
      expm1m = polyl;
    }

    gpga_double res = gpga_double_zero(0u);
    if (gpga_test_and_return_rd(expm1h, expm1m, expm1_ROUNDCSTDIRECTRD,
                                &res)) {
      return res;
    }
    gpga_expm1_direct_td(&expm1h, &expm1m, &expm1l, x, xSqHalfh, xSqHalfl, xSqh,
                         xSql, expoX);
    return ReturnRoundDownwards3(expm1h, expm1m, expm1l);
  }

  gpga_double xMultLog2InvMult2L =
      gpga_double_mul(x, expm1_log2InvMult2L);
  gpga_double shiftedXMult =
      gpga_double_add(xMultLog2InvMult2L, expm1_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, expm1_shiftConst);
  int k = (int)gpga_u64_lo(shiftedXMult);
  int M = k >> 12;
  int index1 = k & (int)expm1_INDEXMASK1;
  int index2 = (k & (int)expm1_INDEXMASK2) >> 6;

  gpga_double s1 = gpga_double_zero(0u);
  gpga_double s2 = gpga_double_zero(0u);
  Mul12(&s1, &s2, expm1_msLog2Div2Lh, kd);
  gpga_double s3 = gpga_double_mul(kd, expm1_msLog2Div2Lm);
  gpga_double s4 = gpga_double_add(s2, s3);
  gpga_double s5 = gpga_double_add(x, s1);
  Add12Cond(&rh, &rm, s5, s4);

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(expm1_quickCommonpolyC3h, rh);
  gpga_double rhSquareHalf = gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(expm1_quickCommonpolyC4h, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  Add12(&polyTblh, &polyTblm, t11, t13);

  polyTblh = gpga_exp_adjust_exponent(polyTblh, M);
  if (!gpga_double_is_zero(polyTblm)) {
    polyTblm = gpga_exp_adjust_exponent(polyTblm, M);
  }

  gpga_double exph = polyTblh;
  gpga_double expm = polyTblm;
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&t1, &t2, gpga_double_from_s32(-1), exph);
  gpga_double t3 = gpga_double_add(t2, expm);
  Add12Cond(&expm1h, &expm1m, t1, t3);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(expm1h, expm1m, expm1_ROUNDCSTCOMMONRD, &res)) {
    return res;
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         expm1_msLog2Div2Lh, expm1_msLog2Div2Lm, expm1_msLog2Div2Ll);
  gpga_double t4 = gpga_double_add(x, msLog2Div2LMultKh);
  Add12Cond(&rh, &t2, t4, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_expm1_common_td(&expm1h, &expm1m, &expm1l, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l, M);
  return ReturnRoundDownwards3(expm1h, expm1m, expm1l);
}

inline gpga_double gpga_expm1_ru(gpga_double x) {
  gpga_double xSqh = gpga_double_zero(0u);
  gpga_double xSql = gpga_double_zero(0u);
  gpga_double xSqHalfh = gpga_double_zero(0u);
  gpga_double xSqHalfl = gpga_double_zero(0u);
  gpga_double xCubeh = gpga_double_zero(0u);
  gpga_double xCubel = gpga_double_zero(0u);
  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  gpga_double expm1h = gpga_double_zero(0u);
  gpga_double expm1m = gpga_double_zero(0u);
  gpga_double expm1l = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);

  uint xhi = gpga_u64_hi(x);
  uint xIntHi = xhi & 0x7fffffffU;
  uint xlo = gpga_u64_lo(x);

  if (xIntHi < expm1_RETURNXBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    return gpga_double_next_up(x);
  }

  if (xIntHi >= expm1_SIMPLEOVERFLOWBOUND) {
    if (xIntHi >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xlo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xhi & 0x80000000u) == 0u) {
        return gpga_double_add(x, x);
      }
      return gpga_double_from_s32(-1);
    }
    if (gpga_double_gt(x, expm1_OVERFLOWBOUND)) {
      return gpga_double_mul(expm1_LARGEST, expm1_LARGEST);
    }
  }

  if (gpga_double_lt(x, expm1_MINUSONEBOUND)) {
    return expm1_MINUSONEPLUSONEULP;
  }

  if (xIntHi < expm1_DIRECTINTERVALBOUND) {
    int expoX = (int)((xIntHi & 0x7ff00000u) >> 20) - (1023 - 5);
    if (expoX >= 0) {
      int delta = -((expoX + 1) << 20);
      xhi = (uint)((int)xhi + delta);
      x = gpga_u64_from_words(xhi, xlo);
      xIntHi = xhi & 0x7fffffffU;
    }

    Mul12(&xSqh, &xSql, x, x);
    gpga_double middlePoly =
        gpga_double_add(expm1_quickDirectpolyC4h,
                        gpga_double_mul(x, expm1_quickDirectpolyC5h));
    gpga_double doublePoly = middlePoly;

    if (xIntHi > expm1_SPECIALINTERVALBOUND) {
      gpga_double highPoly = gpga_double_add(
          expm1_quickDirectpolyC6h,
          gpga_double_mul(
              x, gpga_double_add(
                     expm1_quickDirectpolyC7h,
                     gpga_double_mul(
                         x, gpga_double_add(expm1_quickDirectpolyC8h,
                                            gpga_double_mul(
                                                x, expm1_quickDirectpolyC9h))))));
      gpga_double highPolyWithSquare = gpga_double_mul(xSqh, highPoly);
      doublePoly = gpga_double_add(middlePoly, highPolyWithSquare);
    }

    gpga_double tt1h = gpga_double_mul(x, doublePoly);
    xSqHalfh = gpga_double_mul(xSqh, gpga_double_const_inv2());
    xSqHalfl = gpga_double_mul(xSql, gpga_double_const_inv2());
    gpga_double t2h = gpga_double_zero(0u);
    gpga_double templ = gpga_double_zero(0u);
    Add12(&t2h, &templ, x, xSqHalfh);
    gpga_double t2l = gpga_double_add(templ, xSqHalfl);

    gpga_double t1h = gpga_double_zero(0u);
    gpga_double t1l = gpga_double_zero(0u);
    Add12(&t1h, &t1l, expm1_quickDirectpolyC3h, tt1h);
    Mul122(&xCubeh, &xCubel, x, xSqh, xSql);
    gpga_double tt3h = gpga_double_zero(0u);
    gpga_double tt3l = gpga_double_zero(0u);
    Mul22(&tt3h, &tt3l, xCubeh, xCubel, t1h, t1l);
    Add22(&polyh, &polyl, t2h, t2l, tt3h, tt3l);

    if (expoX >= 0) {
      gpga_double r1h = gpga_double_zero(0u);
      gpga_double r1t = gpga_double_zero(0u);
      Add12(&r1h, &r1t, gpga_double_from_u32(2u), polyh);
      gpga_double r1l = gpga_double_add(r1t, polyl);
      gpga_double rr1h = gpga_double_zero(0u);
      gpga_double rr1l = gpga_double_zero(0u);
      Mul22(&rr1h, &rr1l, r1h, r1l, polyh, polyl);
      if (expoX >= 1) {
        gpga_double r2h = gpga_double_zero(0u);
        gpga_double r2t = gpga_double_zero(0u);
        Add12(&r2h, &r2t, gpga_double_from_u32(2u), rr1h);
        gpga_double r2l = gpga_double_add(r2t, rr1l);
        gpga_double rr2h = gpga_double_zero(0u);
        gpga_double rr2l = gpga_double_zero(0u);
        Mul22(&rr2h, &rr2l, r2h, r2l, rr1h, rr1l);
        if (expoX >= 2) {
          gpga_double r3h = gpga_double_zero(0u);
          gpga_double r3t = gpga_double_zero(0u);
          Add12(&r3h, &r3t, gpga_double_from_u32(2u), rr2h);
          gpga_double r3l = gpga_double_add(r3t, rr2l);
          gpga_double rr3h = gpga_double_zero(0u);
          gpga_double rr3l = gpga_double_zero(0u);
          Mul22(&rr3h, &rr3l, r3h, r3l, rr2h, rr2l);
          expm1h = rr3h;
          expm1m = rr3l;
        } else {
          expm1h = rr2h;
          expm1m = rr2l;
        }
      } else {
        expm1h = rr1h;
        expm1m = rr1l;
      }
    } else {
      expm1h = polyh;
      expm1m = polyl;
    }

    gpga_double res = gpga_double_zero(0u);
    if (gpga_test_and_return_ru(expm1h, expm1m, expm1_ROUNDCSTDIRECTRD,
                                &res)) {
      return res;
    }
    gpga_expm1_direct_td(&expm1h, &expm1m, &expm1l, x, xSqHalfh, xSqHalfl, xSqh,
                         xSql, expoX);
    return ReturnRoundUpwards3(expm1h, expm1m, expm1l);
  }

  gpga_double xMultLog2InvMult2L =
      gpga_double_mul(x, expm1_log2InvMult2L);
  gpga_double shiftedXMult =
      gpga_double_add(xMultLog2InvMult2L, expm1_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, expm1_shiftConst);
  int k = (int)gpga_u64_lo(shiftedXMult);
  int M = k >> 12;
  int index1 = k & (int)expm1_INDEXMASK1;
  int index2 = (k & (int)expm1_INDEXMASK2) >> 6;

  gpga_double s1 = gpga_double_zero(0u);
  gpga_double s2 = gpga_double_zero(0u);
  Mul12(&s1, &s2, expm1_msLog2Div2Lh, kd);
  gpga_double s3 = gpga_double_mul(kd, expm1_msLog2Div2Lm);
  gpga_double s4 = gpga_double_add(s2, s3);
  gpga_double s5 = gpga_double_add(x, s1);
  Add12Cond(&rh, &rm, s5, s4);

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(expm1_quickCommonpolyC3h, rh);
  gpga_double rhSquareHalf = gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(expm1_quickCommonpolyC4h, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  Add12(&polyTblh, &polyTblm, t11, t13);

  polyTblh = gpga_exp_adjust_exponent(polyTblh, M);
  if (!gpga_double_is_zero(polyTblm)) {
    polyTblm = gpga_exp_adjust_exponent(polyTblm, M);
  }

  gpga_double exph = polyTblh;
  gpga_double expm = polyTblm;
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&t1, &t2, gpga_double_from_s32(-1), exph);
  gpga_double t3 = gpga_double_add(t2, expm);
  Add12Cond(&expm1h, &expm1m, t1, t3);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(expm1h, expm1m, expm1_ROUNDCSTCOMMONRD, &res)) {
    return res;
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         expm1_msLog2Div2Lh, expm1_msLog2Div2Lm, expm1_msLog2Div2Ll);
  gpga_double t4 = gpga_double_add(x, msLog2Div2LMultKh);
  Add12Cond(&rh, &t2, t4, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_expm1_common_td(&expm1h, &expm1m, &expm1l, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l, M);
  return ReturnRoundUpwards3(expm1h, expm1m, expm1l);
}

inline gpga_double gpga_expm1_rz(gpga_double x) {
  gpga_double xSqh = gpga_double_zero(0u);
  gpga_double xSql = gpga_double_zero(0u);
  gpga_double xSqHalfh = gpga_double_zero(0u);
  gpga_double xSqHalfl = gpga_double_zero(0u);
  gpga_double xCubeh = gpga_double_zero(0u);
  gpga_double xCubel = gpga_double_zero(0u);
  gpga_double polyh = gpga_double_zero(0u);
  gpga_double polyl = gpga_double_zero(0u);
  gpga_double expm1h = gpga_double_zero(0u);
  gpga_double expm1m = gpga_double_zero(0u);
  gpga_double expm1l = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_double tbl1h = gpga_double_zero(0u);
  gpga_double tbl1m = gpga_double_zero(0u);
  gpga_double tbl1l = gpga_double_zero(0u);
  gpga_double tbl2h = gpga_double_zero(0u);
  gpga_double tbl2m = gpga_double_zero(0u);
  gpga_double tbl2l = gpga_double_zero(0u);

  uint xhi = gpga_u64_hi(x);
  uint xIntHi = xhi & 0x7fffffffU;
  uint xlo = gpga_u64_lo(x);

  if (xIntHi < expm1_RETURNXBOUND) {
    if (gpga_double_is_zero(x)) {
      return x;
    }
    if (gpga_double_sign(x) != 0u) {
      return gpga_double_next_up(x);
    }
    return x;
  }

  if (xIntHi >= expm1_SIMPLEOVERFLOWBOUND) {
    if (xIntHi >= 0x7ff00000u) {
      if (((xIntHi & 0x000fffffU) | xlo) != 0u) {
        return gpga_double_add(x, x);
      }
      if ((xhi & 0x80000000u) == 0u) {
        return gpga_double_add(x, x);
      }
      return gpga_double_from_s32(-1);
    }
    if (gpga_double_gt(x, expm1_OVERFLOWBOUND)) {
      gpga_double one = gpga_double_from_u32(1u);
      return gpga_double_mul(expm1_LARGEST,
                             gpga_double_add(one, expm1_SMALLEST));
    }
  }

  if (gpga_double_lt(x, expm1_MINUSONEBOUND)) {
    return expm1_MINUSONEPLUSONEULP;
  }

  if (xIntHi < expm1_DIRECTINTERVALBOUND) {
    int expoX = (int)((xIntHi & 0x7ff00000u) >> 20) - (1023 - 5);
    if (expoX >= 0) {
      int delta = -((expoX + 1) << 20);
      xhi = (uint)((int)xhi + delta);
      x = gpga_u64_from_words(xhi, xlo);
      xIntHi = xhi & 0x7fffffffU;
    }

    Mul12(&xSqh, &xSql, x, x);
    gpga_double middlePoly =
        gpga_double_add(expm1_quickDirectpolyC4h,
                        gpga_double_mul(x, expm1_quickDirectpolyC5h));
    gpga_double doublePoly = middlePoly;

    if (xIntHi > expm1_SPECIALINTERVALBOUND) {
      gpga_double highPoly = gpga_double_add(
          expm1_quickDirectpolyC6h,
          gpga_double_mul(
              x, gpga_double_add(
                     expm1_quickDirectpolyC7h,
                     gpga_double_mul(
                         x, gpga_double_add(expm1_quickDirectpolyC8h,
                                            gpga_double_mul(
                                                x, expm1_quickDirectpolyC9h))))));
      gpga_double highPolyWithSquare = gpga_double_mul(xSqh, highPoly);
      doublePoly = gpga_double_add(middlePoly, highPolyWithSquare);
    }

    gpga_double tt1h = gpga_double_mul(x, doublePoly);
    xSqHalfh = gpga_double_mul(xSqh, gpga_double_const_inv2());
    xSqHalfl = gpga_double_mul(xSql, gpga_double_const_inv2());
    gpga_double t2h = gpga_double_zero(0u);
    gpga_double templ = gpga_double_zero(0u);
    Add12(&t2h, &templ, x, xSqHalfh);
    gpga_double t2l = gpga_double_add(templ, xSqHalfl);

    gpga_double t1h = gpga_double_zero(0u);
    gpga_double t1l = gpga_double_zero(0u);
    Add12(&t1h, &t1l, expm1_quickDirectpolyC3h, tt1h);
    Mul122(&xCubeh, &xCubel, x, xSqh, xSql);
    gpga_double tt3h = gpga_double_zero(0u);
    gpga_double tt3l = gpga_double_zero(0u);
    Mul22(&tt3h, &tt3l, xCubeh, xCubel, t1h, t1l);
    Add22(&polyh, &polyl, t2h, t2l, tt3h, tt3l);

    if (expoX >= 0) {
      gpga_double r1h = gpga_double_zero(0u);
      gpga_double r1t = gpga_double_zero(0u);
      Add12(&r1h, &r1t, gpga_double_from_u32(2u), polyh);
      gpga_double r1l = gpga_double_add(r1t, polyl);
      gpga_double rr1h = gpga_double_zero(0u);
      gpga_double rr1l = gpga_double_zero(0u);
      Mul22(&rr1h, &rr1l, r1h, r1l, polyh, polyl);
      if (expoX >= 1) {
        gpga_double r2h = gpga_double_zero(0u);
        gpga_double r2t = gpga_double_zero(0u);
        Add12(&r2h, &r2t, gpga_double_from_u32(2u), rr1h);
        gpga_double r2l = gpga_double_add(r2t, rr1l);
        gpga_double rr2h = gpga_double_zero(0u);
        gpga_double rr2l = gpga_double_zero(0u);
        Mul22(&rr2h, &rr2l, r2h, r2l, rr1h, rr1l);
        if (expoX >= 2) {
          gpga_double r3h = gpga_double_zero(0u);
          gpga_double r3t = gpga_double_zero(0u);
          Add12(&r3h, &r3t, gpga_double_from_u32(2u), rr2h);
          gpga_double r3l = gpga_double_add(r3t, rr2l);
          gpga_double rr3h = gpga_double_zero(0u);
          gpga_double rr3l = gpga_double_zero(0u);
          Mul22(&rr3h, &rr3l, r3h, r3l, rr2h, rr2l);
          expm1h = rr3h;
          expm1m = rr3l;
        } else {
          expm1h = rr2h;
          expm1m = rr2l;
        }
      } else {
        expm1h = rr1h;
        expm1m = rr1l;
      }
    } else {
      expm1h = polyh;
      expm1m = polyl;
    }

    gpga_double res = gpga_double_zero(0u);
    if (gpga_test_and_return_rz(expm1h, expm1m, expm1_ROUNDCSTDIRECTRD,
                                &res)) {
      return res;
    }
    gpga_expm1_direct_td(&expm1h, &expm1m, &expm1l, x, xSqHalfh, xSqHalfl, xSqh,
                         xSql, expoX);
    return ReturnRoundTowardsZero3(expm1h, expm1m, expm1l);
  }

  gpga_double xMultLog2InvMult2L =
      gpga_double_mul(x, expm1_log2InvMult2L);
  gpga_double shiftedXMult =
      gpga_double_add(xMultLog2InvMult2L, expm1_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, expm1_shiftConst);
  int k = (int)gpga_u64_lo(shiftedXMult);
  int M = k >> 12;
  int index1 = k & (int)expm1_INDEXMASK1;
  int index2 = (k & (int)expm1_INDEXMASK2) >> 6;

  gpga_double s1 = gpga_double_zero(0u);
  gpga_double s2 = gpga_double_zero(0u);
  Mul12(&s1, &s2, expm1_msLog2Div2Lh, kd);
  gpga_double s3 = gpga_double_mul(kd, expm1_msLog2Div2Lm);
  gpga_double s4 = gpga_double_add(s2, s3);
  gpga_double s5 = gpga_double_add(x, s1);
  Add12Cond(&rh, &rm, s5, s4);

  tbl1h = exp_twoPowerIndex1[index1].hi;
  tbl1m = exp_twoPowerIndex1[index1].mi;
  tbl2h = exp_twoPowerIndex2[index2].hi;
  tbl2m = exp_twoPowerIndex2[index2].mi;

  gpga_double rhSquare = gpga_double_mul(rh, rh);
  gpga_double rhC3 = gpga_double_mul(expm1_quickCommonpolyC3h, rh);
  gpga_double rhSquareHalf = gpga_double_mul(rhSquare, gpga_double_const_inv2());
  gpga_double monomialCube = gpga_double_mul(rhC3, rhSquare);
  gpga_double rhFour = gpga_double_mul(rhSquare, rhSquare);
  gpga_double monomialFour = gpga_double_mul(expm1_quickCommonpolyC4h, rhFour);
  gpga_double highPoly = gpga_double_add(monomialCube, monomialFour);
  gpga_double highPolyWithSquare = gpga_double_add(rhSquareHalf, highPoly);

  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul22(&tablesh, &tablesl, tbl1h, tbl1m, tbl2h, tbl2m);

  gpga_double t8 = gpga_double_add(rm, highPolyWithSquare);
  gpga_double t9 = gpga_double_add(rh, t8);
  gpga_double t10 = gpga_double_mul(tablesh, t9);
  gpga_double t11 = gpga_double_zero(0u);
  gpga_double t12 = gpga_double_zero(0u);
  Add12(&t11, &t12, tablesh, t10);
  gpga_double t13 = gpga_double_add(t12, tablesl);
  gpga_double polyTblh = gpga_double_zero(0u);
  gpga_double polyTblm = gpga_double_zero(0u);
  Add12(&polyTblh, &polyTblm, t11, t13);

  polyTblh = gpga_exp_adjust_exponent(polyTblh, M);
  if (!gpga_double_is_zero(polyTblm)) {
    polyTblm = gpga_exp_adjust_exponent(polyTblm, M);
  }

  gpga_double exph = polyTblh;
  gpga_double expm = polyTblm;
  gpga_double t1 = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  Add12Cond(&t1, &t2, gpga_double_from_s32(-1), exph);
  gpga_double t3 = gpga_double_add(t2, expm);
  Add12Cond(&expm1h, &expm1m, t1, t3);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_rz(expm1h, expm1m, expm1_ROUNDCSTCOMMONRD, &res)) {
    return res;
  }

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         expm1_msLog2Div2Lh, expm1_msLog2Div2Lm, expm1_msLog2Div2Ll);
  gpga_double t4 = gpga_double_add(x, msLog2Div2LMultKh);
  Add12Cond(&rh, &t2, t4, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  tbl1l = exp_twoPowerIndex1[index1].lo;
  tbl2l = exp_twoPowerIndex2[index2].lo;
  gpga_expm1_common_td(&expm1h, &expm1m, &expm1l, rh, rm, rl, tbl1h, tbl1m,
                       tbl1l, tbl2h, tbl2m, tbl2l, M);
  return ReturnRoundTowardsZero3(expm1h, expm1m, expm1l);
}


// CRLIBM_EXP13
inline void gpga_exp13(thread int* exponent, thread gpga_double* exph,
                       thread gpga_double* expm, thread gpga_double* expl,
                       gpga_double x) {
  uint xhi = gpga_u64_hi(x);
  if ((xhi & 0x7ff00000u) == 0u) {
    *exph = gpga_double_from_u32(1u);
    *expm = gpga_double_zero(0u);
    *expl = gpga_double_zero(0u);
    if (exponent) {
      *exponent = 0;
    }
    return;
  }

  gpga_double x_mult = gpga_double_mul(x, exp_log2InvMult2L);
  gpga_double shifted = gpga_double_add(x_mult, exp_shiftConst);
  gpga_double kd = gpga_double_sub(shifted, exp_shiftConst);
  int k = (int)gpga_u64_lo(shifted);
  int M = k >> exp_L;
  int index1 = k & (int)exp_INDEXMASK1;
  int index2 = (k & (int)exp_INDEXMASK2) >> exp_LHALF;

  gpga_double tbl1h = exp_twoPowerIndex1[index1].hi;
  gpga_double tbl1m = exp_twoPowerIndex1[index1].mi;
  gpga_double tbl1l = exp_twoPowerIndex1[index1].lo;
  gpga_double tbl2h = exp_twoPowerIndex2[index2].hi;
  gpga_double tbl2m = exp_twoPowerIndex2[index2].mi;
  gpga_double tbl2l = exp_twoPowerIndex2[index2].lo;

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         exp_msLog2Div2Lh, exp_msLog2Div2Lm, exp_msLog2Div2Ll);
  gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  gpga_exp_td_accurate(exph, expm, expl, rh, rm, rl, tbl1h, tbl1m, tbl1l,
                       tbl2h, tbl2m, tbl2l);
  if (exponent) {
    *exponent = M;
  }
}

inline void gpga_expm1_13(thread gpga_double* expm1h,
                          thread gpga_double* expm1m,
                          thread gpga_double* expm1l, gpga_double x) {
  uint xhi = gpga_u64_hi(x);
  uint xIntHi = xhi & 0x7fffffffU;
  uint xlo = gpga_u64_lo(x);

  if (xIntHi < expm1_DIRECTINTERVALBOUND) {
    int expoX = (int)((xIntHi & 0x7ff00000u) >> 20) - (1023 - 5);
    if (expoX >= 0) {
      int delta = -((expoX + 1) << 20);
      xhi = (uint)((int)xhi + delta);
      x = gpga_u64_from_words(xhi, xlo);
      xIntHi = xhi & 0x7fffffffU;
    }

    gpga_double xSqh = gpga_double_zero(0u);
    gpga_double xSql = gpga_double_zero(0u);
    Mul12(&xSqh, &xSql, x, x);
    gpga_double xSqHalfh = gpga_double_mul(xSqh, gpga_double_const_inv2());
    gpga_double xSqHalfl = gpga_double_mul(xSql, gpga_double_const_inv2());
    gpga_expm1_direct_td(expm1h, expm1m, expm1l, x, xSqHalfh, xSqHalfl, xSqh,
                         xSql, expoX);
    return;
  }

  gpga_double xMultLog2InvMult2L = gpga_double_mul(x, expm1_log2InvMult2L);
  gpga_double shiftedXMult = gpga_double_add(xMultLog2InvMult2L,
                                             expm1_shiftConst);
  gpga_double kd = gpga_double_sub(shiftedXMult, expm1_shiftConst);
  int k = (int)gpga_u64_lo(shiftedXMult);
  int M = k >> 12;
  int index1 = k & (int)expm1_INDEXMASK1;
  int index2 = (k & (int)expm1_INDEXMASK2) >> 6;

  gpga_double msLog2Div2LMultKh = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKm = gpga_double_zero(0u);
  gpga_double msLog2Div2LMultKl = gpga_double_zero(0u);
  Mul133(&msLog2Div2LMultKh, &msLog2Div2LMultKm, &msLog2Div2LMultKl, kd,
         expm1_msLog2Div2Lh, expm1_msLog2Div2Lm, expm1_msLog2Div2Ll);
  gpga_double t1 = gpga_double_add(x, msLog2Div2LMultKh);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Add12Cond(&rh, &t2, t1, msLog2Div2LMultKm);
  Add12Cond(&rm, &rl, t2, msLog2Div2LMultKl);

  gpga_double tbl1h = exp_twoPowerIndex1[index1].hi;
  gpga_double tbl1m = exp_twoPowerIndex1[index1].mi;
  gpga_double tbl1l = exp_twoPowerIndex1[index1].lo;
  gpga_double tbl2h = exp_twoPowerIndex2[index2].hi;
  gpga_double tbl2m = exp_twoPowerIndex2[index2].mi;
  gpga_double tbl2l = exp_twoPowerIndex2[index2].lo;

  gpga_expm1_common_td(expm1h, expm1m, expm1l, rh, rm, rl, tbl1h, tbl1m, tbl1l,
                       tbl2h, tbl2m, tbl2l, M);
}

// CRLIBM_POW
struct GpgaPowArgRed {
  gpga_double ri;
  gpga_double logih;
  gpga_double logim;
  gpga_double logil;
};

GPGA_CONST int pow_L = 8;
GPGA_CONST int pow_MAXINDEX = 106;
GPGA_CONST int pow_INDEXMASK = 255;
GPGA_CONST gpga_double pow_two52 = 0x4330000000000000ul;
GPGA_CONST gpga_double pow_two53 = 0x4340000000000000ul;
GPGA_CONST gpga_double pow_two54 = 0x4350000000000000ul;
GPGA_CONST gpga_double pow_twoM55 = 0x3c80000000000000ul;
GPGA_CONST gpga_double pow_logFastCoeff = 0x3ff6a09e667f3bcdul;
GPGA_CONST gpga_double pow_LARGEST = 0x7fe0000000000000ul;
GPGA_CONST gpga_double pow_SMALLEST = 0x0010000000000000ul;
GPGA_CONST gpga_double pow_shiftConst = 0x4338000000000000ul;
GPGA_CONST gpga_double pow_shiftConstTwoM13 = 0x4268000000000000ul;
GPGA_CONST gpga_double pow_two13 = 0x40c0000000000000ul;
GPGA_CONST gpga_double pow_twoM13 = 0x3f20000000000000ul;
GPGA_CONST uint pow_INDEXMASK1 = 0x1f;
GPGA_CONST uint pow_INDEXMASK2 = 0x1fe0;
GPGA_CONST gpga_double pow_RNROUNDCST = 0x3ff0410410410411ul;
GPGA_CONST gpga_double pow_RDROUNDCST = 0x3c30000000000000ul;
GPGA_CONST gpga_double pow_SUBNORMROUNDCST = 0x43a0000000000000ul;
GPGA_CONST gpga_double pow_PRECISEROUNDCST = 0x3890000000000000ul;
GPGA_CONST gpga_double pow_twoM1000 = 0x0170000000000000ul;
GPGA_CONST gpga_double pow_twoM74 = 0x3b50000000000000ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_1h = 0x3ff71547652b82feul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_1m = 0x3c7777d18d78d3a4ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_2h = 0xbfe71547652b82feul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_2m = 0xbc69fe25073392e8ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_3h = 0x3fdec709dc3a03fdul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_4h = 0xbfd71547652ae169ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_5h = 0x3fd2776c50f37de2ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_6h = 0xbfcec713f58b6770ul;
GPGA_CONST gpga_double pow_log2_70_p_coeff_7h = 0x3fca61692f10726aul;
GPGA_CONST gpga_double exp2_p_coeff_0h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double exp2_p_coeff_1h = 0x3fe62e42fefa39eful;
GPGA_CONST gpga_double exp2_p_coeff_2h = 0x3fcebfbdff932fc8ul;
GPGA_CONST gpga_double exp2_p_coeff_3h = 0x3fac6b091734cf02ul;
struct GpgaPowTwoPowerIndex1 { gpga_double hiM1; gpga_double hi; gpga_double mi; gpga_double lo; };
struct GpgaPowTwoPowerIndex2 { gpga_double hi; gpga_double mi; gpga_double lo; };
GPGA_CONST GpgaPowTwoPowerIndex1 pow_twoPowerIndex1[32] = {
  { 0x0000000000000000ul, 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3f162e807ee7e5b6ul, 0x3ff00058ba01fba0ul, 0xbc9a4a4d4cad39feul, 0x39317c3e43a86f9ful },
  { 0x3f262ebdffb8ed74ul, 0x3ff000b175effdc7ul, 0x3c9ae8e38c59c72aul, 0x39339726694630e3ul },
  { 0x3f30a33ca111ffa6ul, 0x3ff0010a33ca1120ul, 0xbc568ddbffb2ac39ul, 0x38dce699b9e63f7ful },
  { 0x3f362f3904051fa1ul, 0x3ff00162f3904052ul, 0xbc57b5d0d58ea8f4ul, 0x38fe5e06ddd31156ul },
  { 0x3f3bbb54296065cful, 0x3ff001bbb5429606ul, 0x3c973c902846716eul, 0x3927a6cb3bda8909ul },
  { 0x3f40a3c708e73282ul, 0x3ff0021478e11ce6ul, 0x3c94115cb6b16a8eul, 0x3905a0768b51f609ul },
  { 0x3f4369f35efcd9e4ul, 0x3ff0026d3e6bdf9bul, 0x3c8e3a2b72b6b281ul, 0xb9182cfa12767020ul },
  { 0x3f46302f17467628ul, 0x3ff002c605e2e8cful, 0xbc8d7c96f201bb2ful, 0x390d008403605217ul },
  { 0x3f48f67a32195645ul, 0x3ff0031ecf46432bul, 0xbc8bad1eadef26ecul, 0x3920af0d1ad70fa3ul },
  { 0x3f4bbcd4afcacb09ul, 0x3ff003779a95f959ul, 0x3c984711d4c35e9ful, 0x39289bc16f765708ul },
  { 0x3f4e833e90b0271bul, 0x3ff003d067d21605ul, 0xbc7ca6f866b43641ul, 0x391e62c4a3f5c762ul },
  { 0x3f50a4dbea8f5f7eul, 0x3ff0042936faa3d8ul, 0xbc80484245243777ul, 0xb924535b7f8c1e2dul },
  { 0x3f5208203eb5f482ul, 0x3ff00482080fad7dul, 0x3c804c99b7c49394ul, 0x392d979dd6b3cc75ul },
  { 0x3f536b6c44f67eb5ul, 0x3ff004dadb113da0ul, 0xbc94b237da2025f9ul, 0xb938ba92f6b25456ul },
  { 0x3f54cebffd7bab1bul, 0x3ff00533afff5eebul, 0xbc8c9691b7ee1fa4ul, 0x391caf699ef1ab9eul },
  { 0x3f56321b687027a8ul, 0x3ff0058c86da1c0aul, 0xbc75e00e62d6b30dul, 0xb8e30c72e81f4294ul },
  { 0x3f57957e85fea33cul, 0x3ff005e55fa17fa9ul, 0xbc88816ea30c67b1ul, 0xb9205764e11b938eul },
  { 0x3f58f8e95651cda2ul, 0x3ff0063e3a559473ul, 0x3c9a1d6cedbb9481ul, 0xb9134a5384e6f0b9ul },
  { 0x3f5a5c5bd9945793ul, 0x3ff0069716f66516ul, 0xbc7b3d4ea145624aul, 0x39121bde7a0164c1ul },
  { 0x3f5bbfd60ff0f2b5ul, 0x3ff006eff583fc3dul, 0xbc94acf197a00142ul, 0x393f8d0580865d2eul },
  { 0x3f5d2357f992519bul, 0x3ff00748d5fe6494ul, 0x3c99aab0f204c611ul, 0xb91646a2e8467872ul },
  { 0x3f5e86e196a327c3ul, 0x3ff007a1b865a8caul, 0xbc6eaf2ea42391a5ul, 0xb90002bcb3ae9a99ul },
  { 0x3f5fea72e74e2999ul, 0x3ff007fa9cb9d38aul, 0x3c99908ac09487d4ul, 0x392c9dc7fcd469b1ul },
  { 0x3f60a705f5df063bul, 0x3ff0085382faef83ul, 0x3c7da93f90835f75ul, 0x390c3c5aedee9851ul },
  { 0x3f6158d6520ec351ul, 0x3ff008ac6b290762ul, 0xbc95eeea9c36fee1ul, 0x3938b3d8722c624dul },
  { 0x3f620aaa884ba7a5ul, 0x3ff00905554425d4ul, 0xbc86a79084ab093cul, 0x3927217851d1ec6eul },
  { 0x3f62bc8298ab0f4aul, 0x3ff0095e414c5588ul, 0xbc96cee9c84386d4ul, 0x393655ba53fc413bul },
  { 0x3f636e5e834256c3ul, 0x3ff009b72f41a12bul, 0x3c986364f8fbe8f8ul, 0xb9180cbca335a7c3ul },
  { 0x3f64203e4826db0ful, 0x3ff00a101f24136eul, 0xbc9e2a80dba144b9ul, 0x393d4210536ae35ful },
  { 0x3f64d221e76df99ful, 0x3ff00a6910f3b6fdul, 0xbc882e8e14e3110eul, 0xb91706bd4eb22595ul },
  { 0x3f658409612d105eul, 0x3ff00ac204b09688ul, 0x3c879b63bed45265ul, 0x390163dde4b4c1e8ul },
};
GPGA_CONST GpgaPowTwoPowerIndex2 pow_twoPowerIndex2[256] = {
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3ff00b1afa5abcbful, 0xbc84f6b2a7609f71ul, 0xb90b55dd523f3c08ul },
  { 0x3ff0163da9fb3335ul, 0x3c9b61299ab8cdb7ul, 0x392bf48007d80987ul },
  { 0x3ff02168143b0281ul, 0xbc82bf310fc54eb6ul, 0x3929953ea727ff0bul },
  { 0x3ff02c9a3e778061ul, 0xbc719083535b085dul, 0xb919085b0a3d74d5ul },
  { 0x3ff037d42e11bbccul, 0x3c656811eeade11aul, 0x3901313d5abd77e9ul },
  { 0x3ff04315e86e7f85ul, 0xbc90a31c1977c96eul, 0xb8f912fbf44b4040ul },
  { 0x3ff04e5f72f654b1ul, 0x3c84c3793aa0d08dul, 0xb92f9c132b72afe2ul },
  { 0x3ff059b0d3158574ul, 0x3c8d73e2a475b465ul, 0x39105ff94f8d257eul },
  { 0x3ff0650a0e3c1f89ul, 0xbc95cb7b5799c397ul, 0x3933e0adfe6c4c98ul },
  { 0x3ff0706b29ddf6deul, 0xbc8c91dfe2b13c27ul, 0x391fb41f2e2c24abul },
  { 0x3ff07bd42b72a836ul, 0x3c83233454458700ul, 0xb9092b8d5099366eul },
  { 0x3ff0874518759bc8ul, 0x3c6186be4bb284fful, 0x39015820d96b414ful },
  { 0x3ff092bdf66607e0ul, 0xbc968063800a3fd1ul, 0x3904189ff8d63ef8ul },
  { 0x3ff09e3ecac6f383ul, 0x3c91487818316136ul, 0xb9348b45d1fdc259ul },
  { 0x3ff0a9c79b1f3919ul, 0x3c85d16c873d1d38ul, 0xb92cf8d9770223ddul },
  { 0x3ff0b5586cf9890ful, 0x3c98a62e4adc610bul, 0xb9367c9bd6ebf74cul },
  { 0x3ff0c0f145e46c85ul, 0x3c94f98906d21ceful, 0x39039d71c412378eul },
  { 0x3ff0cc922b7247f7ul, 0x3c901edc16e24f71ul, 0x393e8aac564e6fe3ul },
  { 0x3ff0d83b23395decul, 0xbc9bc14de43f316aul, 0xb91696bec12e389cul },
  { 0x3ff0e3ec32d3d1a2ul, 0x3c403a1727c57b53ul, 0xb8e5aa76994e9ddbul },
  { 0x3ff0efa55fdfa9c5ul, 0xbc949db9bc54021bul, 0xb93d4ad57103f1fcul },
  { 0x3ff0fb66affed31bul, 0xbc6b9bedc44ebd7bul, 0xb8faeb1f49d84259ul },
  { 0x3ff1073028d7233eul, 0x3c8d46eb1692fdd5ul, 0x391f57015b4875a8ul },
  { 0x3ff11301d0125b51ul, 0xbc96c51039449b3aul, 0x3929d58b988f562dul },
  { 0x3ff11edbab5e2ab6ul, 0xbc9ca454f703fb72ul, 0x38f3454b21b02588ul },
  { 0x3ff12abdc06c31ccul, 0xbc51b514b36ca5c7ul, 0xb8f08d8f42083120ul },
  { 0x3ff136a814f204abul, 0xbc67108fba48dcf0ul, 0x390c9a4e34e91caaul },
  { 0x3ff1429aaea92de0ul, 0xbc932fbf9af1369eul, 0xb932fe7bb4c76416ul },
  { 0x3ff14e95934f312eul, 0xbc8b91e839bf44abul, 0xb927ddfed6937232ul },
  { 0x3ff15a98c8a58e51ul, 0x3c82406ab9eeab0aul, 0xb9101b575279c474ul },
  { 0x3ff166a45471c3c2ul, 0x3c58f23b82ea1a32ul, 0x38fcec6f65f9f480ul },
  { 0x3ff172b83c7d517bul, 0xbc819041b9d78a76ul, 0x3924f2406aa13ff0ul },
  { 0x3ff17ed48695bbc0ul, 0x3c709e3fe2ac5a64ul, 0x3900f94cec9c9210ul },
  { 0x3ff18af9388c8deaul, 0xbc911023d1970f6cul, 0x391725f0040b97c5ul },
  { 0x3ff1972658375d2ful, 0x3c94aadd85f17e08ul, 0x392629678a30a399ul },
  { 0x3ff1a35beb6fcb75ul, 0x3c8e5b4c7b4968e4ul, 0x390ad36183926ae8ul },
  { 0x3ff1af99f8138a1cul, 0x3c97bf85a4b69280ul, 0xb915c2c423bf7bd0ul },
  { 0x3ff1bbe084045cd4ul, 0xbc995386352ef607ul, 0xb9240ca69503718eul },
  { 0x3ff1c82f95281c6bul, 0x3c9009778010f8c9ul, 0xb91875b881c94e67ul },
  { 0x3ff1d4873168b9aaul, 0x3c9e016e00a2643cul, 0x391ea62d0881b918ul },
  { 0x3ff1e0e75eb44027ul, 0xbc96fdd8088cb6deul, 0xb930459f81668706ul },
  { 0x3ff1ed5022fcd91dul, 0xbc91df98027bb78cul, 0x393e504d36c47475ul },
  { 0x3ff1f9c18438ce4dul, 0xbc9bf524a097af5cul, 0xb92786d77f83061cul },
  { 0x3ff2063b88628cd6ul, 0x3c8dc775814a8495ul, 0xb90781dbc16f1ea4ul },
  { 0x3ff212be3578a819ul, 0x3c93592d2cfcaac9ul, 0xb91664b40209c8aaul },
  { 0x3ff21f49917ddc96ul, 0x3c82a97e9494a5eeul, 0xb92693c2b3b7106bul },
  { 0x3ff22bdda27912d1ul, 0x3c8d34fb5577d69ful, 0xb91b872152843078ul },
  { 0x3ff2387a6e756238ul, 0x3c99b07eb6c70573ul, 0xb924d89f9af532e0ul },
  { 0x3ff2451ffb82140aul, 0x3c8acfcc911ca996ul, 0x3918463b513c7000ul },
  { 0x3ff251ce4fb2a63ful, 0x3c8ac155bef4f4a4ul, 0x38f1a9c8afdcf797ul },
  { 0x3ff25e85711ece75ul, 0x3c93e1a24ac31b2cul, 0x3935ba6e76088bcdul },
  { 0x3ff26b4565e27cddul, 0x3c82bd339940e9d9ul, 0x391277393a461b77ul },
  { 0x3ff2780e341ddf29ul, 0x3c9e067c05f9e76cul, 0xb93bd4b7cee4538bul },
  { 0x3ff284dfe1f56381ul, 0xbc9a4c3a8c3f0d7eul, 0x39367fdaa2e52d7dul },
  { 0x3ff291ba7591bb70ul, 0xbc82cc7228401cbdul, 0x38d7a3902d46e4c4ul },
  { 0x3ff29e9df51fdee1ul, 0x3c8612e8afad1255ul, 0x390de54485604690ul },
  { 0x3ff2ab8a66d10f13ul, 0xbc995743191690a7ul, 0xb92dde6d7e73b7f6ul },
  { 0x3ff2b87fd0dad990ul, 0xbc410adcd6381aa4ul, 0x38e0885fb8796dbdul },
  { 0x3ff2c57e39771b2ful, 0xbc950145a6eb5124ul, 0xb93e245c425cbfd4ul },
  { 0x3ff2d285a6e4030bul, 0x3c90024754db41d5ul, 0xb91ee9d8f8cb9307ul },
  { 0x3ff2df961f641589ul, 0x3c9d16cffbbce198ul, 0x392aadc67a5cf780ul },
  { 0x3ff2ecafa93e2f56ul, 0x3c71ca0f45d52383ul, 0x390d7b08dee6d12aul },
  { 0x3ff2f9d24abd886bul, 0xbc653c55532bda93ul, 0x390286089c742098ul },
  { 0x3ff306fe0a31b715ul, 0x3c86f46ad23182e4ul, 0x3917b7b2f09cd0d9ul },
  { 0x3ff31432edeeb2fdul, 0x3c8959a3f3f3fcd1ul, 0xb9291ceb071b81b5ul },
  { 0x3ff32170fc4cd831ul, 0x3c8a9ce78e18047cul, 0x391b778c882b85e8ul },
  { 0x3ff32eb83ba8ea32ul, 0xbc9c45e83cb4f318ul, 0xb93b78c73a0898b9ul },
  { 0x3ff33c08b26416fful, 0x3c932721843659a6ul, 0xb93406a2ea6cfc6bul },
  { 0x3ff3496266e3fa2dul, 0xbc835a75930881a4ul, 0xb90475af6a7b6cc9ul },
  { 0x3ff356c55f929ff1ul, 0xbc8b5cee5c4e4628ul, 0xb928e524e520d5f2ul },
  { 0x3ff36431a2de883bul, 0xbc8c3144a06cb85eul, 0x392f8bb041238096ul },
  { 0x3ff371a7373aa9cbul, 0xbc963aeabf42eae2ul, 0x39387e3e12516bfaul },
  { 0x3ff37f26231e754aul, 0xbc99f5ca9eceb23cul, 0x39057469ed7e12f8ul },
  { 0x3ff38cae6d05d866ul, 0xbc9e958d3c9904bdul, 0x3920a77a61404f21ul },
  { 0x3ff39a401b7140eful, 0xbc99a9a5fc8e2934ul, 0xb9318ff8ae910b7aul },
  { 0x3ff3a7db34e59ff7ul, 0xbc75e436d661f5e3ul, 0x3909b0b1ff17c296ul },
  { 0x3ff3b57fbfec6cf4ul, 0x3c954c66e26fff18ul, 0x393d68f8b2e3be80ul },
  { 0x3ff3c32dc313a8e5ul, 0xbc9efff8375d29c3ul, 0xb921143f2a93395aul },
  { 0x3ff3d0e544ede173ul, 0x3c7fe8d08c284c71ul, 0x38c1ba164ea65915ul },
  { 0x3ff3dea64c123422ul, 0x3c8ada0911f09ebcul, 0xb92808ba68fa8fb7ul },
  { 0x3ff3ec70df1c5175ul, 0xbc8af6637b8c9bcaul, 0xb909bcdef349ba26ul },
  { 0x3ff3fa4504ac801cul, 0xbc97d023f956f9f3ul, 0xb930473e3724200dul },
  { 0x3ff40822c367a024ul, 0x3c8bddf8b6f4d048ul, 0x39228b1c754495cful },
  { 0x3ff4160a21f72e2aul, 0xbc5ef3691c309278ul, 0xb8d32b43eafc6518ul },
  { 0x3ff423fb2709468aul, 0xbc98462dc0b314ddul, 0xb931cad978fffe80ul },
  { 0x3ff431f5d950a897ul, 0xbc81c7dde35f7999ul, 0x392903c496195feful },
  { 0x3ff43ffa3f84b9d4ul, 0x3c8880be9704c003ul, 0xb9294966ca4958dful },
  { 0x3ff44e086061892dul, 0x3c489b7a04ef80d0ul, 0xb8d0ac312de3d922ul },
  { 0x3ff45c2042a7d232ul, 0xbc68641982fb1f8eul, 0xb8f85a39e45a5ac8ul },
  { 0x3ff46a41ed1d0057ul, 0x3c9c944bd1648a76ul, 0x3937df404ff21f3aul },
  { 0x3ff4786d668b3237ul, 0xbc9c20f0ed445733ul, 0xb91cb6afa23d3b08ul },
  { 0x3ff486a2b5c13cd0ul, 0x3c73c1a3b69062f0ul, 0x390e1eebae743ac0ul },
  { 0x3ff494e1e192aed2ul, 0xbc83b2895e499ea0ul, 0x392f5c05bb2372a6ul },
  { 0x3ff4a32af0d7d3deul, 0x3c99cb62f3d1be56ul, 0x39191876c761e2c7ul },
  { 0x3ff4b17dea6db7d7ul, 0xbc8125b87f2897f0ul, 0x391824406a11ee2dul },
  { 0x3ff4bfdad5362a27ul, 0x3c7d4397afec42e2ul, 0x38ec06c7745c2b39ul },
  { 0x3ff4ce41b817c114ul, 0x3c905e29690abd5dul, 0xb92b977421877867ul },
  { 0x3ff4dcb299fddd0dul, 0x3c98ecdbbc6a7833ul, 0x391212c969559b43ul },
  { 0x3ff4eb2d81d8abfful, 0xbc95257d2e5d7a52ul, 0xb92e770e5a11db22ul },
  { 0x3ff4f9b2769d2ca7ul, 0xbc94b309d25957e3ul, 0xb8f1aa1fd7b685cdul },
  { 0x3ff508417f4531eeul, 0x3c7a249b49b7465ful, 0xb90f426d5f0a11f8ul },
  { 0x3ff516daa2cf6642ul, 0xbc8f768569bd93eful, 0x38f90e718226177dul },
  { 0x3ff5257de83f4eeful, 0xbc7c998d43efef71ul, 0xb910974a1675d1e8ul },
  { 0x3ff5342b569d4f82ul, 0xbc807abe1db13cadul, 0x390fa733951f214cul },
  { 0x3ff542e2f4f6ad27ul, 0x3c87926d192d5f7eul, 0xb91126782ea06baaul },
  { 0x3ff551a4ca5d920ful, 0xbc8d689cefede59bul, 0x3919c991771b0493ul },
  { 0x3ff56070dde910d2ul, 0xbc90fb6e168eebf0ul, 0x39391129ae575c71ul },
  { 0x3ff56f4736b527daul, 0x3c99bb2c011d93adul, 0xb90ff86852a613fful },
  { 0x3ff57e27dbe2c4cful, 0xbc90b98c8a57b9c4ul, 0xb93d39891f4faa20ul },
  { 0x3ff58d12d497c7fdul, 0x3c8295e15b9a1de8ul, 0xb92a26d92ad1e4c6ul },
  { 0x3ff59c0827ff07ccul, 0xbc97e2cee467e60ful, 0x392d0c772f1bbc25ul },
  { 0x3ff5ab07dd485429ul, 0x3c96324c054647adul, 0xb92744ee506fdafeul },
  { 0x3ff5ba11fba87a03ul, 0xbc9b77a14c233e1aul, 0x393476dfb1884200ul },
  { 0x3ff5c9268a5946b7ul, 0x3c3c4b1b816986a2ul, 0x388ec2735254978cul },
  { 0x3ff5d84590998b93ul, 0xbc9cd6a7a8b45643ul, 0x39326dcfecd1b7fbul },
  { 0x3ff5e76f15ad2148ul, 0x3c9ba6f93080e65eul, 0xb9395f9ab75fa7d6ul },
  { 0x3ff5f6a320dceb71ul, 0xbc89eadde3cdcf92ul, 0x3920e1a6fbc77479ul },
  { 0x3ff605e1b976dc09ul, 0xbc93e2429b56de47ul, 0xb9132c54b92e2588ul },
  { 0x3ff6152ae6cdf6f4ul, 0x3c9e4b3e4ab84c27ul, 0xb905a3ca64325ac8ul },
  { 0x3ff6247eb03a5585ul, 0xbc9383c17e40b497ul, 0x3905d8e757cfb991ul },
  { 0x3ff633dd1d1929fdul, 0x3c984710beb964e5ul, 0x39372e21510bddb6ul },
  { 0x3ff6434634ccc320ul, 0xbc8c483c759d8933ul, 0x3913904000c1c40ful },
  { 0x3ff652b9febc8fb7ul, 0xbc9ae3d5c9a73e09ul, 0x3933fda68a873c1aul },
  { 0x3ff6623882552225ul, 0xbc9bb60987591c34ul, 0x3934a337f4dc0a3bul },
  { 0x3ff671c1c70833f6ul, 0xbc8e8732586c6134ul, 0x392f59e80d44da25ul },
  { 0x3ff68155d44ca973ul, 0x3c6038ae44f73e65ul, 0xb8ef2803633b04fful },
  { 0x3ff690f4b19e9538ul, 0x3c8804bd9aeb445dul, 0xb928f8eac8bcebaaul },
  { 0x3ff6a09e667f3bcdul, 0xbc9bdd3413b26456ul, 0x39357d3e3adec175ul },
  { 0x3ff6b052fa75173eul, 0x3c7a38f52c9a9d0eul, 0x39096bba59626d18ul },
  { 0x3ff6c012750bdabful, 0xbc72895667ff0b0dul, 0x390fef5c58766c19ul },
  { 0x3ff6cfdcddd47645ul, 0x3c9c7aa9b6f17309ul, 0xb90880cb27e97d9eul },
  { 0x3ff6dfb23c651a2ful, 0xbc6bbe3a683c88abul, 0x38ca59f88abbe778ul },
  { 0x3ff6ef9298593ae5ul, 0xbc90b9749e1ac8b2ul, 0xb91a8db3ca2ad190ul },
  { 0x3ff6ff7df9519484ul, 0xbc883c0f25860ef6ul, 0xb91001923f4a956eul },
  { 0x3ff70f7466f42e87ul, 0x3c59d644d45aa65ful, 0xb8ed9ca88f47a2a1ul },
  { 0x3ff71f75e8ec5f74ul, 0xbc816e4786887a99ul, 0xb92269796953a4c3ul },
  { 0x3ff72f8286ead08aul, 0xbc920aa02cd62c72ul, 0xb9288dfb7e0baf87ul },
  { 0x3ff73f9a48a58174ul, 0xbc90a8d96c65d53cul, 0x39382ae217f3a768ul },
  { 0x3ff74fbd35d7cbfdul, 0x3c9047fd618a6e1cul, 0x393b42033fadb904ul },
  { 0x3ff75feb564267c9ul, 0xbc90245957316dd3ul, 0xb938f8e7fa19e5e8ul },
  { 0x3ff77024b1ab6e09ul, 0x3c9b7877169147f8ul, 0xb8feb9c5d1e7b193ul },
  { 0x3ff780694fde5d3ful, 0x3c9866b80a02162dul, 0xb9344d42307932f7ul },
  { 0x3ff790b938ac1cf6ul, 0x3c9349a862aadd3eul, 0xb910b109d64fbd5ful },
  { 0x3ff7a11473eb0187ul, 0xbc841577ee04992ful, 0xb8e4217a932d10d4ul },
  { 0x3ff7b17b0976cfdbul, 0xbc9bebb58468dc88ul, 0xb92303754b0bc06dul },
  { 0x3ff7c1ed0130c132ul, 0x3c9f124cd1164dd6ul, 0xb93d4d236cc2bb03ul },
  { 0x3ff7d26a62ff86f0ul, 0x3c91bddbfb72b8b4ul, 0xb939da5eb6946f8cul },
  { 0x3ff7e2f336cf4e62ul, 0x3c705d02ba15797eul, 0x38f70a1427f8fcdful },
  { 0x3ff7f3878491c491ul, 0xbc807f11cf9311aeul, 0x39219f0b3685b7fful },
  { 0x3ff80427543e1a12ul, 0xbc927c86626d972bul, 0x392d4e0d71c9b16eul },
  { 0x3ff814d2add106d9ul, 0x3c9464370d151d4dul, 0x393c694d6561d277ul },
  { 0x3ff82589994cce13ul, 0xbc9d4c1dd41532d8ul, 0x38f0f6ad65cbbac1ul },
  { 0x3ff8364c1eb941f7ul, 0x3c999b9a31df2bd5ul, 0x392e5100ab05208bul },
  { 0x3ff8471a4623c7adul, 0xbc88d684a341cdfbul, 0xb92591e15c16efd1ul },
  { 0x3ff857f4179f5b21ul, 0xbc5ba748f8b216d0ul, 0x38fa58e6e72eee90ul },
  { 0x3ff868d99b4492edul, 0xbc9fc6f89bd4f6baul, 0xb92f16f65181d921ul },
  { 0x3ff879cad931a436ul, 0x3c85d2d7d2db47bdul, 0xb9179679c19ea91ful },
  { 0x3ff88ac7d98a6699ul, 0x3c9994c2f37cb53aul, 0x393d61283ef385deul },
  { 0x3ff89bd0a478580ful, 0x3c9d53954475202bul, 0xb93ffd8e923800f4ul },
  { 0x3ff8ace5422aa0dbul, 0x3c96e9f156864b27ul, 0xb9130644a7836333ul },
  { 0x3ff8be05bad61778ul, 0x3c9ecb5efc43446eul, 0x393e4ef1b4f47e60ul },
  { 0x3ff8cf3216b5448cul, 0xbc70d55e32e9e3aaul, 0xb903dab3db839dd6ul },
  { 0x3ff8e06a5e0866d9ul, 0xbc97114a6fc9b2e6ul, 0x3931d162ae347ca3ul },
  { 0x3ff8f1ae99157736ul, 0x3c85cc13a2e3976cul, 0x38d3bf26d2b85163ul },
  { 0x3ff902fed0282c8aul, 0x3c9592ca85fe3fd2ul, 0x3939aaeca60a407aul },
  { 0x3ff9145b0b91ffc6ul, 0xbc9dd6792e582524ul, 0x392c03855204534aul },
  { 0x3ff925c353aa2fe2ul, 0xbc83455fa639db7ful, 0xb92a049aab220b43ul },
  { 0x3ff93737b0cdc5e5ul, 0xbc675fc781b57ebcul, 0x390697e257ac0db2ul },
  { 0x3ff948b82b5f98e5ul, 0xbc8dc3d6797d2d99ul, 0xb924a682ed507e0bul },
  { 0x3ff95a44cbc8520ful, 0xbc764b7c96a5f039ul, 0xb8e07053c9a98bbbul },
  { 0x3ff96bdd9a7670b3ul, 0xbc5ba5967f19c896ul, 0x38fc4833e2a01129ul },
  { 0x3ff97d829fde4e50ul, 0xbc9d185b7c1b85d1ul, 0x3937edb9d7144b6ful },
  { 0x3ff98f33e47a22a2ul, 0x3c7cabdaa24c78edul, 0xb91f2ec2c877c312ul },
  { 0x3ff9a0f170ca07baul, 0xbc9173bd91cee632ul, 0xb91053987854965ful },
  { 0x3ff9b2bb4d53fe0dul, 0xbc9dd84e4df6d518ul, 0x391f67e4fe184b31ul },
  { 0x3ff9c49182a3f090ul, 0x3c7c7c46b071f2beul, 0x3916376b7943085cul },
  { 0x3ff9d674194bb8d5ul, 0xbc9516bea3dd8233ul, 0xb91519baeb91c698ul },
  { 0x3ff9e86319e32323ul, 0x3c7824ca78e64c6eul, 0x38b0f92c082bbae0ul },
  { 0x3ff9fa5e8d07f29eul, 0xbc84a9ceaaf1faceul, 0x3924f0e6fc88785dul },
  { 0x3ffa0c667b5de565ul, 0xbc9359495d1cd533ul, 0x392354084551b4fbul },
  { 0x3ffa1e7aed8eb8bbul, 0x3c9c6618ee8be70eul, 0x393b7f2fb72d78c0ul },
  { 0x3ffa309bec4a2d33ul, 0x3c96305c7ddc36abul, 0x393547fa22c26d17ul },
  { 0x3ffa42c980460ad8ul, 0xbc9aa780589fb120ul, 0xb93d6e9e41d89183ul },
  { 0x3ffa5503b23e255dul, 0xbc9d2f6edb8d41e1ul, 0xb90bfd7adfd63f48ul },
  { 0x3ffa674a8af46052ul, 0x3c650f5630670366ul, 0x38fb8696ee520475ul },
  { 0x3ffa799e1330b358ul, 0x3c9bcb7ecac563c7ul, 0xb93678693176f751ul },
  { 0x3ffa8bfe53c12e59ul, 0xbc94f867b2ba15a9ul, 0x39301f0d566ba176ul },
  { 0x3ffa9e6b5579fdbful, 0x3c90fac90ef7fd31ul, 0x3928b16ae39e8cb9ul },
  { 0x3ffab0e521356ebaul, 0x3c889c31dae94545ul, 0xb91af43b90f0d971ul },
  { 0x3ffac36bbfd3f37aul, 0xbc8f9234cae76cd0ul, 0xb90c60dbfc7696f8ul },
  { 0x3ffad5ff3a3c2774ul, 0x3c97ef3bb6b1b8e5ul, 0xb9331a55d12f2b84ul },
  { 0x3ffae89f995ad3adul, 0x3c97a1cd345dcc81ul, 0x393a7fbc3ae675eaul },
  { 0x3ffafb4ce622f2fful, 0xbc94b2fc0f315ecdul, 0x393252d2a6932f30ul },
  { 0x3ffb0e07298db666ul, 0xbc9bdef54c80e425ul, 0x39241cbb95c55600ul },
  { 0x3ffb20ce6c9a8952ul, 0x3c94dd024a0756ccul, 0xb93883daf6928c9eul },
  { 0x3ffb33a2b84f15fbul, 0xbc62805e3084d708ul, 0x3902babc0edda4d9ul },
  { 0x3ffb468415b749b1ul, 0xbc7f763de9df7c90ul, 0xb903e5401cf3f56ful },
  { 0x3ffb59728de5593aul, 0xbc9c71dfbbba6de3ul, 0xb90c7470081df7dful },
  { 0x3ffb6c6e29f1c52aul, 0x3c92a8f352883f6eul, 0x3927a1ee98a99862ul },
  { 0x3ffb7f76f2fb5e47ul, 0xbc75584f7e54ac3bul, 0x390aa64481e1ab72ul },
  { 0x3ffb928cf22749e4ul, 0xbc9b721654cb65c6ul, 0x3925111ed9312467ul },
  { 0x3ffba5b030a1064aul, 0xbc9efcd30e54292eul, 0xb8ead1bf91503c67ul },
  { 0x3ffbb8e0b79a6f1ful, 0xbc3f52d1c9696205ul, 0xb8c1b499b8052088ul },
  { 0x3ffbcc1e904bc1d2ul, 0x3c823dd07a2d9e84ul, 0x3929a164050e1258ul },
  { 0x3ffbdf69c3f3a207ul, 0xbc3c262360ea5b52ul, 0xb8db2ab8c26584fful },
  { 0x3ffbf2c25bd71e09ul, 0xbc9efdca3f6b9c73ul, 0x39127e81cecd59daul },
  { 0x3ffc06286141b33dul, 0xbc8d8a5aa1fbca34ul, 0xb916fd5d0fdf4695ul },
  { 0x3ffc199bdd85529cul, 0x3c811065895048ddul, 0x39199e51125928daul },
  { 0x3ffc2d1cd9fa652cul, 0xbc96e51617c8a5d7ul, 0xb915af0e37eae5deul },
  { 0x3ffc40ab5fffd07aul, 0x3c9b4537e083c60aul, 0x3924a6cdfa70f4f8ul },
  { 0x3ffc544778fafb22ul, 0x3c912f072493b5aful, 0x3927634e44f583acul },
  { 0x3ffc67f12e57d14bul, 0x3c92884dff483cadul, 0xb92fc44c329d5cb2ul },
  { 0x3ffc7ba88988c933ul, 0xbc8e76bbbe255559ul, 0xb91239845875b500ul },
  { 0x3ffc8f6d9406e7b5ul, 0x3c71acbc48805c44ul, 0x3906edaac100b8faul },
  { 0x3ffca3405751c4dbul, 0xbc87f2bed10d08f5ul, 0x38d45233cc94585aul },
  { 0x3ffcb720dcef9069ul, 0x3c7503cbd1e949dbul, 0x391d8765566b032eul },
  { 0x3ffccb0f2e6d1675ul, 0xbc7d220f86009093ul, 0x391e0424e773b3b3ul },
  { 0x3ffcdf0b555dc3faul, 0xbc8dd83b53829d72ul, 0xb8faea073a742049ul },
  { 0x3ffcf3155b5bab74ul, 0xbc9a08e9b86dff57ul, 0xb91743fe56ba6df7ul },
  { 0x3ffd072d4a07897cul, 0xbc9cbc3743797a9cul, 0xb93e7044039da0f6ul },
  { 0x3ffd1b532b08c968ul, 0x3c955636219a36eeul, 0xb9396ce6c611cd73ul },
  { 0x3ffd2f87080d89f2ul, 0xbc9d487b719d8578ul, 0x3902da62b2a9fae7ul },
  { 0x3ffd43c8eacaa1d6ul, 0x3c93db53bf5a1614ul, 0x393420bd107a56f7ul },
  { 0x3ffd5818dcfba487ul, 0x3c82ed02d75b3707ul, 0xb90ab053b05531fcul },
  { 0x3ffd6c76e862e6d3ul, 0x3c5fe87a4a8165a0ul, 0x38db1701f59c75fful },
  { 0x3ffd80e316c98398ul, 0xbc911ec18beddfe8ul, 0xb91ed04e7ac8765aul },
  { 0x3ffd955d71ff6075ul, 0x3c9a052dbb9af6beul, 0x39330dc526492014ul },
  { 0x3ffda9e603db3285ul, 0x3c9c2300696db532ul, 0x3937f6246f0ec615ul },
  { 0x3ffdbe7cd63a8315ul, 0xbc9b76f1926b8be4ul, 0xb930838f11e6612dul },
  { 0x3ffdd321f301b460ul, 0x3c92da5778f018c3ul, 0xb93c6cdead661cf3ul },
  { 0x3ffde7d5641c0658ul, 0xbc9ca5528e79ba8ful, 0x3924f9fd822b5ee1ul },
  { 0x3ffdfc97337b9b5ful, 0xbc91a5cd4f184b5cul, 0x393b7225a944efd6ul },
  { 0x3ffe11676b197d17ul, 0xbc72b529bd5c7f44ul, 0x391386309ca5072aul },
  { 0x3ffe264614f5a129ul, 0xbc97b627817a1496ul, 0xb93b9818808c409aul },
  { 0x3ffe3b333b16ee12ul, 0xbc99f4a431fdc68bul, 0x3938b86d919ec784ul },
  { 0x3ffe502ee78b3ff6ul, 0x3c839e8980a9cc8ful, 0x3921e92cb3c2d278ul },
  { 0x3ffe653924676d76ul, 0xbc863ff87522b735ul, 0x3911bdfc8db5a718ul },
  { 0x3ffe7a51fbc74c83ul, 0x3c92d522ca0c8de2ul, 0xb938a757b0b6a9cbul },
  { 0x3ffe8f7977cdb740ul, 0xbc91089480b054b1ul, 0x392306ae5803b7cbul },
  { 0x3ffea4afa2a490daul, 0xbc9e9c23179c2893ul, 0xb92fc0f242bbf3deul },
  { 0x3ffeb9f4867cca6eul, 0x3c94832f2293e4f2ul, 0xb9390fc40251cbe8ul },
  { 0x3ffecf482d8e67f1ul, 0xbc9c93f3b411ad8cul, 0xb930b9dfef44b43bul },
  { 0x3ffee4aaa2188510ul, 0x3c91c68da487568dul, 0x39131c8db5077e24ul },
  { 0x3ffefa1bee615a27ul, 0x3c9dc7f486a4b6b0ul, 0x393f6dd5d229ff69ul },
  { 0x3fff0f9c1cb6412aul, 0xbc93220065181d45ul, 0xb933f8114293b05bul },
  { 0x3fff252b376bba97ul, 0x3c93a1a5bf0d8e43ul, 0x3934c6ad5476b516ul },
  { 0x3fff3ac948dd7274ul, 0xbc795a5a3ed837deul, 0x38f40a183fe4cc10ul },
  { 0x3fff50765b6e4540ul, 0x3c99d3e12dd8a18bul, 0xb914019bffc80ef3ul },
  { 0x3fff6632798844f8ul, 0x3c9fa37b3539343eul, 0x392726a45a4c9e13ul },
  { 0x3fff7bfdad9cbe14ul, 0xbc9dbb12d006350aul, 0x3935c5ce7280fa4dul },
  { 0x3fff91d802243c89ul, 0xbc612ea8a779f689ul, 0xb9089324bfc3ef57ul },
  { 0x3fffa7c1819e90d8ul, 0x3c874853f3a5931eul, 0x38fdc060c36f7651ul },
  { 0x3fffbdba3692d514ul, 0xbc79677315098eb6ul, 0x391b0cd4d28a9a32ul },
  { 0x3fffd3c22b8f71f1ul, 0x3c62eb74966579e7ul, 0x3902f096934ec56cul },
  { 0x3fffe9d96b2a23d9ul, 0x3c74a6037442fde3ul, 0x38fbaf85e8130af3ul },
};
GPGA_CONST gpga_double pow_log2_130_p_coeff_1h = 0x3ff71547652b82feul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_1m = 0x3c7777d0ffda0d24ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_1l = 0xb9160bb8ae96fdf8ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_2h = 0xbfe71547652b82feul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_2m = 0xbc6777d0ffda0d24ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_2l = 0x390633e6775d9370ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_3h = 0x3fdec709dc3a03fdul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_3m = 0x3c7d27f05548af0cul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_4h = 0xbfd71547652b82feul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_4m = 0xbc5777d0ffda7848ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_5h = 0x3fd2776c50ef9bfeul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_5m = 0x3c7e4b29ccc0d7d2ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_6h = 0xbfcec709dc3a03fdul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_6m = 0xbc6d27eb0faef882ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_7h = 0x3fca61762a7aded9ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_7m = 0x3c5fb33145fd23f0ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_8h = 0xbfc71547652b82fful;
GPGA_CONST gpga_double pow_log2_130_p_coeff_9h = 0x3fc484b13d7c029bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_10h = 0xbfc2776c50eaac66ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_11h = 0x3fc0c9a849c0ac55ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_12h = 0xbfbec723d3939d50ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_13h = 0x3fbc6890f2f925e8ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_14h = 0xbfb0585d5bb0cf40ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_15h = 0x3fad100aa60f67b9ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_16h = 0xbfa9d6e1da60e83cul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_17h = 0x3fa6b34a3b1e3e85ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_18h = 0xbfa3a4b8397e1a86ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_19h = 0x3fa0a0d2f2a833ddul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_20h = 0xbf9c6bf526d6d5f9ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_21h = 0x3f98510bb10cbe54ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_22h = 0xbf9453c7c4bd6a46ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_23h = 0x3f906d1a4b98f005ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_24h = 0xbf8c9a5e98a72c2bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_25h = 0x3f88d7ee3a3b5da1ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_26h = 0xbf8522a95d06647aul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_27h = 0x3f816839a0db2f7aul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_28h = 0xbf7b94eb1d2b7a40ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_29h = 0x3f764dba02a54f4bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_30h = 0xbf7122764e0e42a4ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_31h = 0x3f6c0f5205fe68c3ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_32h = 0xbf6717793a3f40f0ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_33h = 0x3f6237faad8e9ef4ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_34h = 0xbf5d6c5f6b3df0fdul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_35h = 0x3f58b0ddf3f78450ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_36h = 0xbf53ff3cb8cc9cd5ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_37h = 0x3f4f4d2dd5c8c9a3ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_38h = 0xbf4a9f4d93c9f064ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_39h = 0x3f45f22c38a1fa60ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_40h = 0xbf41446f02e9cfaful;
GPGA_CONST gpga_double pow_log2_130_p_coeff_41h = 0x3f3c955d6f4c8da0ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_42h = 0xbf37e41fb57f90a2ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_43h = 0x3f332ffa4b5b4d0cul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_44h = 0xbf2e793d7f62f022ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_45h = 0x3f29bfb9951e1e46ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_46h = 0xbf2504ac81de1e13ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_47h = 0x3f208c6f11f0d4f0ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_48h = 0xbf1c1c5892310d58ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_49h = 0x3f17b4c1f7887b4bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_50h = 0xbf1344b16f0d0931ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_51h = 0x3f0ee7fe70c48037ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_52h = 0xbf0a8631d8715f76ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_53h = 0x3f063c71cbb9f9c2ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_54h = 0xbf01f8d3a9e9a3a2ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_55h = 0x3efd4b8f2786b326ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_56h = 0xbef8a0a0f5c0f43bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_57h = 0x3ef401538e84a6a5ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_58h = 0xbeef5e1efb51f8d9ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_59h = 0x3eeacdd9b9e52218ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_60h = 0xbee64a57876f1317ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_61h = 0x3ee1d8f30af8f5c6ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_62h = 0xbedcf53f809fa66aul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_63h = 0x3ed88d28e5653c26ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_64h = 0xbed3b7f9b6c70e5dul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_65h = 0x3ecf3afc3f3a1ed8ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_66h = 0xbec99e02ad1b0bb6ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_67h = 0x3ec34fbb380431a5ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_68h = 0xbebed730d7a60e76ul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_69h = 0x3eb84c8d3c06aa0bul;
GPGA_CONST gpga_double pow_log2_130_p_coeff_70h = 0xbeb4b23f5c57d61aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_0h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_1h = 0x3fe62e42fefa39eful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_1m = 0x3c7abc9e3b39805cul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_2h = 0x3fcebfbdff82c58ful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_2m = 0xbc65e43a53e0e551ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_3h = 0x3fac6b08d704a0c0ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_3m = 0xbc4d33876e373274ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_4h = 0x3f83b2ab6fba4e77ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_5h = 0x3f55d87fe7916e08ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_6h = 0x3f243091a3d9b3eeul;
GPGA_CONST gpga_double pow_exp2_1024_1h = 0x3ff0000000000000ul;
GPGA_CONST gpga_double pow_exp2_1024_2h = 0x3f8b90b8fc2bccb5ul;
GPGA_CONST gpga_double pow_exp2_1024_2m = 0xbd8422c70a44c6ccul;
GPGA_CONST gpga_double pow_exp2_1024_3h = 0x3f2bc8e1740bc741ul;
GPGA_CONST gpga_double pow_exp2_1024_4h = 0x3ed48c6af5b63199ul;
GPGA_CONST gpga_double pow_exp2_1024_5h = 0x3e7b1039f94ed6e4ul;
GPGA_CONST gpga_double pow_exp2_1024_6h = 0x3e21e9ab695aeb2ful;
GPGA_CONST gpga_double pow_exp2_1024_7h = 0x3dc7847e5a3c81ceul;
GPGA_CONST gpga_double pow_exp2_1024_8h = 0x3d6d6f7fba1a2279ul;
GPGA_CONST gpga_double pow_exp2_1024_9h = 0x3d13774b44730a33ul;
GPGA_CONST gpga_double pow_exp2_1024_10h = 0x3cb93b8d3ce97d2ful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_1 = 0x3ff0000000000000ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_2 = 0x3fcebfbdfd9f200cul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_3 = 0x3fac6b08d704a0c0ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_4 = 0x3f83b2ab6fb9f15aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_5 = 0x3f55d87ec84a5701ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_6 = 0x3f22b90229c1f1d1ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_7 = 0x3eebfbdff82c58eaul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_8 = 0x3eb0c6b0a7b8750aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_9 = 0x3e6ee2abef621928ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_10 = 0x3e2aa533b6d64247ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_11 = 0x3de0a9e38988a46eul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_12 = 0x3d98b2d2e55bd147ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_13 = 0x3d3c84233a5a5a92ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_14 = 0x3cdc5edb446292dcul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_15 = 0x3c7bc547cc60f001ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_16 = 0x3c17849d40b55485ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_17 = 0x3baf71db744a755bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_18 = 0x3b43b8c8c8c3bffbul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_19 = 0x3ad1c7d66b36f3feul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_20 = 0x3a5f6b7d5058f6f3ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_21 = 0x39e3b3cd51f01f15ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_22 = 0x3964a582046a7f71ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_23 = 0x38e1fd5ce2b6c8f1ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_24 = 0x385b9a1de7d1f1ddul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_25 = 0x37cf54e6b03c0e95ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_26 = 0x3742104ca3cb7231ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_27 = 0x36b1f5a5f9d8d28aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_28 = 0x361d08b34e4f7c7cul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_29 = 0x35840f73c9f737e9ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_30 = 0x34e7c4f12e9299d2ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_31 = 0x3447ad74f6ecb9d9ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_32 = 0x33a3cad659e57107ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_33 = 0x32fc8f6db0a8c094ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_34 = 0x3252398e664fb423ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_35 = 0x31a4f76003169acbul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_36 = 0x30f527b479e4bd6bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_37 = 0x304333f2a10e8622ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_38 = 0x2f8f46efab3f1581ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_39 = 0x2ed97a55be0966d0ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_40 = 0x2e21e81fd8818b1aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_41 = 0x2d68a3b7f74192eaul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_42 = 0x2cadb9f8e4d31806ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_43 = 0x2bf138136d4d89b3ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_44 = 0x2b3311a91f4db788ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_45 = 0x2a737d3b7d86ab41ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_46 = 0x29b2a5a7dd7f2e53ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_47 = 0x28f0c711eec7e37aul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_48 = 0x282ddca022c8d1ebul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_49 = 0x2769f6d2128602f2ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_50 = 0x26a51b19f9846e3bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_51 = 0x25df507995bb25caul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_52 = 0x2518a93e07ee9c91ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_53 = 0x24513291f0a04ce1ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_54 = 0x238901f6d80f494dul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_55 = 0x22c0132b46c5748dul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_56 = 0x21f69d01a5d31cccul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_57 = 0x212cdc1e4a3fa2b3ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_58 = 0x2063064c6c1872f2ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_59 = 0x1f98e363dd356a13ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_60 = 0x1ece88090bf456f8ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_61 = 0x1e03ff62f7876f33ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_62 = 0x1d394b33c6b6b0fcul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_63 = 0x1c6e7ad1c70f8daaul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_64 = 0x1ba393d2e2c1d662ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_65 = 0x1ad8a2cc329e9c9bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_66 = 0x1a0db307ee46d7a5ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_67 = 0x1942ce8a1f8471ecul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_68 = 0x1877ff43fef58a08ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_69 = 0x17ad4ef7b67ad811ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_70 = 0x16e2c6b101a47c4dul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_71 = 0x16186ff23fe7fa7ful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_72 = 0x154e4c88f2f9d79bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_73 = 0x14845c7844a990c8ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_74 = 0x13ba9f7a5e2c0fe4ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_75 = 0x12f1105f9be40947ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_76 = 0x1227b53f38438599ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_77 = 0x115e978fe3a6f2b2ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_78 = 0x1095bc1018c40480ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_79 = 0x0fccd8f37cb62fe9ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_80 = 0x0f344a9bd5ccb280ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_81 = 0x0e9c1f57c9345b2ful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_82 = 0x0e044dfda34b9010ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_83 = 0x0d6cdc6b4f990edbul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_84 = 0x0cd5c06d299e8316ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_85 = 0x0c3ef7d05f7ce724ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_86 = 0x0ba87d6a6f997b4bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_87 = 0x0b123e112aa1c3c5ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_88 = 0x0a7c8a9a80f9fd06ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_89 = 0x09e7dbdc414bdad4ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_90 = 0x095a070ae9d43c5ful;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_91 = 0x08cd8f2d4a5e74a9ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_92 = 0x0841672e5847e4e7ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_93 = 0x07b590e64b6e3c20ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_94 = 0x072a091b2b6ac2fb0ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_95 = 0x069ecbb2a286d1f6ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_96 = 0x0613d47560f19f4bul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_97 = 0x05891f52a529ddc9ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_98 = 0x04feb951a1ce09bdul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_99 = 0x04749f0f183a4a21ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_100 = 0x03eac630f13b27a7ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_101 = 0x0361385d4d75c4e4ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_102 = 0x02d7f010f1ed4ad3ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_103 = 0x024ef7a09f4142a0ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_104 = 0x01c64c8b13aaea24ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_105 = 0x013def5d10d44d31ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_106 = 0x00b5df3ad6e6f6d5ul;
GPGA_CONST gpga_double pow_exp2_120_p_coeff_107 = 0x002e1c7f062a262aul;

GPGA_CONST GpgaPowArgRed pow_argredtable[256] = {
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3ff0163da9fb3336ul, 0x3c8f7c62022f7ad8ul, 0x3c7ad8c8bd0e1b90ul, 0xb93444f7b4a9a1feul },
  { 0x3ff02c9a3e778061ul, 0x3c8fe8a4db982cbcul, 0x3c7d4c7e5f7e2cf4ul, 0x3966daeb95c60f0aul },
  { 0x3ff04315e86e7f85ul, 0x3c8f4f155d58e6b1ul, 0xbc7c0b53d9f1e771ul, 0xb93fb0e2b53a3b07ul },
  { 0x3ff059b0d3158574ul, 0x3c8f0f217f7a3b88ul, 0x3c7b7d8a7d10a302ul, 0x39478f6c70d4ac89ul },
  { 0x3ff0706b29ddf6deul, 0x3c8f60e8ed1b3b04ul, 0xbc7b2e09e7c5a1d7ul, 0x394f2f73b7412b0cul },
  { 0x3ff0874518759bc8ul, 0x3c8f0b25f4b26a5aul, 0xbc7b7026a6fbe6e2ul, 0x396e24a73a1958a2ul },
  { 0x3ff09e3ecac6f383ul, 0x3c8ec5f46f8e2d51ul, 0x3c7b6c7082b4f89bul, 0xb96e5d55b4cfd0e6ul },
  { 0x3ff0b5586cf9890ful, 0x3c8e9079dc6aa01dul, 0x3c7b9ef2188fcde6ul, 0x3977c7f044d62c7aul },
  { 0x3ff0cc922b7247f7ul, 0x3c8f8a6e5fe9106cul, 0xbc7bca8522ce0d54ul, 0xb96e4f6e269dd38aul },
  { 0x3ff0e3ec32d3d1a2ul, 0x3c8f1ba3be7c74c7ul, 0x3c7b7e87a71de7fful, 0x3974bc1aa9a4af44ul },
  { 0x3ff0fb66b6c0b3c7ul, 0x3c8eece64b1bc2a3ul, 0xbc7bb1a0bb0ed7b8ul, 0xb9633acb17b19c50ul },
  { 0x3ff11301d0125b51ul, 0x3c8e98d32a6c3963ul, 0x3c7b0f6c4e97c3f1ul, 0x3962e1c3a8f0cdb1ul },
  { 0x3ff12abdc06c31ccul, 0x3c8f4d278e0ba4a7ul, 0x3c7b5a69a0cbe74aul, 0x39657a6c940f7b41ul },
  { 0x3ff1429aaea92de0ul, 0x3c8f06a64f2b79f6ul, 0xbc7b63e4f64d4636ul, 0xb970b3e7bce0f720ul },
  { 0x3ff15a98c8a58e52ul, 0x3c8ef84d1a1e6761ul, 0xbc7b373eb0a5e431ul, 0x397b51197f47d991ul },
  { 0x3ff172b83c7d517aul, 0x3c8dcbccb557c7e0ul, 0x3c7af08a7e1a81d8ul, 0xb95d24e377ef54f0ul },
  { 0x3ff18af9388c8de2ul, 0x3c8f6dd0fd4f89a0ul, 0xbc7aa5db2d872c7dul, 0xb963f77a1f879af1ul },
  { 0x3ff1a35beb6fcb75ul, 0x3c8f81ae28bfcf73ul, 0xbc7a6b71b4dc0f51ul, 0x394c17f1a8fef6b9ul },
  { 0x3ff1bbe08b0f28c2ul, 0x3c8f56d82fb9de9eul, 0xbc7a5b1edc4b4f40ul, 0x396ef322f39f0f1dul },
  { 0x3ff1d4873168b9aaul, 0x3c8eebc4c3f3c726ul, 0x3c7a5f5856e90c72ul, 0xb9797dc8a0e2a96bul },
  { 0x3ff1ed5022fcd91dul, 0x3c8f4889b4f4744ful, 0xbc7a3bb4a5b6d2b6ul, 0x395dc5462b0e49feul },
  { 0x3ff2063b88628cd6ul, 0x3c8ef2dd81c65165ul, 0xbc7a278f8a1f2e8bul, 0x3974a754664f0144ul },
  { 0x3ff21f49917ddc96ul, 0x3c8ee6d7125f12ceul, 0x3c7a11957c1bb2a6ul, 0xb9680ff1d6b7f7d9ul },
  { 0x3ff2387a6e756238ul, 0x3c8efbdbb08c84a5ul, 0x3c79d9bdf954e3c4ul, 0xb9648f8d84f7a70cul },
  { 0x3ff251ce4fb2a63ful, 0x3c8e93b8138ca039ul, 0x3c79b8e8b3335e00ul, 0xb956b5bd67d2725ful },
  { 0x3ff26b4565e27cddul, 0x3c8f405e66f51d9bul, 0x3c79a0d9d0f4d97bul, 0xb9759c7659249b9aul },
  { 0x3ff284dfe1f56381ul, 0x3c8ef3f59d2cf534ul, 0x3c7972d0c81d7f80ul, 0x3968d7d1afcd1d32ul },
  { 0x3ff29e9df51fdee1ul, 0x3c8f4f4f5b0a42a5ul, 0x3c7963bf0db50b2ful, 0x3967f0f6a2b3ca91ul },
  { 0x3ff2b87fd0dad990ul, 0x3c8f0c0741f5570dul, 0x3c7940c3ed4881b8ul, 0xb94b6d0f8f424c6aul },
  { 0x3ff2d285a6e4030bul, 0x3c8ef5d0f6f49f0aul, 0x3c7925d3f23480f3ul, 0xb96961fdc0b8de07ul },
  { 0x3ff2ecafa93e2f56ul, 0x3c8ef4dceda55969ul, 0xbc790d7ce6e8411dul, 0x39599e4940ac09d4ul },
  { 0x3ff306fe0a31b715ul, 0x3c8ef20ff9d1480bul, 0xbc78ed23e6b499a1ul, 0xb97a0e8d64a5a48aul },
  { 0x3ff32170fd4e8f51ul, 0x3c8ed97f5f0520dcul, 0xbc78d859b34b3f1ful, 0x39644c6f120ebbedul },
  { 0x3ff33c08b26416fful, 0x3c8efa5a8971cfa4ul, 0x3c78a8cc1d095b85ul, 0x397c7a03bfefbdc2ul },
  { 0x3ff356c55f929ff1ul, 0x3c8f2b6cf45e46b9ul, 0x3c788bcab0dc7d42ul, 0x395ccfcf0a27f3e2ul },
  { 0x3ff371a7373aa9cbul, 0x3c8e96507b6a8821ul, 0x3c7866a9b4ab42d8ul, 0x397d44d253b3c98bul },
  { 0x3ff38cae6d05d866ul, 0x3c8efb2f7c1f77dful, 0xbc78540d2371a299ul, 0xb96ad9c0066c0e55ul },
  { 0x3ff3a7db34e59ff7ul, 0x3c8ee0a02cf7fd80ul, 0xbc7826078792b259ul, 0xb9605c5a740edbd9ul },
  { 0x3ff3c32dc313a8e5ul, 0x3c8e9d3ea3d6f4eeul, 0x3c7813ac2c96268aul, 0x3949fd8d4e227564ul },
  { 0x3ff3dea64c123422ul, 0x3c8ef51a9baf259ful, 0xbc77f63b8aa2ae0bul, 0x3957e1a26ac606f4ul },
  { 0x3ff3fa4504ac801cul, 0x3c8eb69c03f6a8a1ul, 0xbc77d7d8c8b5a60eul, 0x395d2bb1e2c25631ul },
  { 0x3ff4160a21f72e2aul, 0x3c8efb71a5bda6a1ul, 0xbc77b40a6c0e8d64ul, 0x394b35af524b0f1ful },
  { 0x3ff431f5d950a897ul, 0x3c8e887f2c8579a9ul, 0x3c7798727a9299a3ul, 0xb97b83e0c3963f17ul },
  { 0x3ff44e086061892dul, 0x3c8f1c5bb5e7e2f8ul, 0x3c776d2d3947f45cul, 0xb963d1e236c313c0ul },
  { 0x3ff46a41f6c0ee68ul, 0x3c8ec7a2f46b88eful, 0xbc7752d3c4d74b4aul, 0xb97c08d6d3a1c51bul },
  { 0x3ff486a2d5461954ul, 0x3c8ed2f37d49e920ul, 0xbc7744a523b58a76ul, 0x3968fe2a26d0560aul },
  { 0x3ff4a32af0d7d3deul, 0x3c8eae63a2e482c3ul, 0x3c771b0f4c6e6966ul, 0x397fa6c88ab56a3cul },
  { 0x3ff4bfdad5362a27ul, 0x3c8ef1c050dff8d4ul, 0x3c76fbd2bca5d4a9ul, 0x397f703b529b8f6bul },
  { 0x3ff4dcab3b4ee4daul, 0x3c8ecc50b7ea7f15ul, 0xbc76e71578c8d5dful, 0x3970a65a7bd0c99ful },
  { 0x3ff4f9a4b7bb89c4ul, 0x3c8ef8c8e1132018ul, 0x3c76c3b1685fc530ul, 0xb97ef393e1b48700ul },
  { 0x3ff516c79fd1155ful, 0x3c8ed28bb8ea7dc4ul, 0xbc769f9c26ebf05bul, 0xb9660f2591c08619ul },
  { 0x3ff5341476729975ul, 0x3c8ef1d8e7addf82ul, 0x3c7677bff7d2f91cul, 0x397b4f0e3bc9c168ul },
  { 0x3ff5518b7f2e73fcul, 0x3c8e7c3a3c4c2dbcul, 0x3c76536b0bd12084ul, 0x394a4d5a6a17b2d3ul },
  { 0x3ff56f2d1ae33952ul, 0x3c8ef1b41053f1f9ul, 0xbc762c515f167638ul, 0xb94d0a2f1aa9ad25ul },
  { 0x3ff58cf99671c293ul, 0x3c8ef118227e6e39ul, 0x3c761028e9e50f7bul, 0xb965bf51322f86d4ul },
  { 0x3ff5aaef7a42f4d1ul, 0x3c8e8972d1eb7ed6ul, 0x3c75fb6c80f61f1bul, 0xb965d01ae8246841ul },
  { 0x3ff5c910799ed64cul, 0x3c8ef38d3bd16a4ful, 0x3c75d497b3ab9fa9ul, 0xb97de22d946dac6bul },
  { 0x3ff5e75ccbe88b7bul, 0x3c8ec1f37b11ad1aul, 0xbc75c1ed00555fe0ul, 0x397d01549b83d5f0ul },
  { 0x3ff605d4fe6fd5c4ul, 0x3c8ed0b6b45e5461ul, 0x3c75972fe2227eeaul, 0x397902c02425093ful },
  { 0x3ff62479ea0ba70bul, 0x3c8ec2be61b266a3ul, 0x3c7581d0984da0b0ul, 0xb96b6e0c580c94d2ul },
  { 0x3ff6434c1e6b75aful, 0x3c8ed3f65ae198dcul, 0xbc755b1171162fe8ul, 0xb94f3fce28d0fbfful },
  { 0x3ff6624c8c5bcb9aul, 0x3c8ee0f3b5b1e1d9ul, 0x3c7530f4c9a255caul, 0x3972f2841b5ef0c8ul },
  { 0x3ff6817c21c611d7ul, 0x3c8ece0c0d1c22ccul, 0x3c751bf54f1f11d5ul, 0x395b2bcedd624c6cul },
  { 0x3ff6a0dbd3c1ad09ul, 0x3c8e9176b1792aeaul, 0xbc74fa0b588d8cccul, 0x396e0d4dcbaecf08ul },
  { 0x3ff6c06c9f90a278ul, 0x3c8ec18a7a3b5b03ul, 0x3c74d2dd2ec098b5ul, 0xb97cf0c2da54eac7ul },
  { 0x3ff6e02f860f2094ul, 0x3c8e932478c6de1bul, 0xbc74b84f8c3ec9d4ul, 0x3947d1a3c8d1b9dcul },
  { 0x3ff70025a01d9b4aul, 0x3c8e7cc59e4d0d73ul, 0x3c748d4b97b456f0ul, 0xb95fcb10a2a3173cul },
  { 0x3ff7204fda8b1f61ul, 0x3c8e96a9b87a5e49ul, 0x3c746f64c14828a3ul, 0xb96e76be59c5a583ul },
  { 0x3ff740af214536c7ul, 0x3c8ece0f3d0e47b8ul, 0xbc745e3947d89149ul, 0x3952af58f9c303f5ul },
  { 0x3ff76144535e8578ul, 0x3c8e95cb3c435a2cul, 0xbc744163df0b8f62ul, 0xb966c79c87942d2bul },
  { 0x3ff78210726954d0ul, 0x3c8e8e9a0276a43aul, 0x3c741ad55960a81eul, 0x397c77322da490f9ul },
  { 0x3ff7a31366d94e4eul, 0x3c8ed2f41d5a34c3ul, 0x3c74046fcf776e7cul, 0xb96111d7277721c9ul },
  { 0x3ff7c44e2f9b1df7ul, 0x3c8e942154588811ul, 0x3c73da52c2f0ccbdul, 0xb95ad41ea6a6e12bul },
  { 0x3ff7e5c0be3c6a90ul, 0x3c8e8f76c7b0331ful, 0x3c73bccaf5d5a1f9ul, 0x395fbd8f70e8b3bful },
  { 0x3ff8076af5c8d5b5ul, 0x3c8eb47d7b5c1a71ul, 0xbc73a23230b6e4dcul, 0x3977f4a4c3a63b75ul },
  { 0x3ff8294d10d4b071ul, 0x3c8e92ef5b2c20d4ul, 0xbc73798d6fa9d4b5ul, 0xb9543ebc78b47260ul },
  { 0x3ff84b6770c3727cul, 0x3c8edb487169ac2cul, 0x3c735f86db57ee6bul, 0x396a5009f3ac869dul },
  { 0x3ff86dba5d39aa1ful, 0x3c8eb7a61a51a5e3ul, 0xbc734a6b576e3b4eul, 0x39588ad8d8325a4bul },
  { 0x3ff890460c37f8d5ul, 0x3c8eb6bac6a5c112ul, 0xbc73298fcae7e964ul, 0xb960bf6e6aaf7c71ul },
  { 0x3ff8b30abf77c5dful, 0x3c8e96c04cc2004bul, 0xbc730a3327ce4ca7ul, 0xb94a1b9c40f8a8bbul },
  { 0x3ff8d60906dd6a06ul, 0x3c8eb62c0d0f886dul, 0x3c72e8df4c2cf2d8ul, 0xb96f3d799f25637cul },
  { 0x3ff8f94166fa123bul, 0x3c8e96d2ad09c84aul, 0xbc72d23f34a4b7b0ul, 0xb947cd9ea6fe9ca0ul },
  { 0x3ff91cb45660a1fbul, 0x3c8eae81f3a3f1a9ul, 0xbc72b0a86032dcf6ul, 0x394d6d7729d40b1aul },
  { 0x3ff94061aabe0c60ul, 0x3c8e9ea18ee2ed4cul, 0x3c728f8281f6a2f8ul, 0x396f303e9940b139ul },
  { 0x3ff96449d5897c7bul, 0x3c8eae05c18a8c1cul, 0xbc726fe731edbf79ul, 0xb95e7aa20e9cb89aul },
  { 0x3ff9886d9d7acdb2ul, 0x3c8e8c3d5b3922a2ul, 0xbc72604e75411f41ul, 0xb97f78dc1f6b6b77ul },
  { 0x3ff9accdcd8bd41bul, 0x3c8e9d0a8b5a0ec3ul, 0x3c723c3ee4cd2cc3ul, 0x39731f9f21f0eb6cul },
  { 0x3ff9d1698b2c18dcul, 0x3c8e9a8b5901a99cul, 0x3c7221b41cc6ed6aul, 0xb96a0bf81e5899c0ul },
  { 0x3ff9f641b0f0e16cul, 0x3c8e9c47e55b15d7ul, 0xbc72089a9d8212e9ul, 0x39659e3ed2fe5e47ul },
  { 0x3ffa1b563aa60e53ul, 0x3c8e9b9d2c4008d5ul, 0x3c71ed1b4396a6b0ul, 0x396aaf58b2c0f63bul },
  { 0x3ffa40a73dcdd4faul, 0x3c8e9d0f6fcddc3bul, 0xbc71d2263f0b3dbcul, 0xb96df83a816e4a9ful },
  { 0x3ffa6634c7f3d80cul, 0x3c8e9b5e4e5a6b9cul, 0xbc71b79c0845d5f2ul, 0xb96a580dcf911c8eul },
  { 0x3ffa8bfea8d6e9c4ul, 0x3c8e9c2a6f1db457ul, 0xbc719f9f7b05fd99ul, 0xb9670f7cc7f8a7a6ul },
  { 0x3ffab205f1f4b2eeul, 0x3c8e9b2dfdc95f36ul, 0x3c7187191cd60f2bul, 0x395a121fc1ec12b4ul },
  { 0x3ffad84b1f24b36aul, 0x3c8e9b5be8d4eb99ul, 0xbc716f6e97ca3b4cul, 0x397b7a2190a70019ul },
  { 0x3ffafeced95d10d2ul, 0x3c8e9b29b1a13ed2ul, 0x3c7155ffbd1e4f62ul, 0x397cf5dd20e97729ul },
  { 0x3ffb2593c2047f86ul, 0x3c8e9b90d0b9d1f2ul, 0xbc713ddf9fb4c65bul, 0xb95f55c6d8cda160ul },
  { 0x3ffb4c985f05f5b0ul, 0x3c8e9b1785b1c705ul, 0x3c71255f86f46f0bul, 0x397d4b67e5a1a8b1ul },
  { 0x3ffb73eecf64d7dcul, 0x3c8e9b4e4b8b3f61ul, 0x3c710c5f840c8e41ul, 0x397d2fd0e8940b8cul },
  { 0x3ffb9b7fca8d5c80ul, 0x3c8e9b0bb070a76cul, 0x3c70f3838b63f13aul, 0x3970b60c86c7c83bul },
  { 0x3ffbc353ce621218ul, 0x3c8e9b35095a38d4ul, 0x3c70dadf7082b947ul, 0xb97bcd5b60b0d84ful },
  { 0x3ffbeb6b6f6f0b6cul, 0x3c8e9b2b33c85e83ul, 0xbc70c1cf12a3b8f2ul, 0xb954f0b0b4a60290ul },
  { 0x3ffc13c742a7ad64ul, 0x3c8e9b2d9e33aa9bul, 0x3c70a7a1cfa2d3c6ul, 0xb96a23a0f2dcb32aul },
  { 0x3ffc3c67b7d4d05bul, 0x3c8e9b1f0e784f68ul, 0xbc708d7d3e3b6b5bul, 0xb97e1478c41cc6fcul },
  { 0x3ffc655d6f6f9b24ul, 0x3c8e9b30620f10abul, 0xbc70743aa9b8b7a9ul, 0xb964e6d9b7355a41ul },
  { 0x3ffc8e9925d83e13ul, 0x3c8e9b2186fd73a1ul, 0xbc705b6e9efc0b2cul, 0xb953d18013fca4b2ul },
  { 0x3ffcb81b9aab2092ul, 0x3c8e9b2602a75c31ul, 0x3c7042d6a2b2f051ul, 0x397b8e6093208a70ul },
  { 0x3ffce1e58f21c55aul, 0x3c8e9b1fd962f1f9ul, 0x3c702a2ebbc007e0ul, 0x396a0313a23c32dbul },
  { 0x3ffd0bf7e6ed0e07ul, 0x3c8e9b20bfe2c1a1ul, 0x3c7011b8274cf8b5ul, 0x397c96b0e68cf498ul },
  { 0x3ffd3653573bb4a4ul, 0x3c8e9b1f38d26ea4ul, 0x3c6ffe48e4b4b827ul, 0x397c83028df0eb67ul },
  { 0x3ffd61087c5e0a47ul, 0x3c8e9b1f4f1a2733ul, 0x3c6fd4b2c0f28fdbul, 0x39718df6b915c908ul },
  { 0x3ffd8c0872ef6c2bul, 0x3c8e9b1e67f76f5cul, 0x3c6fabe2e1457a2bul, 0x3977e463a486ab0cul },
  { 0x3ffdb75441d989c5ul, 0x3c8e9b1e80f38cdeul, 0x3c6f8425f3626949ul, 0x397a6f8a83a57f73ul },
  { 0x3ffde2ed2607b2c1ul, 0x3c8e9b1e3ee1c4c9ul, 0xbc6f5d6d74f4b9f0ul, 0xb96b8b10f35aa146ul },
  { 0x3ffe0ed47f8071f8ul, 0x3c8e9b1e28b50c8aul, 0xbc6f36f6572240fcul, 0xb975fc8a6f25d75cul },
  { 0x3ffe3b0a9e63b6f3ul, 0x3c8e9b1e2a82c771ul, 0x3c6f114433d420eeul, 0x397c23b0e4a12c14ul },
  { 0x3ffe678fcb9f1eaful, 0x3c8e9b1e24a48b4bul, 0xbc6eeb40e75d3cf6ul, 0xb97b2fb8bd998e08ul },
  { 0x3ffe94646d968f34ul, 0x3c8e9b1e27cd56a6ul, 0x3c6ec5a1537e77d1ul, 0x3952d0d5a275be20ul },
  { 0x3ffec189ea4866d3ul, 0x3c8e9b1e2a6cfb24ul, 0x3c6ea0d7a2aa8624ul, 0x396b212703b240c2ul },
  { 0x3ffeeeffb8e9b4d9ul, 0x3c8e9b1e27a7f19ful, 0xbc6e7a99494167e0ul, 0x3974f615683c0998ul },
  { 0x3fff1cc652e1e0c0ul, 0x3c8e9b1e293f2f7eul, 0x3c6e5659b0e7a1f8ul, 0xb96d9fd04f8ac9e0ul },
  { 0x3fff4ade880dfe85ul, 0x3c8e9b1e27c6ea99ul, 0xbc6e3288f5b6585cul, 0xb96bde3a2b3abf4bul },
  { 0x3fff79497e74f4b0ul, 0x3c8e9b1e27ebf1c1ul, 0xbc6e0ef99a53a4d8ul, 0xb9714df47ca9ad9bul },
  { 0x3fffa8063c7f7ef4ul, 0x3c8e9b1e27d8c352ul, 0xbc6deb7a75e1ec72ul, 0x397d3581a73c7f1cul },
  { 0x3fffd71567af34f5ul, 0x3c8e9b1e27e1f2b6ul, 0x3c6dc77d5d1d870dul, 0x397949166b2bd5a1ul }
};

inline bool gpga_pow_is_odd_integer(gpga_double y) {
  gpga_double ay = gpga_double_abs(y);
  gpga_double t = gpga_double_add(ay, pow_two53);
  gpga_double diff = gpga_double_sub(gpga_double_sub(t, pow_two53), ay);
  return gpga_double_eq(gpga_double_abs(diff), gpga_double_const_one());
}

inline bool gpga_pow_is_integer(gpga_double y) {
  gpga_double ay = gpga_double_abs(y);
  if (gpga_double_ge(ay, pow_two52)) {
    return true;
  }
  gpga_double t = gpga_double_add(ay, pow_two52);
  gpga_double diff = gpga_double_sub(t, pow_two52);
  return gpga_double_eq(diff, ay);
}

inline void gpga_pow_decompose(gpga_double x, thread gpga_double* val,
                               thread gpga_double* expo) {
  int expo_x = gpga_double_exponent(x);
  *expo = gpga_double_from_s64(expo_x);
  if (expo_x <= -1) {
    gpga_double adjusted =
        gpga_double_ldexp(x, 52);
    int expo_adj = gpga_double_exponent(adjusted);
    *expo = gpga_double_from_s64(expo_adj - 52);
    *val = gpga_u64_from_words(0x3ff00000u, gpga_u64_lo(adjusted));
  } else {
    *val = gpga_u64_from_words(0x3ff00000u, gpga_u64_lo(x));
  }
}

inline void gpga_pow_decompose_odd(gpga_double x, thread gpga_double* resm,
                                   thread int* resE) {
  uint exp_bits = gpga_double_exp(x);
  ulong mant = gpga_double_mantissa(x);
  int exp = 0;
  if (exp_bits == 0u) {
    if (mant == 0ul) {
      *resm = gpga_double_zero(0u);
      *resE = 0;
      return;
    }
    exp = -1022;
    while ((mant & (1ul << 52)) == 0ul) {
      mant <<= 1;
      exp -= 1;
    }
  } else {
    exp = (int)exp_bits - 1023;
    mant |= (1ul << 52);
  }
  int shift = 0;
  while ((mant & 1ul) == 0ul) {
    mant >>= 1;
    shift += 1;
  }
  *resE = exp - 52 + shift;
  *resm = gpga_double_from_u64(mant);
}

inline void gpga_pow_log2_130_simple(thread gpga_double* log2h,
                                     thread gpga_double* log2m,
                                     thread gpga_double* log2l,
                                     gpga_double x) {
  gpga_double ah = gpga_double_zero(0u);
  gpga_double al = gpga_double_zero(0u);
  Add12Cond(&ah, &al, x, gpga_double_const_minus_one());
  gpga_double t = gpga_double_div(al, gpga_double_add(ah, gpga_double_const_two()));
  gpga_double u = gpga_double_mul(t, t);
  gpga_double p = pow_log2_130_p_coeff_70h;
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_69h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_68h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_67h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_66h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_65h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_64h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_63h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_62h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_61h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_60h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_59h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_58h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_57h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_56h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_55h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_54h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_53h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_52h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_51h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_50h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_49h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_48h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_47h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_46h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_45h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_44h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_43h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_42h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_41h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_40h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_39h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_38h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_37h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_36h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_35h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_34h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_33h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_32h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_31h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_30h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_29h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_28h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_27h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_26h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_25h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_24h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_23h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_22h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_21h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_20h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_19h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_18h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_17h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_16h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_15h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_14h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_13h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_12h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_11h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_10h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_9h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_8h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_7h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_6h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_5h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_4h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_3h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_2h);
  p = gpga_double_fma(p, u, pow_log2_130_p_coeff_1h);
  gpga_double t2h = gpga_double_mul(u, t);
  gpga_double t2m = gpga_double_zero(0u);
  Add12Cond(&t2h, &t2m, t2h, gpga_double_mul(t2h, p));
  gpga_double th = gpga_double_zero(0u);
  gpga_double tm = gpga_double_zero(0u);
  Add12Cond(&th, &tm, gpga_double_mul(ah, pow_log2_130_p_coeff_1h), t2h);
  gpga_double tl = gpga_double_add(t2m, gpga_double_mul(ah, pow_log2_130_p_coeff_1m));
  Add22(log2h, log2m, th, tm, pow_log2_130_p_coeff_1m, tl);
  *log2l = gpga_double_zero(0u);
}

inline void gpga_pow_log2_130(thread gpga_double* log2h,
                              thread gpga_double* log2m,
                              thread gpga_double* log2l, gpga_double x) {
  gpga_pow_log2_130_simple(log2h, log2m, log2l, x);
}

inline void gpga_pow_log2_130_core(thread gpga_double* resh,
                                   thread gpga_double* resm,
                                   thread gpga_double* resl, int index,
                                   gpga_double ed, gpga_double xh,
                                   gpga_double xm) {
  gpga_double p_t_1_0h = pow_log2_130_p_coeff_13h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, xh);
  gpga_double p_t_3_0h = gpga_double_add(pow_log2_130_p_coeff_12h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, xh);
  gpga_double p_t_5_0h = gpga_double_add(pow_log2_130_p_coeff_11h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_mul(p_t_5_0h, xh);
  gpga_double p_t_7_0h = gpga_double_add(pow_log2_130_p_coeff_10h, p_t_6_0h);
  gpga_double p_t_8_0h = gpga_double_mul(p_t_7_0h, xh);
  gpga_double p_t_9_0h = gpga_double_zero(0u);
  gpga_double p_t_9_0m = gpga_double_zero(0u);
  Add12(&p_t_9_0h, &p_t_9_0m, pow_log2_130_p_coeff_9h, p_t_8_0h);
  gpga_double p_t_10_0h = gpga_double_zero(0u);
  gpga_double p_t_10_0m = gpga_double_zero(0u);
  Mul22(&p_t_10_0h, &p_t_10_0m, p_t_9_0h, p_t_9_0m, xh, xm);
  gpga_double p_t_11_0h = gpga_double_zero(0u);
  gpga_double p_t_11_0m = gpga_double_zero(0u);
  Add122(&p_t_11_0h, &p_t_11_0m, pow_log2_130_p_coeff_8h, p_t_10_0h,
         p_t_10_0m);
  gpga_double p_t_12_0h = gpga_double_zero(0u);
  gpga_double p_t_12_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_12_0h, &p_t_12_0m, pow_log2_130_p_coeff_7h,
           pow_log2_130_p_coeff_7m, xh, xm, p_t_11_0h, p_t_11_0m);
  gpga_double p_t_13_0h = gpga_double_zero(0u);
  gpga_double p_t_13_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_13_0h, &p_t_13_0m, pow_log2_130_p_coeff_6h,
           pow_log2_130_p_coeff_6m, xh, xm, p_t_12_0h, p_t_12_0m);
  gpga_double p_t_14_0h = gpga_double_zero(0u);
  gpga_double p_t_14_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_14_0h, &p_t_14_0m, pow_log2_130_p_coeff_5h,
           pow_log2_130_p_coeff_5m, xh, xm, p_t_13_0h, p_t_13_0m);
  gpga_double p_t_15_0h = gpga_double_zero(0u);
  gpga_double p_t_15_0m = gpga_double_zero(0u);
  Mul22(&p_t_15_0h, &p_t_15_0m, p_t_14_0h, p_t_14_0m, xh, xm);
  gpga_double p_t_16_0h = gpga_double_zero(0u);
  gpga_double p_t_16_0m = gpga_double_zero(0u);
  gpga_double p_t_16_0l = gpga_double_zero(0u);
  Add23(&p_t_16_0h, &p_t_16_0m, &p_t_16_0l, pow_log2_130_p_coeff_4h,
        pow_log2_130_p_coeff_4m, p_t_15_0h, p_t_15_0m);
  gpga_double p_t_17_0h = gpga_double_zero(0u);
  gpga_double p_t_17_0m = gpga_double_zero(0u);
  gpga_double p_t_17_0l = gpga_double_zero(0u);
  Mul233(&p_t_17_0h, &p_t_17_0m, &p_t_17_0l, xh, xm, p_t_16_0h,
         p_t_16_0m, p_t_16_0l);
  gpga_double p_t_18_0h = gpga_double_zero(0u);
  gpga_double p_t_18_0m = gpga_double_zero(0u);
  gpga_double p_t_18_0l = gpga_double_zero(0u);
  Add233(&p_t_18_0h, &p_t_18_0m, &p_t_18_0l, pow_log2_130_p_coeff_3h,
         pow_log2_130_p_coeff_3m, p_t_17_0h, p_t_17_0m, p_t_17_0l);
  gpga_double p_t_19_0h = gpga_double_zero(0u);
  gpga_double p_t_19_0m = gpga_double_zero(0u);
  gpga_double p_t_19_0l = gpga_double_zero(0u);
  Mul233(&p_t_19_0h, &p_t_19_0m, &p_t_19_0l, xh, xm, p_t_18_0h,
         p_t_18_0m, p_t_18_0l);
  gpga_double p_t_20_0h = gpga_double_zero(0u);
  gpga_double p_t_20_0m = gpga_double_zero(0u);
  gpga_double p_t_20_0l = gpga_double_zero(0u);
  Add33(&p_t_20_0h, &p_t_20_0m, &p_t_20_0l, pow_log2_130_p_coeff_2h,
        pow_log2_130_p_coeff_2m, pow_log2_130_p_coeff_2l, p_t_19_0h,
        p_t_19_0m, p_t_19_0l);
  gpga_double p_t_21_0h = gpga_double_zero(0u);
  gpga_double p_t_21_0m = gpga_double_zero(0u);
  gpga_double p_t_21_0l = gpga_double_zero(0u);
  Mul233(&p_t_21_0h, &p_t_21_0m, &p_t_21_0l, xh, xm, p_t_20_0h,
         p_t_20_0m, p_t_20_0l);
  gpga_double p_t_21_1h = gpga_double_zero(0u);
  gpga_double p_t_21_1m = gpga_double_zero(0u);
  gpga_double p_t_21_1l = gpga_double_zero(0u);
  Renormalize3(&p_t_21_1h, &p_t_21_1m, &p_t_21_1l, p_t_21_0h, p_t_21_0m,
               p_t_21_0l);
  gpga_double p_t_22_0h = gpga_double_zero(0u);
  gpga_double p_t_22_0m = gpga_double_zero(0u);
  gpga_double p_t_22_0l = gpga_double_zero(0u);
  Add33(&p_t_22_0h, &p_t_22_0m, &p_t_22_0l, pow_log2_130_p_coeff_1h,
        pow_log2_130_p_coeff_1m, pow_log2_130_p_coeff_1l, p_t_21_1h,
        p_t_21_1m, p_t_21_1l);
  gpga_double p_t_23_0h = gpga_double_zero(0u);
  gpga_double p_t_23_0m = gpga_double_zero(0u);
  gpga_double p_t_23_0l = gpga_double_zero(0u);
  Mul233(&p_t_23_0h, &p_t_23_0m, &p_t_23_0l, xh, xm, p_t_22_0h,
         p_t_22_0m, p_t_22_0l);

  gpga_double p_resh = p_t_23_0h;
  gpga_double p_resm = p_t_23_0m;
  gpga_double p_resl = p_t_23_0l;

  GpgaPowArgRed table = pow_argredtable[index];
  gpga_double log2yh = gpga_double_zero(0u);
  gpga_double log2ym = gpga_double_zero(0u);
  gpga_double log2yl = gpga_double_zero(0u);
  Add33(&log2yh, &log2ym, &log2yl, table.logih, table.logim, table.logil,
        p_resh, p_resm, p_resl);
  gpga_double log2xh = gpga_double_zero(0u);
  gpga_double log2xm = gpga_double_zero(0u);
  gpga_double log2xl = gpga_double_zero(0u);
  Add133(&log2xh, &log2xm, &log2xl, ed, log2yh, log2ym, log2yl);
  Renormalize3(resh, resm, resl, log2xh, log2xm, log2xl);
}

inline void gpga_pow_exp2_120_core(thread int* H, thread gpga_double* resh,
                                   thread gpga_double* resm,
                                   thread gpga_double* resl,
                                   gpga_double xh, gpga_double xm,
                                   gpga_double xl) {
  gpga_double xhMult2L = gpga_double_mul(xh, pow_two13);
  gpga_double shifted = gpga_double_add(pow_shiftConst, xhMult2L);
  gpga_double rhMult2L = gpga_double_sub(xhMult2L,
                                         gpga_double_sub(shifted,
                                                         pow_shiftConst));
  gpga_double r = gpga_double_mul(rhMult2L, pow_twoM13);
  int k = (int)gpga_u64_lo(shifted);
  *H = k >> 13;
  int index1 = k & (int)pow_INDEXMASK1;
  int index2 = (k & (int)pow_INDEXMASK2) >> 5;

  gpga_double t1h = gpga_double_zero(0u);
  gpga_double t2 = gpga_double_zero(0u);
  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  Add12Cond(&t1h, &t2, r, xm);
  Add12Cond(&t1m, &t1l, t2, xl);
  gpga_double rh = gpga_double_zero(0u);
  gpga_double t3 = gpga_double_zero(0u);
  Add12(&rh, &t3, t1h, t1m);
  gpga_double rm = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  Add12(&rm, &rl, t3, t1l);

  gpga_double p_t_1_0h = pow_exp2_120_p_coeff_6h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, rh);
  gpga_double p_t_3_0h =
      gpga_double_add(pow_exp2_120_p_coeff_5h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, rh);
  gpga_double p_t_5_0h = gpga_double_zero(0u);
  gpga_double p_t_5_0m = gpga_double_zero(0u);
  Add12(&p_t_5_0h, &p_t_5_0m, pow_exp2_120_p_coeff_4h, p_t_4_0h);
  gpga_double p_t_6_0h = gpga_double_zero(0u);
  gpga_double p_t_6_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_6_0h, &p_t_6_0m, pow_exp2_120_p_coeff_3h,
           pow_exp2_120_p_coeff_3m, rh, rm, p_t_5_0h, p_t_5_0m);
  gpga_double p_t_7_0h = gpga_double_zero(0u);
  gpga_double p_t_7_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_7_0h, &p_t_7_0m, pow_exp2_120_p_coeff_2h,
           pow_exp2_120_p_coeff_2m, rh, rm, p_t_6_0h, p_t_6_0m);
  gpga_double p_t_8_0h = gpga_double_zero(0u);
  gpga_double p_t_8_0m = gpga_double_zero(0u);
  Mul22(&p_t_8_0h, &p_t_8_0m, p_t_7_0h, p_t_7_0m, rh, rm);
  gpga_double p_t_9_0h = gpga_double_zero(0u);
  gpga_double p_t_9_0m = gpga_double_zero(0u);
  gpga_double p_t_9_0l = gpga_double_zero(0u);
  Add23(&p_t_9_0h, &p_t_9_0m, &p_t_9_0l, pow_exp2_120_p_coeff_1h,
        pow_exp2_120_p_coeff_1m, p_t_8_0h, p_t_8_0m);
  gpga_double p_t_10_0h = gpga_double_zero(0u);
  gpga_double p_t_10_0m = gpga_double_zero(0u);
  gpga_double p_t_10_0l = gpga_double_zero(0u);
  Mul33(&p_t_10_0h, &p_t_10_0m, &p_t_10_0l, rh, rm, rl, p_t_9_0h, p_t_9_0m,
        p_t_9_0l);
  gpga_double p_t_11_0h = gpga_double_zero(0u);
  gpga_double p_t_11_0m = gpga_double_zero(0u);
  gpga_double p_t_11_0l = gpga_double_zero(0u);
  Add133(&p_t_11_0h, &p_t_11_0m, &p_t_11_0l, pow_exp2_120_p_coeff_0h,
         p_t_10_0h, p_t_10_0m, p_t_10_0l);
  gpga_double p_resh = gpga_double_zero(0u);
  gpga_double p_resm = gpga_double_zero(0u);
  gpga_double p_resl = gpga_double_zero(0u);
  Renormalize3(&p_resh, &p_resm, &p_resl, p_t_11_0h, p_t_11_0m, p_t_11_0l);

  GpgaExpTableEntry tbl1 = exp_twoPowerIndex1[index1];
  GpgaExpTableEntry tbl2 = exp_twoPowerIndex2[index2];
  gpga_double tablesh = gpga_double_zero(0u);
  gpga_double tablesm = gpga_double_zero(0u);
  gpga_double tablesl = gpga_double_zero(0u);
  Mul33(&tablesh, &tablesm, &tablesl, tbl1.hi, tbl1.mi, tbl1.lo, tbl2.hi,
        tbl2.mi, tbl2.lo);
  gpga_double exp2h = gpga_double_zero(0u);
  gpga_double exp2m = gpga_double_zero(0u);
  gpga_double exp2l = gpga_double_zero(0u);
  Mul33(&exp2h, &exp2m, &exp2l, tablesh, tablesm, tablesl, p_resh, p_resm,
        p_resl);
  Renormalize3(resh, resm, resl, exp2h, exp2m, exp2l);
}

inline gpga_double gpga_pow_exp2_120(gpga_double y) {
  int H = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_pow_exp2_120_core(&H, &resh, &resm, &resl, y, gpga_double_zero(0u),
                         gpga_double_zero(0u));
  gpga_double sum = gpga_double_add(resh, gpga_double_add(resm, resl));
  return gpga_double_ldexp(sum, H);
}

inline void gpga_pow_120_core(thread int* H, thread gpga_double* resh,
                              thread gpga_double* resm,
                              thread gpga_double* resl,
                              thread gpga_double* log2xh,
                              gpga_double y, int index, gpga_double ed,
                              gpga_double zh, gpga_double zm) {
  gpga_double log2xm = gpga_double_zero(0u);
  gpga_double log2xl = gpga_double_zero(0u);
  gpga_pow_log2_130_core(log2xh, &log2xm, &log2xl, index, ed, zh, zm);

  gpga_double ylog2xh = gpga_double_zero(0u);
  gpga_double ylog2xm = gpga_double_zero(0u);
  gpga_double ylog2xl = gpga_double_zero(0u);
  Mul133(&ylog2xh, &ylog2xm, &ylog2xl, y, *log2xh, log2xm, log2xl);

  gpga_pow_exp2_120_core(H, resh, resm, resl, ylog2xh, ylog2xm, ylog2xl);
}

inline gpga_double gpga_pow_120(gpga_double x, gpga_double y) {
  if (gpga_double_is_zero(x) || gpga_double_is_inf(x) ||
      gpga_double_is_nan(x)) {
    return gpga_double_pow_int(x, gpga_double_to_s32(y));
  }

  gpga_double xval = x;
  ulong x_bits = xval;
  uint hi = gpga_u64_hi(x_bits);
  int E = 0;
  if ((hi & 0x7FF00000u) == 0u) {
    xval = gpga_double_mul(xval, pow_two52);
    x_bits = xval;
    hi = gpga_u64_hi(x_bits);
    E = -52;
  }
  E += ((int)(hi >> 20) & 0x7FF) - 1023;
  uint idx = hi & 0x000FFFFFu;
  hi = idx | 0x3ff00000u;
  x_bits = gpga_u64_from_words(hi, gpga_u64_lo(x_bits));
  xval = x_bits;
  idx = (idx + (1u << (20 - pow_L - 1))) >> (20 - pow_L);
  if ((int)idx >= pow_MAXINDEX) {
    hi -= 0x00100000u;
    x_bits = gpga_u64_from_words(hi, gpga_u64_lo(x_bits));
    xval = x_bits;
    E += 1;
  }
  idx &= (uint)pow_INDEXMASK;

  gpga_double ed = gpga_double_from_s64(E);
  gpga_double yh = gpga_u64_from_words(gpga_u64_hi(x_bits), 0u);
  gpga_double yl = gpga_double_sub(xval, yh);

  GpgaPowArgRed table = pow_argredtable[idx];
  gpga_double yrih = gpga_double_mul(yh, table.ri);
  gpga_double yril = gpga_double_mul(yl, table.ri);
  gpga_double th = gpga_double_sub(yrih, gpga_double_const_one());
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  Add12Cond(&zh, &zm, th, yril);

  gpga_double log2xh = gpga_double_zero(0u);
  gpga_double log2xm = gpga_double_zero(0u);
  gpga_double log2xl = gpga_double_zero(0u);
  gpga_pow_log2_130_core(&log2xh, &log2xm, &log2xl, (int)idx, ed, zh, zm);

  gpga_double ylog2xh = gpga_double_zero(0u);
  gpga_double ylog2xm = gpga_double_zero(0u);
  gpga_double ylog2xl = gpga_double_zero(0u);
  Mul133(&ylog2xh, &ylog2xm, &ylog2xl, y, log2xh, log2xm, log2xl);

  int H = 0;
  gpga_double powh = gpga_double_zero(0u);
  gpga_double powm = gpga_double_zero(0u);
  gpga_double powl = gpga_double_zero(0u);
  gpga_pow_exp2_120_core(&H, &powh, &powm, &powl, ylog2xh, ylog2xm, ylog2xl);
  gpga_double sum = gpga_double_add(powh, gpga_double_add(powm, powl));
  return gpga_double_ldexp(sum, H);
}

inline bool gpga_pow_round_and_check_rn_core(
    thread gpga_double* pow, int H, gpga_double powh, gpga_double powm,
    gpga_double powl, thread int* G, thread gpga_double* kh,
    thread gpga_double* kl) {
  gpga_double th = gpga_double_zero(0u);
  gpga_double tm = gpga_double_zero(0u);
  gpga_double tl = gpga_double_zero(0u);
  int K = 0;
  if (gpga_double_lt(powh, gpga_double_const_one()) ||
      (gpga_double_eq(powh, gpga_double_const_one()) &&
       gpga_double_lt(powm, gpga_double_zero(0u)))) {
    powh = gpga_double_add(powh, powh);
    powm = gpga_double_add(powm, powm);
    powl = gpga_double_add(powl, powl);
    H -= 1;
  }
  if (gpga_double_gt(powh, gpga_double_const_two()) ||
      (gpga_double_eq(powh, gpga_double_const_two()) &&
       gpga_double_ge(powm, gpga_double_zero(0u)))) {
    gpga_double half_val = gpga_double_const_inv2();
    powh = gpga_double_mul(powh, half_val);
    powm = gpga_double_mul(powm, half_val);
    powl = gpga_double_mul(powl, half_val);
    H += 1;
  }

  if (H <= -1023) {
    gpga_double twodb =
        gpga_double_ldexp(gpga_double_const_one(), H + 1074);
    gpga_double twoH1074powh = gpga_double_mul(twodb, powh);
    gpga_double twoH1074powm = gpga_double_mul(twodb, powm);
    gpga_double shiftedpowh = gpga_double_add(pow_two52, twoH1074powh);
    th = gpga_double_sub(shiftedpowh, pow_two52);
    gpga_double delta = gpga_double_sub(twoH1074powh, th);
    Add12Cond(&tm, &tl, delta, twoH1074powm);
    K = -1074;
  } else {
    th = gpga_double_mul(powh, pow_two52);
    tm = gpga_double_mul(powm, pow_two52);
    tl = gpga_double_mul(powl, pow_two52);
    K = H - 52;
  }

  *G = K;
  *kh = th;
  *kl = tm;

  gpga_double t1m = gpga_double_zero(0u);
  gpga_double t1l = gpga_double_zero(0u);
  if (gpga_double_gt(tm, gpga_double_zero(0u))) {
    t1m = gpga_double_neg(tm);
    t1l = gpga_double_neg(tl);
  } else {
    t1m = tm;
    t1l = tl;
  }
  gpga_double half_val = gpga_double_const_inv2();
  gpga_double delta =
      gpga_double_abs(gpga_double_sub(gpga_double_add(half_val, t1m), t1l));
  gpga_double scaledth = gpga_double_mul(th, pow_PRECISEROUNDCST);

  if (gpga_double_gt(delta, scaledth)) {
    gpga_double abs_tm = gpga_double_abs(tm);
    if (gpga_double_ge(abs_tm, half_val)) {
      if (gpga_double_eq(abs_tm, half_val)) {
        if (gpga_double_lt(tm, gpga_double_zero(0u))) {
          if (gpga_double_lt(tl, gpga_double_zero(0u))) {
            th = gpga_double_sub(th, gpga_double_const_one());
          }
        } else {
          if (gpga_double_gt(tl, gpga_double_zero(0u))) {
            th = gpga_double_add(th, gpga_double_const_one());
          }
        }
      } else {
        if (gpga_double_lt(tm, gpga_double_zero(0u))) {
          th = gpga_double_sub(th, gpga_double_const_one());
        } else {
          th = gpga_double_add(th, gpga_double_const_one());
        }
      }
    }

    int K1 = K >> 1;
    int K2 = K - K1;
    gpga_double two_k1 =
        gpga_double_ldexp(gpga_double_const_one(), K1);
    gpga_double two_k2 =
        gpga_double_ldexp(gpga_double_const_one(), K2);
    *pow = gpga_double_mul(two_k2, gpga_double_mul(two_k1, th));
    return true;
  }
  return false;
}

inline bool gpga_pow_round_and_check_rn(thread gpga_double* rh,
                                        thread gpga_double* rl,
                                        gpga_double zh, gpga_double zm,
                                        gpga_double zl, thread gpga_double* ed,
                                        thread gpga_double* sign) {
  gpga_double addh = gpga_double_zero(0u);
  gpga_double addl = gpga_double_zero(0u);
  Add12Cond(&addh, &addl, zh, zm);
  gpga_double roundcst = pow_RNROUNDCST;
  gpga_double shifted = gpga_double_add(addh, roundcst);
  gpga_double rounded = gpga_double_sub(shifted, roundcst);
  gpga_double diff = gpga_double_sub(addh, rounded);
  gpga_double delta = gpga_double_add(diff, addl);
  if (gpga_double_eq(rounded, gpga_double_zero(0u))) {
    rounded = gpga_double_mul(*sign, rounded);
  }
  *rh = rounded;
  *rl = delta;
  gpga_double abs_delta = gpga_double_abs(delta);
  if (gpga_double_gt(abs_delta, pow_PRECISEROUNDCST)) {
    *ed = gpga_double_abs(delta);
    return true;
  }
  return false;
}

inline gpga_double gpga_pow_exact_case(gpga_double x, gpga_double y,
                                       gpga_double zh, gpga_double zm,
                                       gpga_double sign, gpga_double ed) {
  gpga_double ey = gpga_double_zero(0u);
  gpga_double yy = gpga_double_zero(0u);
  gpga_pow_decompose(y, &yy, &ey);
  gpga_double ey_int = gpga_double_to_s64(ey);
  gpga_double temp = gpga_double_add(ey_int, pow_two52);
  gpga_double bits = gpga_double_add(temp, gpga_double_const_one());
  temp = bits;
  gpga_double eps = gpga_double_sub(temp, pow_two52);
  gpga_double a = gpga_double_mul(zh, eps);
  gpga_double b = gpga_double_mul(zm, eps);
  gpga_double c = gpga_double_add(a, b);
  gpga_double result = gpga_double_add(c, pow_RNROUNDCST);
  gpga_double exact = gpga_double_sub(result, pow_RNROUNDCST);
  return gpga_double_mul(sign, exact);
}

inline bool gpga_pow_exact_case_core(thread gpga_double* pow, gpga_double x,
                                     gpga_double y, int G, gpga_double kh,
                                     gpga_double kl, gpga_double log2xh) {
  gpga_double x_scaled = x;
  if (gpga_double_exp(x_scaled) == 0u) {
    x_scaled = gpga_double_mul(x_scaled, pow_two52);
  }
  uint x_hi = gpga_u64_hi(x_scaled);
  uint x_lo = gpga_u64_lo(x_scaled);
  if (((x_hi & 0x000fffffu) | x_lo) == 0u) {
    uint y_hi = gpga_u64_hi(y);
    gpga_double yh = gpga_u64_from_words(y_hi, 0u);
    gpga_double yl = gpga_double_sub(y, yh);
    gpga_double Eyh = gpga_double_mul(log2xh, yh);
    gpga_double Eyl = gpga_double_mul(log2xh, yl);
    gpga_double delta = gpga_double_sub(
        gpga_double_sub(gpga_double_add(Eyh, pow_shiftConst), pow_shiftConst),
        Eyh);
    if (!gpga_double_eq(delta, Eyl)) {
      return false;
    }
  } else {
    if (gpga_double_lt(y, gpga_double_zero(0u)) ||
        gpga_double_gt(y, gpga_double_from_u32(35u))) {
      return false;
    }
    gpga_double n = gpga_double_zero(0u);
    int F = 0;
    gpga_pow_decompose_odd(y, &n, &F);
    if (gpga_double_gt(n, gpga_double_from_u32(35u)) || F < -5) {
      return false;
    }
    if (F < 0) {
      gpga_double m = gpga_double_zero(0u);
      int E = 0;
      gpga_pow_decompose_odd(x, &m, &E);
      gpga_double ed = gpga_double_from_s32(E);
      gpga_double Ey = gpga_double_mul(ed, y);
      gpga_double shiftedEy = gpga_double_add(Ey, pow_shiftConst);
      gpga_double nearestEy = gpga_double_sub(shiftedEy, pow_shiftConst);
      if (!gpga_double_eq(nearestEy, Ey)) {
        return false;
      }
      int Ey_int = (int)gpga_double_to_s64(nearestEy);
      int scale_exp = G - Ey_int;
      gpga_double scale =
          gpga_double_ldexp(gpga_double_const_one(), scale_exp);
      gpga_double value =
          gpga_double_eq(kl, gpga_double_zero(0u)) ? kh : kl;
      gpga_double scaled_value = gpga_double_mul(scale, value);
      if (!gpga_pow_is_odd_integer(scaled_value)) {
        return false;
      }
    }
  }

  int G1 = G >> 1;
  int G2 = G - G1;
  gpga_double two_g1 = gpga_double_ldexp(gpga_double_const_one(), G1);
  gpga_double two_g2 = gpga_double_ldexp(gpga_double_const_one(), G2);
  gpga_double temp =
      gpga_double_mul(two_g1, gpga_double_mul(kh, two_g2));
  if (!gpga_double_eq(kl, gpga_double_zero(0u)) &&
      ((gpga_u64_lo(temp) & 1u) != 0u)) {
    temp += 1ul;
  }
  *pow = temp;
  return true;
}

inline gpga_double gpga_pow_exact_rn(gpga_double x, gpga_double y,
                                     gpga_double sign, int index,
                                     gpga_double ed, gpga_double zh,
                                     gpga_double zm) {
  int H = 0;
  int G = 0;
  gpga_double powh = gpga_double_zero(0u);
  gpga_double powm = gpga_double_zero(0u);
  gpga_double powl = gpga_double_zero(0u);
  gpga_double log2xh = gpga_double_zero(0u);
  gpga_pow_120_core(&H, &powh, &powm, &powl, &log2xh, y, index, ed, zh, zm);

  gpga_double pow = gpga_double_zero(0u);
  gpga_double kh = gpga_double_zero(0u);
  gpga_double kl = gpga_double_zero(0u);
  if (gpga_pow_round_and_check_rn_core(&pow, H, powh, powm, powl, &G, &kh,
                                       &kl)) {
    return gpga_double_mul(sign, pow);
  }
  if (gpga_pow_exact_case_core(&pow, x, y, G, kh, kl, log2xh)) {
    return gpga_double_mul(sign, pow);
  }
  return gpga_double_from_s32(-5);
}

inline gpga_double gpga_pow_rn(gpga_double x, gpga_double y) {
  gpga_double one = gpga_double_const_one();
  gpga_double zero = gpga_double_zero(0u);
  uint x_exp = gpga_double_exp(x);
  uint y_exp = gpga_double_exp(y);
  if ((((x_exp + 1u) & 0x7ffu) <= 1u) ||
      (((y_exp + 1u) & 0x7ffu) <= 1u)) {
    if (gpga_double_eq(x, one)) {
      return one;
    }
    if (gpga_double_eq(y, zero)) {
      return one;
    }
    if (gpga_double_eq(y, one)) {
      return x;
    }
    if (gpga_double_eq(y, gpga_double_const_two())) {
      return gpga_double_mul(x, x);
    }
    if (gpga_double_eq(y, gpga_double_const_minus_one())) {
      return gpga_double_div(one, x);
    }
    if (gpga_double_is_zero(x) && !gpga_double_is_inf(y) &&
        !gpga_double_is_nan(y)) {
      if (gpga_double_lt(y, zero)) {
        if (gpga_pow_is_odd_integer(y)) {
          return gpga_double_div(one, x);
        }
        return gpga_double_div(one, gpga_double_mul(x, x));
      }
      if (gpga_pow_is_odd_integer(y)) {
        return x;
      }
      return gpga_double_mul(x, x);
    }
    if (gpga_double_is_inf(y) || gpga_double_is_nan(y)) {
      if (gpga_double_is_nan(y)) {
        return y;
      }
      if (gpga_double_eq(x, gpga_double_const_minus_one())) {
        return one;
      }
      gpga_double absx = gpga_double_abs(x);
      bool y_positive = gpga_double_sign(y) == 0u;
      bool abs_gt_one = gpga_double_gt(absx, one);
      if (abs_gt_one ^ y_positive) {
        return zero;
      }
      return gpga_double_abs(y);
    }
    if (gpga_double_is_inf(x) || gpga_double_is_nan(x)) {
      if (gpga_double_is_nan(x)) {
        return x;
      }
      if (gpga_double_sign(x) == 0u) {
        if (gpga_double_gt(y, zero)) {
          return x;
        }
        return zero;
      }
      if (gpga_double_gt(y, zero)) {
        if (gpga_pow_is_odd_integer(y)) {
          return x;
        }
        return gpga_double_abs(x);
      }
      if (gpga_pow_is_odd_integer(y)) {
        return gpga_double_zero(1u);
      }
      return zero;
    }
  }

  gpga_double sign = one;
  if (gpga_double_lt(x, zero)) {
    if (!gpga_pow_is_integer(y)) {
      return gpga_double_nan();
    }
    x = gpga_double_neg(x);
    if (gpga_pow_is_odd_integer(y)) {
      sign = gpga_double_neg(sign);
    }
  }

  gpga_double xdb = x;
  uint hi = gpga_u64_hi(xdb);
  int E = 0;
  if ((hi & 0xfff00000u) == 0u) {
    xdb = gpga_double_mul(xdb, pow_two52);
    E = -52;
    hi = gpga_u64_hi(xdb);
  }

  E += ((int)(hi >> 20) & 0x7ff) - 1023;
  uint index = hi & 0x000fffffu;
  hi = index | 0x3ff00000u;
  xdb = gpga_u64_from_words(hi, gpga_u64_lo(xdb));
  index = (index + (1u << (20 - pow_L - 1))) >> (20 - pow_L);
  if ((int)index >= pow_MAXINDEX) {
    hi -= 0x00100000u;
    xdb = gpga_u64_from_words(hi, gpga_u64_lo(xdb));
    E += 1;
  }
  gpga_double ed = gpga_double_from_s32(E);

  gpga_double yh = gpga_u64_from_words(gpga_u64_hi(xdb), 0u);
  gpga_double yl = gpga_double_sub(xdb, yh);

  if (gpga_double_eq(yl, zero) &&
      (gpga_double_eq(y, gpga_double_from_u32(3u)) ||
       gpga_double_eq(y, gpga_double_from_u32(4u))) &&
      E > -255) {
    if (gpga_double_eq(y, gpga_double_from_u32(3u))) {
      return gpga_double_mul(sign, gpga_double_mul(x, gpga_double_mul(x, x)));
    }
    gpga_double x_sq = gpga_double_mul(x, x);
    return gpga_double_mul(sign, gpga_double_mul(x_sq, x_sq));
  }

  gpga_double f = gpga_double_sub(xdb, one);
  gpga_double log2FastApprox =
      gpga_double_add(ed, gpga_double_mul(pow_logFastCoeff, f));
  gpga_double ylog2xFast = gpga_double_mul(y, log2FastApprox);
  gpga_double abs_ylog2xFast = gpga_double_abs(ylog2xFast);
  gpga_double overflow_bound = gpga_double_from_s32(1261);
  if (gpga_double_ge(abs_ylog2xFast, overflow_bound)) {
    if (gpga_double_gt(ylog2xFast, zero)) {
      gpga_double max_val = gpga_double_mul(sign, pow_LARGEST);
      return gpga_double_mul(max_val, pow_LARGEST);
    }
    gpga_double min_val = gpga_double_mul(sign, pow_SMALLEST);
    return gpga_double_mul(min_val, pow_SMALLEST);
  }

  if (gpga_double_le(abs_ylog2xFast, pow_twoM55)) {
    gpga_double rounded = gpga_double_add(one, pow_SMALLEST);
    return gpga_double_mul(sign, rounded);
  }

  index &= (uint)pow_INDEXMASK;
  GpgaPowArgRed table = pow_argredtable[index];
  gpga_double ri = table.ri;
  gpga_double logih = table.logih;
  gpga_double logim = table.logim;

  gpga_double yrih = gpga_double_mul(yh, ri);
  gpga_double yril = gpga_double_mul(yl, ri);
  gpga_double th = gpga_double_sub(yrih, one);
  gpga_double zh = gpga_double_zero(0u);
  gpga_double zm = gpga_double_zero(0u);
  Add12Cond(&zh, &zm, th, yril);

  gpga_double zhSq = gpga_double_mul(zh, zh);
  gpga_double p35 = gpga_double_add(pow_log2_70_p_coeff_3h,
                                    gpga_double_mul(zhSq,
                                                    pow_log2_70_p_coeff_5h));
  gpga_double p46 = gpga_double_add(pow_log2_70_p_coeff_4h,
                                    gpga_double_mul(zhSq,
                                                    pow_log2_70_p_coeff_6h));
  gpga_double zhFour = gpga_double_mul(zhSq, zhSq);
  gpga_double p36 = gpga_double_add(p35, gpga_double_mul(zh, p46));
  gpga_double p7 = gpga_double_mul(pow_log2_70_p_coeff_7h, zhFour);
  gpga_double p_t_9_0h = gpga_double_add(p36, p7);
  gpga_double p_t_10_0h = gpga_double_mul(p_t_9_0h, zh);
  gpga_double p_t_11_0h = gpga_double_zero(0u);
  gpga_double p_t_11_0m = gpga_double_zero(0u);
  Add212(&p_t_11_0h, &p_t_11_0m, pow_log2_70_p_coeff_2h,
         pow_log2_70_p_coeff_2m, p_t_10_0h);
  gpga_double p_t_12_0h = gpga_double_zero(0u);
  gpga_double p_t_12_0m = gpga_double_zero(0u);
  MulAdd22(&p_t_12_0h, &p_t_12_0m, pow_log2_70_p_coeff_1h,
           pow_log2_70_p_coeff_1m, zh, zm, p_t_11_0h, p_t_11_0m);
  gpga_double log2zh = gpga_double_zero(0u);
  gpga_double log2zm = gpga_double_zero(0u);
  Mul22(&log2zh, &log2zm, p_t_12_0h, p_t_12_0m, zh, zm);

  gpga_double log2yh = gpga_double_zero(0u);
  gpga_double log2ym = gpga_double_zero(0u);
  Add122(&log2yh, &log2ym, ed, logih, logim);
  gpga_double log2xh = gpga_double_zero(0u);
  gpga_double log2xm = gpga_double_zero(0u);
  Add22(&log2xh, &log2xm, log2yh, log2ym, log2zh, log2zm);

  gpga_double ylog2xh = gpga_double_zero(0u);
  gpga_double temp1 = gpga_double_zero(0u);
  Mul12(&ylog2xh, &temp1, y, log2xh);
  gpga_double ylog2xm = gpga_double_add(temp1, gpga_double_mul(y, log2xm));

  gpga_double shifted = gpga_double_add(pow_shiftConstTwoM13, ylog2xh);
  gpga_double r = gpga_double_sub(ylog2xh,
                                  gpga_double_sub(shifted,
                                                  pow_shiftConstTwoM13));
  int k = (int)gpga_u64_lo(shifted);
  int H = k >> 13;
  int index1 = k & (int)pow_INDEXMASK1;
  int index2 = (k & (int)pow_INDEXMASK2) >> 5;
  gpga_double rh = gpga_double_add(r, ylog2xm);

  gpga_double tbl1 = pow_twoPowerIndex1[index1].hiM1;
  gpga_double tbl2h = pow_twoPowerIndex2[index2].hi;
  gpga_double tbl2m = pow_twoPowerIndex2[index2].mi;

  gpga_double p_t_1_0h = exp2_p_coeff_3h;
  gpga_double p_t_2_0h = gpga_double_mul(p_t_1_0h, rh);
  gpga_double p_t_3_0h = gpga_double_add(exp2_p_coeff_2h, p_t_2_0h);
  gpga_double p_t_4_0h = gpga_double_mul(p_t_3_0h, rh);
  gpga_double p_t_5_0h = gpga_double_add(exp2_p_coeff_1h, p_t_4_0h);
  gpga_double ph = gpga_double_mul(p_t_5_0h, rh);

  gpga_double lowerTerms =
      gpga_double_add(tbl1, gpga_double_add(ph, gpga_double_mul(tbl1, ph)));
  gpga_double powh = gpga_double_zero(0u);
  gpga_double powm = gpga_double_zero(0u);
  Add212(&powh, &powm, tbl2h, tbl2m, gpga_double_mul(tbl2h, lowerTerms));

  if (H >= 1025) {
    gpga_double max_val = gpga_double_mul(sign, pow_LARGEST);
    return gpga_double_mul(max_val, pow_LARGEST);
  }
  if (H <= -1077) {
    gpga_double min_val = gpga_double_mul(sign, pow_SMALLEST);
    return gpga_double_mul(min_val, pow_SMALLEST);
  }

  if (H > -1022) {
    gpga_double rounded = gpga_double_add(powh, gpga_double_mul(powm, pow_RNROUNDCST));
    if (gpga_double_eq(powh, rounded)) {
      gpga_double powdb = powh;
      uint pow_hi = gpga_u64_hi(powdb);
      uint pow_lo = gpga_u64_lo(powdb);
      if (H < 1023) {
        pow_hi += (uint)(H << 20);
        powdb = gpga_u64_from_words(pow_hi, pow_lo);
        return gpga_double_mul(sign, powdb);
      }
      pow_hi += (uint)((H - 3) << 20);
      powdb = gpga_u64_from_words(pow_hi, pow_lo);
      return gpga_double_mul(sign, gpga_double_mul(powdb, gpga_double_from_u32(8u)));
    }
  } else {
    if (gpga_double_lt(powh, one) ||
        (gpga_double_eq(powh, one) && gpga_double_lt(powm, zero))) {
      powh = gpga_double_add(powh, powh);
      powm = gpga_double_add(powm, powm);
      H -= 1;
    }
    if (gpga_double_gt(powh, gpga_double_const_two()) ||
        (gpga_double_eq(powh, gpga_double_const_two()) && gpga_double_ge(powm, zero))) {
      gpga_double half_val = gpga_double_const_inv2();
      powh = gpga_double_mul(powh, half_val);
      powm = gpga_double_mul(powm, half_val);
      H += 1;
    }

    if (H > -1023) {
      gpga_double rounded = gpga_double_add(powh, gpga_double_mul(powm, pow_RNROUNDCST));
      if (gpga_double_eq(powh, rounded)) {
        gpga_double powdb = powh;
        uint pow_hi = gpga_u64_hi(powdb);
        uint pow_lo = gpga_u64_lo(powdb);
        pow_hi += (uint)(H << 20);
        powdb = gpga_u64_from_words(pow_hi, pow_lo);
        return gpga_double_mul(sign, powdb);
      }
    } else {
      gpga_double twodb = gpga_double_ldexp(one, H + 1074);
      gpga_double twoH1074powh = gpga_double_mul(twodb, powh);
      gpga_double twoH1074powm = gpga_double_mul(twodb, powm);
      gpga_double shiftedpowh = gpga_double_add(pow_two52, twoH1074powh);
      gpga_double nearestintpowh = gpga_double_sub(shiftedpowh, pow_two52);
      gpga_double deltaint = gpga_double_sub(twoH1074powh, nearestintpowh);
      gpga_double delta = gpga_double_add(deltaint, twoH1074powm);

      if (gpga_double_gt(gpga_double_abs(delta), gpga_double_const_inv2())) {
        if (gpga_double_gt(delta, zero)) {
          nearestintpowh = gpga_double_add(nearestintpowh, one);
          delta = gpga_double_sub(delta, one);
        } else {
          nearestintpowh = gpga_double_sub(nearestintpowh, one);
          delta = gpga_double_add(delta, one);
        }
      }

      gpga_double rest = gpga_double_abs(
          gpga_double_sub(gpga_double_const_inv2(), gpga_double_abs(delta)));
      if (gpga_double_gt(gpga_double_mul(rest, pow_SUBNORMROUNDCST), twodb)) {
        gpga_double result = gpga_double_add(nearestintpowh, pow_two52);
        uint res_hi = gpga_u64_hi(result);
        uint res_lo = gpga_u64_lo(result);
        res_hi = (res_hi + (1u << 20)) & 0x001fffffu;
        result = gpga_u64_from_words(res_hi, res_lo);
        return gpga_double_mul(sign, result);
      }
    }
  }

  return gpga_pow_exact_rn(x, y, sign, index, ed, zh, zm);
}

// CRLIBM_CSH_FAST
GPGA_CONST gpga_double csh_maxepsilon = 0x3c2c60de1bd9507dul;
GPGA_CONST gpga_double csh_round_cst = 0x3ff039a85dae6c53ul;
GPGA_CONST gpga_double csh_inv_ln_2 = 0x3ff71547652b82feul;
GPGA_CONST gpga_double csh_ln2_hi = 0x3fe62e42fefa3800ul;
GPGA_CONST gpga_double csh_ln2_lo = 0x3d2ef35793c76730ul;
GPGA_CONST gpga_double csh_two_43_44 = 0x42b8000000000000ul;
GPGA_CONST gpga_double csh_two_minus_30 = 0x3d70000000000000ul;
GPGA_CONST int csh_bias = 89;
GPGA_CONST gpga_double csh_max_input = 0x408633ce8fb9f87eul;
GPGA_CONST gpga_double csh_c2 = 0x3fe0000000000000ul;
GPGA_CONST gpga_double csh_c4 = 0x3fa5555555555555ul;
GPGA_CONST gpga_double csh_c6 = 0x3f56c16c16c16c17ul;
GPGA_CONST gpga_double csh_s3 = 0x3fc5555555555555ul;
GPGA_CONST gpga_double csh_s5 = 0x3f81111111111111ul;
GPGA_CONST gpga_double csh_s7 = 0x3f2a01a01a01a01aul;
GPGA_CONST gpga_double csh_largest_double = 0x7feffffffffffffful;
GPGA_CONST gpga_double csh_tiniest_double = 0x0000000000000001ul;

GPGA_CONST gpga_double csh_cosh_sinh_table[179][4] = {
  { 0x3ff0fa08d2f35f97ul, 0x3c9b39d19dab3af1ul, 0xbfd6b36fbb84c928ul, 0xbc68daf8fefe1e5ful },
  { 0x3ff0f46473177841ul, 0xbc97df6029551c51ul, 0xbfd66f92e6a06fc9ul, 0x3c70a785d9a66b42ul },
  { 0x3ff0eed107a16db4ul, 0x3c988108d3e071f5ul, 0xbfd62bcc8150dbabul, 0xbc7700bdf95d00caul },
  { 0x3ff0e94e8afdd406ul, 0xbc9330cbb0b14f49ul, 0xbfd5e81c47cfa1dbul, 0x3c791ec54e7b2c63ul },
  { 0x3ff0e3dcf7aa2e1bul, 0x3c9c05bdf7fb3140ul, 0xbfd5a481f66c8331ul, 0xbc6e8e0ccff78dd1ul },
  { 0x3ff0de7c4834e82eul, 0xbc877bec5f430e44ul, 0xbfd560fd498d28aaul, 0x3c4f07b3ccea8a26ul },
  { 0x3ff0d92c773d5255ul, 0xbc84956c3d5d1da2ul, 0xbfd51d8dfdacdfc5ul, 0x3c419f0e98966b86ul },
  { 0x3ff0d3ed7f739b28ul, 0xbc99722fea8a9ed5ul, 0xbfd4da33cf5c5703ul, 0xbc7b1f077be71fbeul },
  { 0x3ff0cebf5b98ca6cul, 0x3c9e808009b9f220ul, 0xbfd496ee7b415a78ul, 0x3c51c3babdfe61f1ul },
  { 0x3ff0c9a2067ebbdaul, 0x3c813cd8803d61f3ul, 0xbfd453bdbe16906cul, 0xbc78d78145d8536eul },
  { 0x3ff0c4957b0819e9ul, 0x3c9a26e06e52ba24ul, 0xbfd410a154ab361dul, 0x3c78dcb6cf7da2e8ul },
  { 0x3ff0bf99b42858b8ul, 0xbc67ad830b1f30a5ul, 0xbfd3cd98fbe2dc86ul, 0x3c7d0a5e269038fcul },
  { 0x3ff0baaeace3b0fcul, 0xbc93292f75521e77ul, 0xbfd38aa470b52549ul, 0x3c67f94b0b01b8a6ul },
  { 0x3ff0b5d4604f1b07ul, 0x3c732c1407eecfa5ul, 0xbfd347c3702d7fa4ul, 0xbc7435be701422c8ul },
  { 0x3ff0b10ac99049deul, 0xbc617c2d610fcc2eul, 0xbfd304f5b76ae57eul, 0x3c75eb0942e4ca9cul },
  { 0x3ff0ac51e3dda65bul, 0x3c9b22c09daca977ul, 0xbfd2c23b039f9881ul, 0xbc5773019b082732ul },
  { 0x3ff0a7a9aa7e4a68ul, 0x3c71b296281520e0ul, 0xbfd27f931210df54ul, 0x3c6ef999502a1e59ul },
  { 0x3ff0a31218c9fc41ul, 0x3c88525909e044c2ul, 0xbfd23cfda016c2d9ul, 0x3c500762449d986bul },
  { 0x3ff09e8b2a2929d1ul, 0xbc9ee5f5535446b4ul, 0xbfd1fa7a6b1bcb8aul, 0xbc50621a5a4cb636ul },
  { 0x3ff09a14da14e415ul, 0xbc965668233b29c8ul, 0xbfd1b809309cbee1ul, 0xbc6e5b16bbb1cb75ul },
  { 0x3ff095af2416da9aul, 0x3c8024a039dc7749ul, 0xbfd175a9ae285cd6ul, 0x3c3db749c4496e39ul },
  { 0x3ff0915a03c95705ul, 0x3c78a8a60bd1cd00ul, 0xbfd1335ba15f1d6cul, 0x3c53ce0f341ed7b6ul },
  { 0x3ff08d1574d738acul, 0xbc984bd5c4a8c043ul, 0xbfd0f11ec7f2ee53ul, 0xbc75badfc5355afcul },
  { 0x3ff088e172fbf041ul, 0xbc9de12f0d55140ful, 0xbfd0aef2dfa6f09bul, 0x3c414e60a7827088ul },
  { 0x3ff084bdfa037b8ful, 0xbc7d97e2e3c5ebfcul, 0xbfd06cd7a64f3673ul, 0xbc6370ba839871e4ul },
  { 0x3ff080ab05ca6146ul, 0xbc723216fc66378ful, 0xbfd02accd9d08102ul, 0x3c5998b320c03715ul },
  { 0x3ff07ca8923dacd6ul, 0xbc9653a7e736d67aul, 0xbfcfd1a4703ffc8ful, 0xbc59ea3ba8860819ul },
  { 0x3ff078b69b5aea5cul, 0xbc8880d66b6d819bul, 0xbfcf4dcefe860e28ul, 0xbc6883d4c6a2c678ul },
  { 0x3ff074d51d3022a1ul, 0x3c9e93f82426ef93ul, 0xbfceca18da9dba19ul, 0x3c36d470491d8a98ul },
  { 0x3ff0710413dbd729ul, 0x3c766560ca5328edul, 0xbfce468180d0d17ful, 0x3c605627658d0660ul },
  { 0x3ff06d437b8cfe4dul, 0xbc73adf7d1025e7eul, 0xbfcdc3086d87ef96ul, 0x3c6c0bd92a97b34cul },
  { 0x3ff069935082ff6eul, 0x3c7685c4d7395a9ful, 0xbfcd3fad1d49f620ul, 0x3c5de85126ece4f0ul },
  { 0x3ff065f38f0daf34ul, 0xbc7821b3af1520a9ul, 0xbfccbc6f0cbb89edul, 0xbc4b40a051092884ul },
  { 0x3ff06264338d4bdcul, 0xbc90000ff34422a4ul, 0xbfcc394db89e8f7ful, 0xbc646f7752292d2dul },
  { 0x3ff05ee53a727999ul, 0x3c92512f6647668dul, 0xbfcbb6489dd1a7ccul, 0x3c2344253ed3824dul },
  { 0x3ff05b76a03e3f07ul, 0x3c7ca6152b52e765ul, 0xbfcb335f394fad1bul, 0xbc47f25734b51aceul },
  { 0x3ff05818618201a8ul, 0xbc8bfa985a557953ul, 0xbfcab091082f3002ul, 0x3c4b662660bf022cul },
  { 0x3ff054ca7adf8277ul, 0x3c89c0c1377a9f8cul, 0xbfca2ddd87a1f479ul, 0xbc67e5cef07409b2ul },
  { 0x3ff0518ce908da8cul, 0x3c987d1825a535e2ul, 0xbfc9ab4434f46f10ul, 0xbc1274bc57573053ul },
  { 0x3ff04e5fa8c077ccul, 0xbc9d8190ecf07bf2ul, 0xbfc928c48d8d4236ul, 0xbc556048370edf81ul },
  { 0x3ff04b42b6d919a9ul, 0xbc9f233892aa0a0bul, 0xbfc8a65e0eecbba5ul, 0x3c6339db0663df15ul },
  { 0x3ff048361035cdfaul, 0xbc9e50aabbc5ec1cul, 0xbfc8241036ac51ddul, 0xbc5a42dcdf8cb355ul },
  { 0x3ff04539b1c9eddaul, 0x3c8d2224c7598281ul, 0xbfc7a1da827e21c3ul, 0xbc6cb79f06d46085ul },
  { 0x3ff0424d98991a9ful, 0x3c66bb23a130683dul, 0xbfc71fbc702c6c4ful, 0xbc6baa2a00c832ddul },
  { 0x3ff03f71c1b73ad9ul, 0xbc885168a8202e85ul, 0xbfc69db57d991458ul, 0x3c69321c97b08bd2ul },
  { 0x3ff03ca62a487769ul, 0xbc9585fdb95a2e63ul, 0xbfc61bc528bd1c73ul, 0x3c6255592267ecebul },
  { 0x3ff039eacf8138a4ul, 0x3c8acc0c8fe6098dul, 0xbfc599eaefa824f1ul, 0x3c5f28e72b9ba1a8ul },
  { 0x3ff0373faea6238aul, 0xbc73e9a172989a92ul, 0xbfc51826507fe9ebul, 0x3c6ac2196946da28ul },
  { 0x3ff034a4c50c1706ul, 0xbc8a90767ecc2f29ul, 0xbfc49676c97fc168ul, 0x3c6b5d1393c07384ul },
  { 0x3ff0321a10182946ul, 0x3c88fa5cfe5f3ff1ul, 0xbfc414dbd8f81999ul, 0x3c3e14380b2260acul },
  { 0x3ff02f9f8d3fa521ul, 0xbc58a44a88a18235ul, 0xbfc39354fd4df72aul, 0xbc4eaac264ec3b0cul },
  { 0x3ff02d353a080789ul, 0xbc907780f6c7e6f8ul, 0xbfc311e1b4fa73a6ul, 0x3c61065e4c87081dul },
  { 0x3ff02adb1406fd12ul, 0x3c969329720d8336ul, 0xbfc290817e8a3beful, 0x3c65a4e28f21151dul },
  { 0x3ff0289118e25f8bul, 0xbc9a7da4524aba66ul, 0xbfc20f33d89d0ecdul, 0x3c6557f755a9198dul },
  { 0x3ff026574650339cul, 0xbc31ec5d0688dd52ul, 0xbfc18df841e53b8cul, 0xbc49852d50e682bbul },
  { 0x3ff0242d9a16a685ul, 0xbc7352fd295277dbul, 0xbfc10cce392720b0ul, 0xbc55a2911e4c8e28ul },
  { 0x3ff02214120c0bdeul, 0xbc9456d26b72974eul, 0xbfc08bb53d38aab7ul, 0xbc60fb6dd37d3177ul },
  { 0x3ff0200aac16db6ful, 0xbc809b4f99576fc1ul, 0xbfc00aaccd00d2f1ul, 0x3c53ea29146349deul },
  { 0x3ff01e11662daf18ul, 0xbc833f823120653cul, 0xbfbf1368ceee3cc9ul, 0x3c5213e77bd924abul },
  { 0x3ff01c283e5740c5ul, 0x3c95d2959ec02117ul, 0xbfbe119717463991ul, 0xbc5e1fca410e61a9ul },
  { 0x3ff01a4f32aa6878ul, 0x3c833fda67bcc8b5ul, 0xbfbd0fe37137cf18ul, 0x3c4ae71d698e234cul },
  { 0x3ff01886414e1a5cul, 0x3c4976c5e0b191daul, 0xbfbc0e4cdb0f41d4ul, 0x3c586525c9bfc58bul },
  { 0x3ff016cd687964eful, 0xbc9d38ab209297adul, 0xbfbb0cd25335e625ul, 0xbc480157fbdc145aul },
  { 0x3ff01524a6736f36ul, 0x3c930803c450fc30ul, 0xbfba0b72d8311ebeul, 0xbc507aa850193d71ul },
  { 0x3ff0138bf993770aul, 0xbc7502e0b809a4d7ul, 0xbfb90a2d68a15b27ul, 0xbc5b1be37f556554ul },
  { 0x3ff012036040cf67ul, 0x3c9847c0422fb0bcul, 0xbfb8090103411660ul, 0xbc5dd34210739476ul },
  { 0x3ff0108ad8f2dedbul, 0x3c93dc2153b40698ul, 0xbfb707eca6e3d59bul, 0x3c5030b2f456fffful },
  { 0x3ff00f2262311df8ul, 0x3c99322ea7c410f3ul, 0xbfb606ef5275270dul, 0x3c56910ee6ee4dc3ul },
  { 0x3ff00dc9fa9315dful, 0xbc84d0e2602b4c56ul, 0xbfb5060804f7a0ddul, 0xbc239ff7edaf37d2ul },
  { 0x3ff00c81a0c05ed4ul, 0xbc7736a77a57ac2eul, 0xbfb40535bd83e026ul, 0x3c4b9b735f0b8ac5ul },
  { 0x3ff00b4953709eeaul, 0xbc9df4d4f8cdacd0ul, 0xbfb304777b47880cul, 0xbc517be53b77e1b2ul },
  { 0x3ff00a21116b88b6ul, 0xbc71d458aaaec5a4ul, 0xbfb203cc3d8440eful, 0x3c1ef16241b5a4e1ul },
  { 0x3ff00908d988da1bul, 0x3c9519b194c96ca5ul, 0xbfb10333038eb7a7ul, 0x3c044b95317baea7ul },
  { 0x3ff00800aab05b20ul, 0xbc936eb99febdb21ul, 0xbfb002aacccd9cddul, 0x3c53a7fdfac9c47cul },
  { 0x3ff0070883d9dcd5ul, 0xbc9ec747430de399ul, 0xbfae0465317148ddul, 0x3c4b5db17929765aul },
  { 0x3ff00620640d384ful, 0xbc97fcbc170fb049ul, 0xbfac0392cdaf09cful, 0x3c2b59cd7d4f7337ul },
  { 0x3ff005484a624daeul, 0x3c5d057523e9c180ul, 0xbfaa02dc6d81ee12ul, 0x3c2a5e3b7e00ba39ul },
  { 0x3ff0048036010336ul, 0xbc89227d10beb244ul, 0xbfa8024010336abful, 0x3c4941afc229b627ul },
  { 0x3ff003c826214474ul, 0xbc4ae17a45dfb29bul, 0xbfa601bbb526f7cful, 0x3c39422f2da1796bul },
  { 0x3ff003201a0b0179ul, 0x3c9fa45bd21a4c9aul, 0xbfa4014d5bd80f80ul, 0xbc45a2e6a4813a5bul },
  { 0x3ff0028811162e22ul, 0x3c6f0e222a930764ul, 0xbfa200f303d82dd0ul, 0xbc4be9ef0cbdb96bul },
  { 0x3ff002000aaac16cul, 0x3c88618f578ddd8dul, 0xbfa000aaacccd00dul, 0xbbfd9e591eff67c8ul },
  { 0x3ff001880640b4e1ul, 0x3c7fb10f8827c989ul, 0xbf9c00e4acdae8f4ul, 0xbbe9474122a0bac6ul },
  { 0x3ff001200360040dul, 0xbc884c57402eab5ful, 0xbf98009001033411ul, 0xbc37e141dd340191ul },
  { 0x3ff000c801a0ac06ul, 0xbc7bd6c4673a2eeeul, 0xbf94005355bd803eul, 0xbbd1997bca73460dul },
  { 0x3ff0008000aaab06ul, 0xbc93e2be2abad90dul, 0xbf90002aaaccccdaul, 0x3c2930213ac1711cul },
  { 0x3ff0004800360010ul, 0x3c899ae6db8f0a40ul, 0xbf88002400103337ul, 0x3c250734af88b300ul },
  { 0x3ff00020000aaaacul, 0x3c76c1861862adfdul, 0xbf80000aaaaccccdul, 0xbbba01fc9193923eul },
  { 0x3ff000080000aaabul, 0xbc93e93e8d68d67bul, 0xbf700002aaaacccdul, 0x3c09319317bfb4dbul },
  { 0x3ff0000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul, 0x0000000000000000ul },
  { 0x3ff000080000aaabul, 0xbc93e93e8d68d67bul, 0x3f700002aaaacccdul, 0xbc09319317bfb4dbul },
  { 0x3ff00020000aaaacul, 0x3c76c1861862adfdul, 0x3f80000aaaaccccdul, 0x3bba01fc9193923eul },
  { 0x3ff0004800360010ul, 0x3c899ae6db8f0a40ul, 0x3f88002400103337ul, 0xbc250734af88b300ul },
  { 0x3ff0008000aaab06ul, 0xbc93e2be2abad90dul, 0x3f90002aaaccccdaul, 0xbc2930213ac1711cul },
  { 0x3ff000c801a0ac06ul, 0xbc7bd6c4673a2eeeul, 0x3f94005355bd803eul, 0x3bd1997bca73460dul },
  { 0x3ff001200360040dul, 0xbc884c57402eab5ful, 0x3f98009001033411ul, 0x3c37e141dd340191ul },
  { 0x3ff001880640b4e1ul, 0x3c7fb10f8827c989ul, 0x3f9c00e4acdae8f4ul, 0x3be9474122a0bac6ul },
  { 0x3ff002000aaac16cul, 0x3c88618f578ddd8dul, 0x3fa000aaacccd00dul, 0x3bfd9e591eff67c8ul },
  { 0x3ff0028811162e22ul, 0x3c6f0e222a930764ul, 0x3fa200f303d82dd0ul, 0x3c4be9ef0cbdb96bul },
  { 0x3ff003201a0b0179ul, 0x3c9fa45bd21a4c9aul, 0x3fa4014d5bd80f80ul, 0x3c45a2e6a4813a5bul },
  { 0x3ff003c826214474ul, 0xbc4ae17a45dfb29bul, 0x3fa601bbb526f7cful, 0xbc39422f2da1796bul },
  { 0x3ff0048036010336ul, 0xbc89227d10beb244ul, 0x3fa8024010336abful, 0xbc4941afc229b627ul },
  { 0x3ff005484a624daeul, 0x3c5d057523e9c180ul, 0x3faa02dc6d81ee12ul, 0xbc2a5e3b7e00ba39ul },
  { 0x3ff00620640d384ful, 0xbc97fcbc170fb049ul, 0x3fac0392cdaf09cful, 0xbc2b59cd7d4f7337ul },
  { 0x3ff0070883d9dcd5ul, 0xbc9ec747430de399ul, 0x3fae0465317148ddul, 0xbc4b5db17929765aul },
  { 0x3ff00800aab05b20ul, 0xbc936eb99febdb21ul, 0x3fb002aacccd9cddul, 0xbc53a7fdfac9c47cul },
  { 0x3ff00908d988da1bul, 0x3c9519b194c96ca5ul, 0x3fb10333038eb7a7ul, 0xbc044b95317baea7ul },
  { 0x3ff00a21116b88b6ul, 0xbc71d458aaaec5a4ul, 0x3fb203cc3d8440eful, 0xbc1ef16241b5a4e1ul },
  { 0x3ff00b4953709eeaul, 0xbc9df4d4f8cdacd0ul, 0x3fb304777b47880cul, 0x3c517be53b77e1b2ul },
  { 0x3ff00c81a0c05ed4ul, 0xbc7736a77a57ac2eul, 0x3fb40535bd83e026ul, 0xbc4b9b735f0b8ac5ul },
  { 0x3ff00dc9fa9315dful, 0xbc84d0e2602b4c56ul, 0x3fb5060804f7a0ddul, 0x3c239ff7edaf37d2ul },
  { 0x3ff00f2262311df8ul, 0x3c99322ea7c410f3ul, 0x3fb606ef5275270dul, 0xbc56910ee6ee4dc3ul },
  { 0x3ff0108ad8f2dedbul, 0x3c93dc2153b40698ul, 0x3fb707eca6e3d59bul, 0xbc5030b2f456fffful },
  { 0x3ff012036040cf67ul, 0x3c9847c0422fb0bcul, 0x3fb8090103411660ul, 0x3c5dd34210739476ul },
  { 0x3ff0138bf993770aul, 0xbc7502e0b809a4d7ul, 0x3fb90a2d68a15b27ul, 0x3c5b1be37f556554ul },
  { 0x3ff01524a6736f36ul, 0x3c930803c450fc30ul, 0x3fba0b72d8311ebeul, 0x3c507aa850193d71ul },
  { 0x3ff016cd687964eful, 0xbc9d38ab209297adul, 0x3fbb0cd25335e625ul, 0x3c480157fbdc145aul },
  { 0x3ff01886414e1a5cul, 0x3c4976c5e0b191daul, 0x3fbc0e4cdb0f41d4ul, 0xbc586525c9bfc58bul },
  { 0x3ff01a4f32aa6878ul, 0x3c833fda67bcc8b5ul, 0x3fbd0fe37137cf18ul, 0xbc4ae71d698e234cul },
  { 0x3ff01c283e5740c5ul, 0x3c95d2959ec02117ul, 0x3fbe119717463991ul, 0x3c5e1fca410e61a9ul },
  { 0x3ff01e11662daf18ul, 0xbc833f823120653cul, 0x3fbf1368ceee3cc9ul, 0xbc5213e77bd924abul },
  { 0x3ff0200aac16db6ful, 0xbc809b4f99576fc1ul, 0x3fc00aaccd00d2f1ul, 0xbc53ea29146349deul },
  { 0x3ff02214120c0bdeul, 0xbc9456d26b72974eul, 0x3fc08bb53d38aab7ul, 0x3c60fb6dd37d3177ul },
  { 0x3ff0242d9a16a685ul, 0xbc7352fd295277dbul, 0x3fc10cce392720b0ul, 0x3c55a2911e4c8e28ul },
  { 0x3ff026574650339cul, 0xbc31ec5d0688dd52ul, 0x3fc18df841e53b8cul, 0x3c49852d50e682bbul },
  { 0x3ff0289118e25f8bul, 0xbc9a7da4524aba66ul, 0x3fc20f33d89d0ecdul, 0xbc6557f755a9198dul },
  { 0x3ff02adb1406fd12ul, 0x3c969329720d8336ul, 0x3fc290817e8a3beful, 0xbc65a4e28f21151dul },
  { 0x3ff02d353a080789ul, 0xbc907780f6c7e6f8ul, 0x3fc311e1b4fa73a6ul, 0xbc61065e4c87081dul },
  { 0x3ff02f9f8d3fa521ul, 0xbc58a44a88a18235ul, 0x3fc39354fd4df72aul, 0x3c4eaac264ec3b0cul },
  { 0x3ff0321a10182946ul, 0x3c88fa5cfe5f3ff1ul, 0x3fc414dbd8f81999ul, 0xbc3e14380b2260acul },
  { 0x3ff034a4c50c1706ul, 0xbc8a90767ecc2f29ul, 0x3fc49676c97fc168ul, 0xbc6b5d1393c07384ul },
  { 0x3ff0373faea6238aul, 0xbc73e9a172989a92ul, 0x3fc51826507fe9ebul, 0xbc6ac2196946da28ul },
  { 0x3ff039eacf8138a4ul, 0x3c8acc0c8fe6098dul, 0x3fc599eaefa824f1ul, 0xbc5f28e72b9ba1a8ul },
  { 0x3ff03ca62a487769ul, 0xbc9585fdb95a2e63ul, 0x3fc61bc528bd1c73ul, 0xbc6255592267ecebul },
  { 0x3ff03f71c1b73ad9ul, 0xbc885168a8202e85ul, 0x3fc69db57d991458ul, 0xbc69321c97b08bd2ul },
  { 0x3ff0424d98991a9ful, 0x3c66bb23a130683dul, 0x3fc71fbc702c6c4ful, 0x3c6baa2a00c832ddul },
  { 0x3ff04539b1c9eddaul, 0x3c8d2224c7598281ul, 0x3fc7a1da827e21c3ul, 0x3c6cb79f06d46085ul },
  { 0x3ff048361035cdfaul, 0xbc9e50aabbc5ec1cul, 0x3fc8241036ac51ddul, 0x3c5a42dcdf8cb355ul },
  { 0x3ff04b42b6d919a9ul, 0xbc9f233892aa0a0bul, 0x3fc8a65e0eecbba5ul, 0xbc6339db0663df15ul },
  { 0x3ff04e5fa8c077ccul, 0xbc9d8190ecf07bf2ul, 0x3fc928c48d8d4236ul, 0x3c556048370edf81ul },
  { 0x3ff0518ce908da8cul, 0x3c987d1825a535e2ul, 0x3fc9ab4434f46f10ul, 0x3c1274bc57573053ul },
  { 0x3ff054ca7adf8277ul, 0x3c89c0c1377a9f8cul, 0x3fca2ddd87a1f479ul, 0x3c67e5cef07409b2ul },
  { 0x3ff05818618201a8ul, 0xbc8bfa985a557953ul, 0x3fcab091082f3002ul, 0xbc4b662660bf022cul },
  { 0x3ff05b76a03e3f07ul, 0x3c7ca6152b52e765ul, 0x3fcb335f394fad1bul, 0x3c47f25734b51aceul },
  { 0x3ff05ee53a727999ul, 0x3c92512f6647668dul, 0x3fcbb6489dd1a7ccul, 0xbc2344253ed3824dul },
  { 0x3ff06264338d4bdcul, 0xbc90000ff34422a4ul, 0x3fcc394db89e8f7ful, 0x3c646f7752292d2dul },
  { 0x3ff065f38f0daf34ul, 0xbc7821b3af1520a9ul, 0x3fccbc6f0cbb89edul, 0x3c4b40a051092884ul },
  { 0x3ff069935082ff6eul, 0x3c7685c4d7395a9ful, 0x3fcd3fad1d49f620ul, 0xbc5de85126ece4f0ul },
  { 0x3ff06d437b8cfe4dul, 0xbc73adf7d1025e7eul, 0x3fcdc3086d87ef96ul, 0xbc6c0bd92a97b34cul },
  { 0x3ff0710413dbd729ul, 0x3c766560ca5328edul, 0x3fce468180d0d17ful, 0xbc605627658d0660ul },
  { 0x3ff074d51d3022a1ul, 0x3c9e93f82426ef93ul, 0x3fceca18da9dba19ul, 0xbc36d470491d8a98ul },
  { 0x3ff078b69b5aea5cul, 0xbc8880d66b6d819bul, 0x3fcf4dcefe860e28ul, 0x3c6883d4c6a2c678ul },
  { 0x3ff07ca8923dacd6ul, 0xbc9653a7e736d67aul, 0x3fcfd1a4703ffc8ful, 0x3c59ea3ba8860819ul },
  { 0x3ff080ab05ca6146ul, 0xbc723216fc66378ful, 0x3fd02accd9d08102ul, 0xbc5998b320c03715ul },
  { 0x3ff084bdfa037b8ful, 0xbc7d97e2e3c5ebfcul, 0x3fd06cd7a64f3673ul, 0x3c6370ba839871e4ul },
  { 0x3ff088e172fbf041ul, 0xbc9de12f0d55140ful, 0x3fd0aef2dfa6f09bul, 0xbc414e60a7827088ul },
  { 0x3ff08d1574d738acul, 0xbc984bd5c4a8c043ul, 0x3fd0f11ec7f2ee53ul, 0x3c75badfc5355afcul },
  { 0x3ff0915a03c95705ul, 0x3c78a8a60bd1cd00ul, 0x3fd1335ba15f1d6cul, 0xbc53ce0f341ed7b6ul },
  { 0x3ff095af2416da9aul, 0x3c8024a039dc7749ul, 0x3fd175a9ae285cd6ul, 0xbc3db749c4496e39ul },
  { 0x3ff09a14da14e415ul, 0xbc965668233b29c8ul, 0x3fd1b809309cbee1ul, 0x3c6e5b16bbb1cb75ul },
  { 0x3ff09e8b2a2929d1ul, 0xbc9ee5f5535446b4ul, 0x3fd1fa7a6b1bcb8aul, 0x3c50621a5a4cb636ul },
  { 0x3ff0a31218c9fc41ul, 0x3c88525909e044c2ul, 0x3fd23cfda016c2d9ul, 0xbc500762449d986bul },
  { 0x3ff0a7a9aa7e4a68ul, 0x3c71b296281520e0ul, 0x3fd27f931210df54ul, 0xbc6ef999502a1e59ul },
  { 0x3ff0ac51e3dda65bul, 0x3c9b22c09daca977ul, 0x3fd2c23b039f9881ul, 0x3c5773019b082732ul },
  { 0x3ff0b10ac99049deul, 0xbc617c2d610fcc2eul, 0x3fd304f5b76ae57eul, 0xbc75eb0942e4ca9cul },
  { 0x3ff0b5d4604f1b07ul, 0x3c732c1407eecfa5ul, 0x3fd347c3702d7fa4ul, 0x3c7435be701422c8ul },
  { 0x3ff0baaeace3b0fcul, 0xbc93292f75521e77ul, 0x3fd38aa470b52549ul, 0xbc67f94b0b01b8a6ul },
  { 0x3ff0bf99b42858b8ul, 0xbc67ad830b1f30a5ul, 0x3fd3cd98fbe2dc86ul, 0xbc7d0a5e269038fcul },
  { 0x3ff0c4957b0819e9ul, 0x3c9a26e06e52ba24ul, 0x3fd410a154ab361dul, 0xbc78dcb6cf7da2e8ul },
  { 0x3ff0c9a2067ebbdaul, 0x3c813cd8803d61f3ul, 0x3fd453bdbe16906cul, 0x3c78d78145d8536eul },
  { 0x3ff0cebf5b98ca6cul, 0x3c9e808009b9f220ul, 0x3fd496ee7b415a78ul, 0xbc51c3babdfe61f1ul },
  { 0x3ff0d3ed7f739b28ul, 0xbc99722fea8a9ed5ul, 0x3fd4da33cf5c5703ul, 0x3c7b1f077be71fbeul },
  { 0x3ff0d92c773d5255ul, 0xbc84956c3d5d1da2ul, 0x3fd51d8dfdacdfc5ul, 0xbc419f0e98966b86ul },
  { 0x3ff0de7c4834e82eul, 0xbc877bec5f430e44ul, 0x3fd560fd498d28aaul, 0xbc4f07b3ccea8a26ul },
  { 0x3ff0e3dcf7aa2e1bul, 0x3c9c05bdf7fb3140ul, 0x3fd5a481f66c8331ul, 0x3c6e8e0ccff78dd1ul },
  { 0x3ff0e94e8afdd406ul, 0xbc9330cbb0b14f49ul, 0x3fd5e81c47cfa1dbul, 0xbc791ec54e7b2c63ul },
  { 0x3ff0eed107a16db4ul, 0x3c988108d3e071f5ul, 0x3fd62bcc8150dbabul, 0x3c7700bdf95d00caul },
  { 0x3ff0f46473177841ul, 0xbc97df6029551c51ul, 0x3fd66f92e6a06fc9ul, 0xbc70a785d9a66b42ul },
  { 0x3ff0fa08d2f35f97ul, 0x3c9b39d19dab3af1ul, 0x3fd6b36fbb84c928ul, 0x3c68daf8fefe1e5ful },
};

inline void gpga_csh_do_cosh(gpga_double x, thread gpga_double* preshi,
                             thread gpga_double* preslo) {
  int k = 0;
  gpga_double ch_hi = gpga_double_zero(0u);
  gpga_double ch_lo = gpga_double_zero(0u);
  gpga_double sh_hi = gpga_double_zero(0u);
  gpga_double sh_lo = gpga_double_zero(0u);
  gpga_double temp_hi = gpga_double_zero(0u);
  gpga_double temp_lo = gpga_double_zero(0u);
  gpga_double temp = gpga_double_zero(0u);
  gpga_double b_hi = gpga_double_zero(0u);
  gpga_double b_lo = gpga_double_zero(0u);
  gpga_double b_sa_hi = gpga_double_zero(0u);
  gpga_double b_sa_lo = gpga_double_zero(0u);
  gpga_double b_ca_hi = gpga_double_zero(0u);
  gpga_double b_ca_lo = gpga_double_zero(0u);
  gpga_double ca_hi = gpga_double_zero(0u);
  gpga_double ca_lo = gpga_double_zero(0u);
  gpga_double sa_hi = gpga_double_zero(0u);
  gpga_double sa_lo = gpga_double_zero(0u);
  gpga_double tcb_hi = gpga_double_zero(0u);
  gpga_double tsb_hi = gpga_double_zero(0u);
  gpga_double square_b_hi = gpga_double_zero(0u);
  gpga_double ch_2_pk_hi = gpga_double_zero(0u);
  gpga_double ch_2_pk_lo = gpga_double_zero(0u);
  gpga_double ch_2_mk_hi = gpga_double_zero(0u);
  gpga_double ch_2_mk_lo = gpga_double_zero(0u);
  gpga_double sh_2_pk_hi = gpga_double_zero(0u);
  gpga_double sh_2_pk_lo = gpga_double_zero(0u);
  gpga_double sh_2_mk_hi = gpga_double_zero(0u);
  gpga_double sh_2_mk_lo = gpga_double_zero(0u);

  gpga_double x_mult = gpga_double_mul(x, csh_inv_ln_2);
  gpga_double shifted = gpga_double_add(x_mult, exp_shiftConst);
  k = (int)gpga_u64_lo(shifted);

  if (k != 0) {
    gpga_double k_d = gpga_double_from_s32(k);
    temp_hi = gpga_double_sub(x, gpga_double_mul(csh_ln2_hi, k_d));
    temp_lo = gpga_double_mul(gpga_double_neg(csh_ln2_lo), k_d);
    Add12Cond(&b_hi, &b_lo, temp_hi, temp_lo);
  } else {
    b_hi = x;
    b_lo = gpga_double_zero(0u);
  }

  gpga_double two_p_plus_k = gpga_exp_make_pow2(k - 1 + 1023);
  gpga_double two_p_minus_k = gpga_exp_make_pow2(-k - 1 + 1023);

  gpga_double table_index_float = gpga_double_add(b_hi, csh_two_43_44);
  int table_index = (int)gpga_u64_lo(table_index_float);
  table_index_float = gpga_double_sub(table_index_float, csh_two_43_44);
  table_index += csh_bias;
  b_hi = gpga_double_sub(b_hi, table_index_float);

  square_b_hi = gpga_double_mul(b_hi, b_hi);
  uint abs_y_hi = gpga_u64_hi(b_hi) & 0x7fffffffU;
  uint two_minus_30_hi = gpga_u64_hi(csh_two_minus_30);
  if (abs_y_hi < two_minus_30_hi) {
    tcb_hi = gpga_double_zero(0u);
    tsb_hi = gpga_double_zero(0u);
  } else {
    gpga_double t1 = gpga_double_add(csh_c4, gpga_double_mul(square_b_hi, csh_c6));
    gpga_double t2 = gpga_double_add(csh_c2, gpga_double_mul(square_b_hi, t1));
    tcb_hi = gpga_double_mul(square_b_hi, t2);
    gpga_double s1 = gpga_double_add(csh_s5, gpga_double_mul(square_b_hi, csh_s7));
    gpga_double s2 = gpga_double_add(csh_s3, gpga_double_mul(square_b_hi, s1));
    tsb_hi = gpga_double_mul(square_b_hi, s2);
  }

  if (table_index != csh_bias) {
    ca_hi = csh_cosh_sinh_table[table_index][0];
    ca_lo = csh_cosh_sinh_table[table_index][1];
    sa_hi = csh_cosh_sinh_table[table_index][2];
    sa_lo = csh_cosh_sinh_table[table_index][3];

    Mul12(&b_sa_hi, &b_sa_lo, sa_hi, b_hi);
    temp = gpga_double_add(ca_lo, gpga_double_mul(b_hi, sa_lo));
    temp = gpga_double_add(temp, gpga_double_mul(b_lo, sa_hi));
    temp = gpga_double_add(temp, b_sa_lo);
    temp = gpga_double_add(temp, gpga_double_mul(b_sa_hi, tsb_hi));
    temp = gpga_double_add(temp, gpga_double_mul(ca_hi, tcb_hi));
    temp = gpga_double_add(temp, b_sa_hi);
    Add12Cond(&ch_hi, &ch_lo, ca_hi, temp);
  } else {
    Add12Cond(&ch_hi, &ch_lo, gpga_double_from_u32(1u), tcb_hi);
  }

  if (k != 0) {
    if (table_index != csh_bias) {
      Mul12(&b_ca_hi, &b_ca_lo, ca_hi, b_hi);
      temp = gpga_double_add(sa_lo, gpga_double_mul(b_lo, ca_hi));
      temp = gpga_double_add(temp, gpga_double_mul(b_hi, ca_lo));
      temp = gpga_double_add(temp, b_ca_lo);
      temp = gpga_double_add(temp, gpga_double_mul(sa_hi, tcb_hi));
      temp = gpga_double_add(temp, gpga_double_mul(b_ca_hi, tsb_hi));
      Add12(&temp_hi, &temp_lo, b_ca_hi, temp);
      Add22Cond(&sh_hi, &sh_lo, sa_hi, gpga_double_zero(0u), temp_hi, temp_lo);
    } else {
      Add12Cond(&sh_hi, &sh_lo, b_hi,
                gpga_double_add(gpga_double_mul(tsb_hi, b_hi), b_lo));
    }

    if ((k < 35) && (k > -35)) {
      ch_2_pk_hi = gpga_double_mul(ch_hi, two_p_plus_k);
      ch_2_pk_lo = gpga_double_mul(ch_lo, two_p_plus_k);
      ch_2_mk_hi = gpga_double_mul(ch_hi, two_p_minus_k);
      ch_2_mk_lo = gpga_double_mul(ch_lo, two_p_minus_k);
      sh_2_pk_hi = gpga_double_mul(sh_hi, two_p_plus_k);
      sh_2_pk_lo = gpga_double_mul(sh_lo, two_p_plus_k);
      sh_2_mk_hi = gpga_double_mul(gpga_double_neg(sh_hi), two_p_minus_k);
      sh_2_mk_lo = gpga_double_mul(gpga_double_neg(sh_lo), two_p_minus_k);

      Add22Cond(preshi, preslo, ch_2_mk_hi, ch_2_mk_lo, sh_2_mk_hi, sh_2_mk_lo);
      Add22Cond(&ch_2_mk_hi, &ch_2_mk_lo, sh_2_pk_hi, sh_2_pk_lo,
                *preshi, *preslo);
      Add22Cond(preshi, preslo, ch_2_pk_hi, ch_2_pk_lo, ch_2_mk_hi,
                ch_2_mk_lo);
    } else if (k >= 35) {
      ch_2_pk_hi = gpga_double_mul(ch_hi, two_p_plus_k);
      ch_2_pk_lo = gpga_double_mul(ch_lo, two_p_plus_k);
      sh_2_pk_hi = gpga_double_mul(sh_hi, two_p_plus_k);
      sh_2_pk_lo = gpga_double_mul(sh_lo, two_p_plus_k);
      Add22Cond(preshi, preslo, ch_2_pk_hi, ch_2_pk_lo, sh_2_pk_hi, sh_2_pk_lo);
    } else {
      ch_2_mk_hi = gpga_double_mul(ch_hi, two_p_minus_k);
      ch_2_mk_lo = gpga_double_mul(ch_lo, two_p_minus_k);
      sh_2_mk_hi = gpga_double_mul(gpga_double_neg(sh_hi), two_p_minus_k);
      sh_2_mk_lo = gpga_double_mul(gpga_double_neg(sh_lo), two_p_minus_k);
      Add22Cond(preshi, preslo, ch_2_mk_hi, ch_2_mk_lo, sh_2_mk_hi, sh_2_mk_lo);
    }
  } else {
    *preshi = ch_hi;
    *preslo = ch_lo;
  }
}

inline void gpga_csh_do_cosh_accurate(thread int* pexponent,
                                      thread gpga_double* presh,
                                      thread gpga_double* presm,
                                      thread gpga_double* presl,
                                      gpga_double x) {
  gpga_double exph = gpga_double_zero(0u);
  gpga_double expm = gpga_double_zero(0u);
  gpga_double expl = gpga_double_zero(0u);
  gpga_double expph = gpga_double_zero(0u);
  gpga_double exppm = gpga_double_zero(0u);
  gpga_double exppl = gpga_double_zero(0u);
  gpga_double expmh = gpga_double_zero(0u);
  gpga_double expmm = gpga_double_zero(0u);
  gpga_double expml = gpga_double_zero(0u);
  int exponentm = 0;

  if (gpga_double_lt(x, gpga_double_zero(0u))) {
    x = gpga_double_neg(x);
  }
  if (gpga_double_gt(x, gpga_double_from_s32(40))) {
    gpga_exp13(pexponent, presh, presm, presl, x);
    return;
  }

  gpga_exp13(pexponent, &expph, &exppm, &exppl, x);
  gpga_exp13(&exponentm, &expmh, &expmm, &expml, gpga_double_neg(x));
  int deltaexponent = exponentm - *pexponent;
  if (!gpga_double_is_zero(expmh)) {
    expmh = gpga_exp_adjust_exponent(expmh, deltaexponent);
  }
  if (!gpga_double_is_zero(expmm)) {
    expmm = gpga_exp_adjust_exponent(expmm, deltaexponent);
  }
  if (!gpga_double_is_zero(expml)) {
    expml = gpga_exp_adjust_exponent(expml, deltaexponent);
  }
  Add33(&exph, &expm, &expl, expph, exppm, exppl, expmh, expmm, expml);
  Renormalize3(presh, presm, presl, exph, expm, expl);
}

inline gpga_double gpga_cosh_rn(gpga_double x) {
  uint hx = gpga_u64_hi(x) & 0x7fffffffU;
  uint lo = gpga_u64_lo(x);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if (hx > max_input_hi) {
    if (hx >= 0x7ff00000u) {
      if (((hx & 0x000fffffU) | lo) != 0u) {
        return gpga_double_add(x, x);
      }
      return gpga_double_inf(0u);
    }
  }

  if (gpga_double_ge(x, csh_max_input) ||
      gpga_double_le(x, gpga_double_neg(csh_max_input))) {
    return gpga_double_mul(csh_largest_double, csh_largest_double);
  }

  if (hx < 0x3e500000u) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    return gpga_double_add(gpga_double_from_u32(1u), csh_tiniest_double);
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_cosh(x, &rh, &rl);

  gpga_double test = gpga_double_add(rh, gpga_double_mul(rl, csh_round_cst));
  if (gpga_double_eq(rh, test)) {
    return rh;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_cosh_accurate(&exponent, &resh, &resm, &resl, x);
  gpga_double res = gpga_double_zero(0u);
  RoundToNearest3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_cosh_ru(gpga_double x) {
  uint hx = gpga_u64_hi(x) & 0x7fffffffU;
  uint lo = gpga_u64_lo(x);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if (hx > max_input_hi) {
    if (hx >= 0x7ff00000u && (((hx & 0x000fffffU) | lo) != 0u)) {
      return x;
    }
    return gpga_double_inf(0u);
  }

  if (gpga_double_ge(x, csh_max_input) ||
      gpga_double_le(x, gpga_double_neg(csh_max_input))) {
    return gpga_double_mul(csh_largest_double, csh_largest_double);
  }

  if (hx < 0x3e500000u) {
    if (gpga_double_is_zero(x)) {
      return gpga_double_from_u32(1u);
    }
    return gpga_double_next_up(gpga_double_from_u32(1u));
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_cosh(x, &rh, &rl);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(rh, rl, csh_maxepsilon, &res)) {
    return res;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_cosh_accurate(&exponent, &resh, &resm, &resl, x);
  RoundUpwards3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_cosh_rd(gpga_double x) {
  uint hx = gpga_u64_hi(x) & 0x7fffffffU;
  uint lo = gpga_u64_lo(x);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if (hx > max_input_hi) {
    if (hx >= 0x7ff00000u) {
      if (((hx & 0x000fffffU) | lo) != 0u) {
        return x;
      }
      return gpga_double_inf(0u);
    }
  }

  if (gpga_double_ge(x, csh_max_input) ||
      gpga_double_le(x, gpga_double_neg(csh_max_input))) {
    return csh_largest_double;
  }

  if (hx < 0x3e500000u) {
    return gpga_double_from_u32(1u);
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_cosh(x, &rh, &rl);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(rh, rl, csh_maxepsilon, &res)) {
    return res;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_cosh_accurate(&exponent, &resh, &resm, &resl, x);
  RoundDownwards3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_cosh_rz(gpga_double x) {
  return gpga_cosh_rd(x);
}

inline void gpga_csh_do_sinh(gpga_double x, thread gpga_double* prh,
                             thread gpga_double* prl) {
  int k = 0;
  gpga_double temp1 = gpga_double_zero(0u);
  gpga_double ch_hi = gpga_double_zero(0u);
  gpga_double ch_lo = gpga_double_zero(0u);
  gpga_double sh_hi = gpga_double_zero(0u);
  gpga_double sh_lo = gpga_double_zero(0u);
  gpga_double ch_2_pk_hi = gpga_double_zero(0u);
  gpga_double ch_2_pk_lo = gpga_double_zero(0u);
  gpga_double ch_2_mk_hi = gpga_double_zero(0u);
  gpga_double ch_2_mk_lo = gpga_double_zero(0u);
  gpga_double sh_2_pk_hi = gpga_double_zero(0u);
  gpga_double sh_2_pk_lo = gpga_double_zero(0u);
  gpga_double sh_2_mk_hi = gpga_double_zero(0u);
  gpga_double sh_2_mk_lo = gpga_double_zero(0u);
  gpga_double b_hi = gpga_double_zero(0u);
  gpga_double b_lo = gpga_double_zero(0u);
  gpga_double ca_b_hi = gpga_double_zero(0u);
  gpga_double ca_b_lo = gpga_double_zero(0u);
  gpga_double temp_hi = gpga_double_zero(0u);
  gpga_double temp_lo = gpga_double_zero(0u);
  gpga_double sa_b_hi = gpga_double_zero(0u);
  gpga_double sa_b_lo = gpga_double_zero(0u);
  gpga_double ca_hi = gpga_double_zero(0u);
  gpga_double ca_lo = gpga_double_zero(0u);
  gpga_double sa_hi = gpga_double_zero(0u);
  gpga_double sa_lo = gpga_double_zero(0u);
  gpga_double tcb_hi = gpga_double_zero(0u);
  gpga_double tsb_hi = gpga_double_zero(0u);
  gpga_double square_y_hi = gpga_double_zero(0u);

  gpga_double x_mult = gpga_double_mul(x, csh_inv_ln_2);
  gpga_double shifted = gpga_double_add(x_mult, exp_shiftConst);
  k = (int)gpga_u64_lo(shifted);
  if (k != 0) {
    gpga_double k_d = gpga_double_from_s32(k);
    temp_hi = gpga_double_sub(x, gpga_double_mul(csh_ln2_hi, k_d));
    temp_lo = gpga_double_mul(gpga_double_neg(csh_ln2_lo), k_d);
    Add12Cond(&b_hi, &b_lo, temp_hi, temp_lo);
  } else {
    b_hi = x;
    b_lo = gpga_double_zero(0u);
  }

  gpga_double two_p_plus_k = gpga_exp_make_pow2(k - 1 + 1023);
  gpga_double two_p_minus_k = gpga_exp_make_pow2(-k - 1 + 1023);

  gpga_double table_index_float = gpga_double_add(b_hi, csh_two_43_44);
  int table_index = (int)gpga_u64_lo(table_index_float);
  table_index_float = gpga_double_sub(table_index_float, csh_two_43_44);
  table_index += csh_bias;
  b_hi = gpga_double_sub(b_hi, table_index_float);

  square_y_hi = gpga_double_mul(b_hi, b_hi);
  uint abs_y_hi = gpga_u64_hi(b_hi) & 0x7fffffffU;
  uint two_minus_30_hi = gpga_u64_hi(csh_two_minus_30);
  if (abs_y_hi <= two_minus_30_hi) {
    tsb_hi = gpga_double_zero(0u);
    tcb_hi = gpga_double_zero(0u);
  } else {
    gpga_double s1 = gpga_double_add(csh_s5, gpga_double_mul(square_y_hi, csh_s7));
    tsb_hi = gpga_double_mul(square_y_hi,
                             gpga_double_add(csh_s3, gpga_double_mul(square_y_hi, s1)));
    gpga_double t1 = gpga_double_add(csh_c4, gpga_double_mul(square_y_hi, csh_c6));
    tcb_hi = gpga_double_mul(square_y_hi,
                             gpga_double_add(csh_c2, gpga_double_mul(square_y_hi, t1)));
  }

  if (table_index != csh_bias) {
    ca_hi = csh_cosh_sinh_table[table_index][0];
    ca_lo = csh_cosh_sinh_table[table_index][1];
    sa_hi = csh_cosh_sinh_table[table_index][2];
    sa_lo = csh_cosh_sinh_table[table_index][3];

    temp1 = sa_lo;
    temp1 = gpga_double_add(temp1, gpga_double_mul(b_lo, ca_hi));
    temp1 = gpga_double_add(temp1, gpga_double_mul(b_hi, ca_lo));
    Mul12(&ca_b_hi, &ca_b_lo, ca_hi, b_hi);
    temp1 = gpga_double_add(temp1, ca_b_lo);
    temp1 = gpga_double_add(temp1, gpga_double_mul(sa_hi, tcb_hi));
    temp1 = gpga_double_add(temp1, gpga_double_mul(ca_b_hi, tsb_hi));
    Add12Cond(&temp_hi, &temp_lo, ca_b_hi, temp1);
    Add22Cond(&sh_hi, &sh_lo, sa_hi, gpga_double_zero(0u), temp_hi, temp_lo);

    temp1 = ca_lo;
    Mul12(&sa_b_hi, &sa_b_lo, sa_hi, b_hi);
    temp1 = gpga_double_add(temp1, gpga_double_mul(b_hi, sa_lo));
    temp1 = gpga_double_add(temp1, gpga_double_mul(b_lo, sa_hi));
    temp1 = gpga_double_add(temp1, sa_b_lo);
    temp1 = gpga_double_add(temp1, gpga_double_mul(sa_b_hi, tsb_hi));
    temp1 = gpga_double_add(temp1, gpga_double_mul(ca_hi, tcb_hi));
    temp1 = gpga_double_add(temp1, sa_b_hi);
    Add12Cond(&ch_hi, &ch_lo, ca_hi, temp1);
  } else {
    Add12Cond(&sh_hi, &sh_lo, b_hi,
              gpga_double_add(gpga_double_mul(tsb_hi, b_hi), b_lo));
    Add12Cond(&ch_hi, &ch_lo, gpga_double_from_u32(1u), tcb_hi);
  }

  if (k != 0) {
    if ((k < 35) && (k > -35)) {
      ch_2_pk_hi = gpga_double_mul(ch_hi, two_p_plus_k);
      ch_2_pk_lo = gpga_double_mul(ch_lo, two_p_plus_k);
      ch_2_mk_hi = gpga_double_mul(gpga_double_neg(ch_hi), two_p_minus_k);
      ch_2_mk_lo = gpga_double_mul(gpga_double_neg(ch_lo), two_p_minus_k);
      sh_2_pk_hi = gpga_double_mul(sh_hi, two_p_plus_k);
      sh_2_pk_lo = gpga_double_mul(sh_lo, two_p_plus_k);
      sh_2_mk_hi = gpga_double_mul(sh_hi, two_p_minus_k);
      sh_2_mk_lo = gpga_double_mul(sh_lo, two_p_minus_k);

      Add22Cond(prh, prl, ch_2_mk_hi, ch_2_mk_lo, sh_2_mk_hi, sh_2_mk_lo);
      Add22Cond(&ch_2_mk_hi, &ch_2_mk_lo, sh_2_pk_hi, sh_2_pk_lo, *prh, *prl);
      Add22Cond(prh, prl, ch_2_pk_hi, ch_2_pk_lo, ch_2_mk_hi, ch_2_mk_lo);
    } else if (k >= 35) {
      ch_2_pk_hi = gpga_double_mul(ch_hi, two_p_plus_k);
      ch_2_pk_lo = gpga_double_mul(ch_lo, two_p_plus_k);
      sh_2_pk_hi = gpga_double_mul(sh_hi, two_p_plus_k);
      sh_2_pk_lo = gpga_double_mul(sh_lo, two_p_plus_k);
      Add22Cond(prh, prl, ch_2_pk_hi, ch_2_pk_lo, sh_2_pk_hi, sh_2_pk_lo);
    } else {
      ch_2_mk_hi = gpga_double_mul(gpga_double_neg(ch_hi), two_p_minus_k);
      ch_2_mk_lo = gpga_double_mul(gpga_double_neg(ch_lo), two_p_minus_k);
      sh_2_mk_hi = gpga_double_mul(sh_hi, two_p_minus_k);
      sh_2_mk_lo = gpga_double_mul(sh_lo, two_p_minus_k);
      Add22Cond(prh, prl, ch_2_mk_hi, ch_2_mk_lo, sh_2_mk_hi, sh_2_mk_lo);
    }
  } else {
    *prh = sh_hi;
    *prl = sh_lo;
  }
}

inline void gpga_csh_do_sinh_accurate(thread int* pexponent,
                                      thread gpga_double* presh,
                                      thread gpga_double* presm,
                                      thread gpga_double* presl,
                                      gpga_double x) {
  gpga_double exph = gpga_double_zero(0u);
  gpga_double expm = gpga_double_zero(0u);
  gpga_double expl = gpga_double_zero(0u);
  gpga_double expph = gpga_double_zero(0u);
  gpga_double exppm = gpga_double_zero(0u);
  gpga_double exppl = gpga_double_zero(0u);
  gpga_double expmh = gpga_double_zero(0u);
  gpga_double expmm = gpga_double_zero(0u);
  gpga_double expml = gpga_double_zero(0u);

  if (gpga_double_gt(x, gpga_double_from_s32(40))) {
    gpga_exp13(pexponent, presh, presm, presl, x);
    return;
  }
  if (gpga_double_lt(x, gpga_double_from_s32(-40))) {
    gpga_exp13(pexponent, presh, presm, presl, gpga_double_neg(x));
    *presh = gpga_double_neg(*presh);
    *presm = gpga_double_neg(*presm);
    *presl = gpga_double_neg(*presl);
    return;
  }

  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    gpga_expm1_13(&expph, &exppm, &exppl, x);
    gpga_expm1_13(&expmh, &expmm, &expml, gpga_double_neg(x));
    Add33(&exph, &expm, &expl, expph, exppm, exppl,
          gpga_double_neg(expmh), gpga_double_neg(expmm), gpga_double_neg(expml));
    Renormalize3(presh, presm, presl, exph, expm, expl);
    *pexponent = 0;
    return;
  }

  gpga_expm1_13(&expph, &exppm, &exppl, x);
  gpga_expm1_13(&expmh, &expmm, &expml, gpga_double_neg(x));
  Add33(&exph, &expm, &expl, gpga_double_neg(expmh), gpga_double_neg(expmm),
        gpga_double_neg(expml), expph, exppm, exppl);
  Renormalize3(presh, presm, presl, exph, expm, expl);
  *pexponent = 0;
}

inline gpga_double gpga_sinh_rn(gpga_double x) {
  uint hx = gpga_u64_hi(x) & 0x7fffffffU;
  uint lo = gpga_u64_lo(x);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if (hx > max_input_hi) {
    if (hx >= 0x7ff00000u) {
      if (((hx & 0x000fffffU) | lo) != 0u) {
        return gpga_double_add(x, x);
      }
      return x;
    }
    if (gpga_double_gt(x, csh_max_input)) {
      return gpga_double_mul(csh_largest_double, csh_largest_double);
    }
    if (gpga_double_lt(x, gpga_double_neg(csh_max_input))) {
      return gpga_double_neg(gpga_double_mul(csh_largest_double,
                                             csh_largest_double));
    }
  }

  if (hx < 0x3e500000u) {
    return x;
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_sinh(x, &rh, &rl);

  gpga_double test = gpga_double_add(rh, gpga_double_mul(rl, csh_round_cst));
  if (gpga_double_eq(rh, test)) {
    return rh;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_sinh_accurate(&exponent, &resh, &resm, &resl, x);
  gpga_double res = gpga_double_zero(0u);
  RoundToNearest3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_sinh_ru(gpga_double x) {
  gpga_double ax = gpga_double_abs(x);
  uint ax_hi = gpga_u64_hi(ax);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if ((ax_hi & 0x7ff00000u) >= 0x7ff00000u) {
    return x;
  }

  if (gpga_double_gt(ax, csh_max_input)) {
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      return gpga_double_inf(0u);
    }
    gpga_double neg = gpga_double_neg(csh_largest_double);
    return neg;
  }

  if (ax_hi < 0x3e500000u) {
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      return gpga_double_next_up(x);
    }
    return x;
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_sinh(x, &rh, &rl);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_ru(rh, rl, csh_maxepsilon, &res)) {
    return res;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_sinh_accurate(&exponent, &resh, &resm, &resl, x);
  RoundUpwards3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_sinh_rd(gpga_double x) {
  gpga_double ax = gpga_double_abs(x);
  uint ax_hi = gpga_u64_hi(ax);
  uint max_input_hi = gpga_u64_hi(csh_max_input);

  if ((ax_hi & 0x7ff00000u) >= 0x7ff00000u) {
    return x;
  }

  if (gpga_double_gt(ax, csh_max_input)) {
    if (gpga_double_gt(x, gpga_double_zero(0u))) {
      return csh_largest_double;
    }
    return gpga_double_inf(1u);
  }

  if (ax_hi < 0x3e500000u) {
    if (gpga_double_lt(x, gpga_double_zero(0u))) {
      return gpga_double_next_down(x);
    }
    return x;
  }

  gpga_double rh = gpga_double_zero(0u);
  gpga_double rl = gpga_double_zero(0u);
  gpga_csh_do_sinh(x, &rh, &rl);

  gpga_double res = gpga_double_zero(0u);
  if (gpga_test_and_return_rd(rh, rl, csh_maxepsilon, &res)) {
    return res;
  }

  int exponent = 0;
  gpga_double resh = gpga_double_zero(0u);
  gpga_double resm = gpga_double_zero(0u);
  gpga_double resl = gpga_double_zero(0u);
  gpga_csh_do_sinh_accurate(&exponent, &resh, &resm, &resl, x);
  RoundDownwards3(&res, resh, resm, resl);

  uint hi = gpga_u64_hi(res);
  hi = (uint)((int)hi + ((exponent - 11) << 20));
  res = gpga_u64_from_words(hi, gpga_u64_lo(res));
  return gpga_double_mul(gpga_double_from_u32(1024u), res);
}

inline gpga_double gpga_sinh_rz(gpga_double x) {
  if (gpga_double_gt(x, gpga_double_zero(0u))) {
    return gpga_sinh_rd(x);
  }
  return gpga_sinh_ru(x);
}

#undef R_HW
#undef R_SGN
#undef R_IND
#undef R_EXP
#undef X_HW
#undef X_SGN
#undef X_IND
#undef X_EXP
#undef Y_HW
#undef Y_SGN
#undef Y_IND
#undef Y_EXP
#undef Z_HW
#undef Z_SGN
#undef Z_IND
#undef Z_EXP
#undef SCS_CARRY_PROPAGATE
#endif  // GPGA_REAL_H
