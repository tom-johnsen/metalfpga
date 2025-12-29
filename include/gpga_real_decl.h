#ifndef GPGA_REAL_DECL_H
#define GPGA_REAL_DECL_H

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

typedef ulong gpga_double;

gpga_double gpga_bits_to_real(ulong bits);
ulong gpga_real_to_bits(gpga_double value);

gpga_double gpga_double_from_u64(ulong value);
gpga_double gpga_double_from_s64(long value);
gpga_double gpga_double_from_u32(uint value);
gpga_double gpga_double_from_s32(int value);
long gpga_double_to_s64(gpga_double value);

gpga_double gpga_double_neg(gpga_double value);
gpga_double gpga_double_add(gpga_double a, gpga_double b);
gpga_double gpga_double_sub(gpga_double a, gpga_double b);
gpga_double gpga_double_mul(gpga_double a, gpga_double b);
gpga_double gpga_double_div(gpga_double a, gpga_double b);
gpga_double gpga_double_pow(gpga_double base, gpga_double exp);

gpga_double gpga_double_log10(gpga_double value);
gpga_double gpga_double_ln(gpga_double value);
gpga_double gpga_double_exp_real(gpga_double value);
gpga_double gpga_double_sqrt(gpga_double value);

gpga_double gpga_double_floor(gpga_double value);
gpga_double gpga_double_ceil(gpga_double value);

gpga_double gpga_double_sin(gpga_double value);
gpga_double gpga_double_cos(gpga_double value);
gpga_double gpga_double_tan(gpga_double value);
gpga_double gpga_double_asin(gpga_double value);
gpga_double gpga_double_acos(gpga_double value);
gpga_double gpga_double_atan(gpga_double value);

bool gpga_double_eq(gpga_double a, gpga_double b);
bool gpga_double_lt(gpga_double a, gpga_double b);
bool gpga_double_gt(gpga_double a, gpga_double b);
bool gpga_double_le(gpga_double a, gpga_double b);
bool gpga_double_ge(gpga_double a, gpga_double b);
bool gpga_double_is_zero(gpga_double value);

#endif  // GPGA_REAL_DECL_H
