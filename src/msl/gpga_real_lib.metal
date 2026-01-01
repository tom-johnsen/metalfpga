#define gpga_bits_to_real gpga_real_impl_bits_to_real
#define gpga_real_to_bits gpga_real_impl_real_to_bits
#define gpga_double_from_u64 gpga_real_impl_double_from_u64
#define gpga_double_from_s64 gpga_real_impl_double_from_s64
#define gpga_double_from_u32 gpga_real_impl_double_from_u32
#define gpga_double_from_s32 gpga_real_impl_double_from_s32
#define gpga_double_to_s64 gpga_real_impl_double_to_s64
#define gpga_double_neg gpga_real_impl_double_neg
#define gpga_double_add gpga_real_impl_double_add
#define gpga_double_sub gpga_real_impl_double_sub
#define gpga_double_mul gpga_real_impl_double_mul
#define gpga_double_div gpga_real_impl_double_div
#define gpga_double_pow gpga_real_impl_double_pow
#define gpga_double_log10 gpga_real_impl_double_log10
#define gpga_double_ln gpga_real_impl_double_ln
#define gpga_double_exp_real gpga_real_impl_double_exp_real
#define gpga_double_sqrt gpga_real_impl_double_sqrt
#define gpga_double_floor gpga_real_impl_double_floor
#define gpga_double_ceil gpga_real_impl_double_ceil
#define gpga_double_sin gpga_real_impl_double_sin
#define gpga_double_cos gpga_real_impl_double_cos
#define gpga_double_tan gpga_real_impl_double_tan
#define gpga_double_asin gpga_real_impl_double_asin
#define gpga_double_acos gpga_real_impl_double_acos
#define gpga_double_atan gpga_real_impl_double_atan
#define gpga_double_eq gpga_real_impl_double_eq
#define gpga_double_lt gpga_real_impl_double_lt
#define gpga_double_gt gpga_real_impl_double_gt
#define gpga_double_le gpga_real_impl_double_le
#define gpga_double_ge gpga_real_impl_double_ge
#define gpga_double_is_zero gpga_real_impl_double_is_zero

#include "gpga_real.h"

#undef gpga_bits_to_real
#undef gpga_real_to_bits
#undef gpga_double_from_u64
#undef gpga_double_from_s64
#undef gpga_double_from_u32
#undef gpga_double_from_s32
#undef gpga_double_to_s64
#undef gpga_double_neg
#undef gpga_double_add
#undef gpga_double_sub
#undef gpga_double_mul
#undef gpga_double_div
#undef gpga_double_pow
#undef gpga_double_log10
#undef gpga_double_ln
#undef gpga_double_exp_real
#undef gpga_double_sqrt
#undef gpga_double_floor
#undef gpga_double_ceil
#undef gpga_double_sin
#undef gpga_double_cos
#undef gpga_double_tan
#undef gpga_double_asin
#undef gpga_double_acos
#undef gpga_double_atan
#undef gpga_double_eq
#undef gpga_double_lt
#undef gpga_double_gt
#undef gpga_double_le
#undef gpga_double_ge
#undef gpga_double_is_zero

gpga_double gpga_bits_to_real(ulong bits) {
  return gpga_real_impl_bits_to_real(bits);
}

ulong gpga_real_to_bits(gpga_double value) {
  return gpga_real_impl_real_to_bits(value);
}

gpga_double gpga_double_from_u64(ulong value) {
  return gpga_real_impl_double_from_u64(value);
}

gpga_double gpga_double_from_s64(long value) {
  return gpga_real_impl_double_from_s64(value);
}

gpga_double gpga_double_from_u32(uint value) {
  return gpga_real_impl_double_from_u32(value);
}

gpga_double gpga_double_from_s32(int value) {
  return gpga_real_impl_double_from_s32(value);
}

long gpga_double_to_s64(gpga_double value) {
  return gpga_real_impl_double_to_s64(value);
}

gpga_double gpga_double_neg(gpga_double value) {
  return gpga_real_impl_double_neg(value);
}

gpga_double gpga_double_add(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_add(a, b);
}

gpga_double gpga_double_sub(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_sub(a, b);
}

gpga_double gpga_double_mul(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_mul(a, b);
}

gpga_double gpga_double_div(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_div(a, b);
}

gpga_double gpga_double_pow(gpga_double base, gpga_double exp) {
  return gpga_real_impl_double_pow(base, exp);
}

gpga_double gpga_double_log10(gpga_double value) {
  return gpga_real_impl_double_log10(value);
}

gpga_double gpga_double_ln(gpga_double value) {
  return gpga_real_impl_double_ln(value);
}

gpga_double gpga_double_exp_real(gpga_double value) {
  return gpga_real_impl_double_exp_real(value);
}

gpga_double gpga_double_sqrt(gpga_double value) {
  return gpga_real_impl_double_sqrt(value);
}

gpga_double gpga_double_floor(gpga_double value) {
  return gpga_real_impl_double_floor(value);
}

gpga_double gpga_double_ceil(gpga_double value) {
  return gpga_real_impl_double_ceil(value);
}

gpga_double gpga_double_sin(gpga_double value) {
  return gpga_real_impl_double_sin(value);
}

gpga_double gpga_double_cos(gpga_double value) {
  return gpga_real_impl_double_cos(value);
}

gpga_double gpga_double_tan(gpga_double value) {
  return gpga_real_impl_double_tan(value);
}

gpga_double gpga_double_asin(gpga_double value) {
  return gpga_real_impl_double_asin(value);
}

gpga_double gpga_double_acos(gpga_double value) {
  return gpga_real_impl_double_acos(value);
}

gpga_double gpga_double_atan(gpga_double value) {
  return gpga_real_impl_double_atan(value);
}

bool gpga_double_eq(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_eq(a, b);
}

bool gpga_double_lt(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_lt(a, b);
}

bool gpga_double_gt(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_gt(a, b);
}

bool gpga_double_le(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_le(a, b);
}

bool gpga_double_ge(gpga_double a, gpga_double b) {
  return gpga_real_impl_double_ge(a, b);
}

bool gpga_double_is_zero(gpga_double value) {
  return gpga_real_impl_double_is_zero(value);
}
