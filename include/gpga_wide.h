#ifndef GPGA_WIDE_H
#define GPGA_WIDE_H

#if defined(__METAL_VERSION__)
#include <metal_stdlib>
using namespace metal;
#else
#include <cstdint>
// Fallback typedefs for editors/non-Metal tooling.
typedef uint32_t uint;
typedef uint64_t ulong;
static inline uint popcount(uint value) { return __builtin_popcount(value); }
#endif

// Wide (>64-bit) helper generator for MSL.
#define GPGA_WIDE_DEFINE(width, words, last_mask) \
struct GpgaWide##width { ulong w[words]; }; \
inline GpgaWide##width gpga_wide_zero_##width() { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = 0ul; } \
  return out; \
} \
inline GpgaWide##width gpga_wide_mask_const_##width() { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = 0xFFFFFFFFFFFFFFFFul; } \
  out.w[words - 1] = last_mask; \
  return out; \
} \
inline GpgaWide##width gpga_wide_from_u64_##width(ulong value) { \
  GpgaWide##width out = gpga_wide_zero_##width(); \
  out.w[0] = value; \
  return out; \
} \
inline GpgaWide##width gpga_wide_mask_##width(GpgaWide##width v) { \
  v.w[words - 1] &= last_mask; \
  return v; \
} \
inline ulong gpga_wide_to_u64_##width(GpgaWide##width v) { \
  return v.w[0]; \
} \
inline bool gpga_wide_any_##width(GpgaWide##width v) { \
  for (uint i = 0u; i < words; ++i) { \
    ulong word = v.w[i]; \
    if (i == words - 1u) { \
      word &= last_mask; \
    } \
    if (word != 0ul) return true; \
  } \
  return false; \
} \
inline GpgaWide##width gpga_wide_select_##width(bool cond, GpgaWide##width a, GpgaWide##width b) { \
  if (cond) return a; \
  return b; \
} \
inline uint gpga_wide_get_bit_##width(GpgaWide##width v, uint idx) { \
  if (idx >= width##u) return 0u; \
  uint word = idx >> 6u; \
  uint bit = idx & 63u; \
  return uint((v.w[word] >> bit) & 1ul); \
} \
inline GpgaWide##width gpga_wide_set_bit_##width(GpgaWide##width v, uint idx, uint bit) { \
  if (idx >= width##u) return v; \
  uint word = idx >> 6u; \
  uint shift = idx & 63u; \
  ulong mask = (1ul << shift); \
  if (bit != 0u) { \
    v.w[word] |= mask; \
  } else { \
    v.w[word] &= ~mask; \
  } \
  return v; \
} \
inline GpgaWide##width gpga_wide_set_word_##width(GpgaWide##width v, uint word, ulong value) { \
  if (word >= words) return v; \
  v.w[word] = value; \
  if (word == words - 1u) { \
    v.w[word] &= last_mask; \
  } \
  return v; \
} \
inline uint gpga_wide_signbit_##width(GpgaWide##width v) { \
  return gpga_wide_get_bit_##width(v, width - 1u); \
} \
inline GpgaWide##width gpga_wide_not_##width(GpgaWide##width a) { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = ~a.w[i]; } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_and_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = a.w[i] & b.w[i]; } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_or_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = a.w[i] | b.w[i]; } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_xor_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width out; \
  for (uint i = 0u; i < words; ++i) { out.w[i] = a.w[i] ^ b.w[i]; } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_add_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width out; \
  ulong carry = 0ul; \
  for (uint i = 0u; i < words; ++i) { \
    ulong ai = a.w[i]; \
    ulong bi = b.w[i]; \
    ulong sum = ai + bi; \
    ulong carry1 = (sum < ai) ? 1ul : 0ul; \
    sum += carry; \
    ulong carry2 = (sum < carry) ? 1ul : 0ul; \
    carry = (carry1 | carry2); \
    out.w[i] = sum; \
  } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_sub_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width out; \
  ulong borrow = 0ul; \
  for (uint i = 0u; i < words; ++i) { \
    ulong ai = a.w[i]; \
    ulong bi = b.w[i]; \
    ulong diff = ai - bi; \
    ulong borrow1 = (ai < bi) ? 1ul : 0ul; \
    ulong diff2 = diff - borrow; \
    ulong borrow2 = (diff < borrow) ? 1ul : 0ul; \
    borrow = (borrow1 | borrow2); \
    out.w[i] = diff2; \
  } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_shl_##width(GpgaWide##width a, uint shift) { \
  if (shift >= width##u) return gpga_wide_zero_##width(); \
  uint word_shift = shift >> 6u; \
  uint bit_shift = shift & 63u; \
  GpgaWide##width out = gpga_wide_zero_##width(); \
  for (int i = int(words) - 1; i >= 0; --i) { \
    int src = i - int(word_shift); \
    if (src < 0) continue; \
    ulong val = a.w[src] << bit_shift; \
    if (bit_shift != 0u && src > 0) { \
      val |= (a.w[src - 1] >> (64u - bit_shift)); \
    } \
    out.w[i] = val; \
  } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_shr_##width(GpgaWide##width a, uint shift) { \
  if (shift >= width##u) return gpga_wide_zero_##width(); \
  uint word_shift = shift >> 6u; \
  uint bit_shift = shift & 63u; \
  GpgaWide##width out = gpga_wide_zero_##width(); \
  for (uint i = 0u; i < words; ++i) { \
    uint src = i + word_shift; \
    if (src >= words) continue; \
    ulong val = a.w[src] >> bit_shift; \
    if (bit_shift != 0u && (src + 1u) < words) { \
      val |= (a.w[src + 1u] << (64u - bit_shift)); \
    } \
    out.w[i] = val; \
  } \
  return gpga_wide_mask_##width(out); \
} \
inline GpgaWide##width gpga_wide_sar_##width(GpgaWide##width a, uint shift) { \
  uint sign = gpga_wide_signbit_##width(a); \
  if (shift >= width##u) { \
    return sign ? gpga_wide_mask_const_##width() : gpga_wide_zero_##width(); \
  } \
  GpgaWide##width out = gpga_wide_shr_##width(a, shift); \
  if (sign == 0u || shift == 0u) { \
    return out; \
  } \
  uint fill_start = width##u - shift; \
  uint word = fill_start >> 6u; \
  uint bit = fill_start & 63u; \
  for (uint i = word + 1u; i < words; ++i) { \
    out.w[i] = 0xFFFFFFFFFFFFFFFFul; \
  } \
  if (word < words) { \
    ulong mask = (bit == 0u) ? 0xFFFFFFFFFFFFFFFFul : (0xFFFFFFFFFFFFFFFFul << bit); \
    out.w[word] |= mask; \
  } \
  return gpga_wide_mask_##width(out); \
} \
inline bool gpga_wide_eq_##width(GpgaWide##width a, GpgaWide##width b) { \
  for (uint i = 0u; i < words; ++i) { \
    ulong aw = a.w[i]; \
    ulong bw = b.w[i]; \
    if (i == words - 1u) { \
      aw &= last_mask; \
      bw &= last_mask; \
    } \
    if (aw != bw) return false; \
  } \
  return true; \
} \
inline bool gpga_wide_ne_##width(GpgaWide##width a, GpgaWide##width b) { \
  return !gpga_wide_eq_##width(a, b); \
} \
inline bool gpga_wide_lt_u_##width(GpgaWide##width a, GpgaWide##width b) { \
  for (int i = int(words) - 1; i >= 0; --i) { \
    ulong aw = a.w[i]; \
    ulong bw = b.w[i]; \
    if (i == int(words) - 1) { \
      aw &= last_mask; \
      bw &= last_mask; \
    } \
    if (aw < bw) return true; \
    if (aw > bw) return false; \
  } \
  return false; \
} \
inline bool gpga_wide_gt_u_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_u_##width(b, a); \
} \
inline bool gpga_wide_le_u_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_u_##width(a, b) || gpga_wide_eq_##width(a, b); \
} \
inline bool gpga_wide_ge_u_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_u_##width(b, a) || gpga_wide_eq_##width(a, b); \
} \
inline bool gpga_wide_lt_s_##width(GpgaWide##width a, GpgaWide##width b) { \
  uint sa = gpga_wide_signbit_##width(a); \
  uint sb = gpga_wide_signbit_##width(b); \
  if (sa != sb) return sa > sb; \
  return gpga_wide_lt_u_##width(a, b); \
} \
inline bool gpga_wide_gt_s_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_s_##width(b, a); \
} \
inline bool gpga_wide_le_s_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_s_##width(a, b) || gpga_wide_eq_##width(a, b); \
} \
inline bool gpga_wide_ge_s_##width(GpgaWide##width a, GpgaWide##width b) { \
  return gpga_wide_lt_s_##width(b, a) || gpga_wide_eq_##width(a, b); \
} \
inline uint gpga_wide_red_and_##width(GpgaWide##width a) { \
  return gpga_wide_eq_##width(gpga_wide_mask_##width(a), gpga_wide_mask_const_##width()) ? 1u : 0u; \
} \
inline uint gpga_wide_red_or_##width(GpgaWide##width a) { \
  return gpga_wide_any_##width(a) ? 1u : 0u; \
} \
inline uint gpga_wide_red_xor_##width(GpgaWide##width a) { \
  uint parity = 0u; \
  for (uint i = 0u; i < words; ++i) { \
    ulong word = a.w[i]; \
    if (i == words - 1u) { \
      word &= last_mask; \
    } \
    uint lo = uint(word); \
    uint hi = uint(word >> 32u); \
    parity ^= (popcount(lo) + popcount(hi)) & 1u; \
  } \
  return parity & 1u; \
} \
inline GpgaWide##width gpga_wide_mul_##width(GpgaWide##width a, GpgaWide##width b) { \
  GpgaWide##width result = gpga_wide_zero_##width(); \
  GpgaWide##width temp = a; \
  for (uint bit = 0u; bit < width##u; ++bit) { \
    if (gpga_wide_get_bit_##width(b, bit)) { \
      result = gpga_wide_add_##width(result, temp); \
    } \
    temp = gpga_wide_shl_##width(temp, 1u); \
  } \
  return result; \
} \
inline GpgaWide##width gpga_wide_div_##width(GpgaWide##width num, GpgaWide##width den) { \
  if (!gpga_wide_any_##width(den)) return gpga_wide_zero_##width(); \
  GpgaWide##width quotient = gpga_wide_zero_##width(); \
  GpgaWide##width rem = gpga_wide_zero_##width(); \
  for (int bit = int(width) - 1; bit >= 0; --bit) { \
    rem = gpga_wide_shl_##width(rem, 1u); \
    if (gpga_wide_get_bit_##width(num, uint(bit)) != 0u) { \
      rem = gpga_wide_set_bit_##width(rem, 0u, 1u); \
    } \
    if (!gpga_wide_lt_u_##width(rem, den)) { \
      rem = gpga_wide_sub_##width(rem, den); \
      quotient = gpga_wide_set_bit_##width(quotient, uint(bit), 1u); \
    } \
  } \
  return quotient; \
} \
inline GpgaWide##width gpga_wide_mod_##width(GpgaWide##width num, GpgaWide##width den) { \
  if (!gpga_wide_any_##width(den)) return gpga_wide_zero_##width(); \
  GpgaWide##width rem = gpga_wide_zero_##width(); \
  for (int bit = int(width) - 1; bit >= 0; --bit) { \
    rem = gpga_wide_shl_##width(rem, 1u); \
    if (gpga_wide_get_bit_##width(num, uint(bit)) != 0u) { \
      rem = gpga_wide_set_bit_##width(rem, 0u, 1u); \
    } \
    if (!gpga_wide_lt_u_##width(rem, den)) { \
      rem = gpga_wide_sub_##width(rem, den); \
    } \
  } \
  return rem; \
} \
inline GpgaWide##width gpga_wide_pow_u_##width(GpgaWide##width base, GpgaWide##width exp) { \
  ulong exp_u = gpga_wide_to_u64_##width(exp); \
  GpgaWide##width result = gpga_wide_from_u64_##width(1ul); \
  GpgaWide##width cur = base; \
  while (exp_u != 0ul) { \
    if (exp_u & 1ul) { \
      result = gpga_wide_mul_##width(result, cur); \
    } \
    cur = gpga_wide_mul_##width(cur, cur); \
    exp_u >>= 1ul; \
  } \
  return result; \
} \
inline GpgaWide##width gpga_wide_pow_s_##width(GpgaWide##width base, GpgaWide##width exp) { \
  long exp_s = (long)gpga_wide_to_u64_##width(exp); \
  if (exp_s < 0) return gpga_wide_zero_##width(); \
  return gpga_wide_pow_u_##width(base, exp); \
} \
inline GpgaWide##width gpga_wide_sext_from_u64_##width(ulong value, uint src_width) { \
  if (src_width == 0u) return gpga_wide_zero_##width(); \
  GpgaWide##width out = gpga_wide_zero_##width(); \
  out.w[0] = value; \
  if (src_width < 64u) { \
    ulong sign_mask = 1ul << (src_width - 1u); \
    if ((value & sign_mask) != 0ul) { \
      ulong upper = ~((1ul << src_width) - 1ul); \
      out.w[0] |= upper; \
      for (uint i = 1u; i < words; ++i) { out.w[i] = 0xFFFFFFFFFFFFFFFFul; } \
    } \
  } \
  return gpga_wide_mask_##width(out); \
}

#define GPGA_WIDE_DEFINE_RESIZE(dst, src, dst_words, src_words, dst_last_mask, src_mod) \
inline GpgaWide##dst gpga_wide_resize_##dst##_from_##src(GpgaWide##src v) { \
  GpgaWide##dst out = gpga_wide_zero_##dst(); \
  uint count = (dst_words < src_words) ? dst_words : src_words; \
  for (uint i = 0u; i < count; ++i) { out.w[i] = v.w[i]; } \
  out.w[dst_words - 1] &= dst_last_mask; \
  return out; \
} \
inline GpgaWide##dst gpga_wide_sext_##dst##_from_##src(GpgaWide##src v) { \
  if (dst <= src) { \
    return gpga_wide_resize_##dst##_from_##src(v); \
  } \
  GpgaWide##dst out = gpga_wide_resize_##dst##_from_##src(v); \
  uint sign = gpga_wide_get_bit_##src(v, src - 1u); \
  if (sign != 0u) { \
    if (src_mod != 0) { \
      ulong upper_mask = ~((1ul << src_mod) - 1ul); \
      out.w[src_words - 1] |= upper_mask; \
    } \
    for (uint i = src_words; i < dst_words; ++i) { out.w[i] = 0xFFFFFFFFFFFFFFFFul; } \
  } \
  out.w[dst_words - 1] &= dst_last_mask; \
  return out; \
}

#define GPGA_WIDE_DEFINE_FS(width) \
struct GpgaWideFs##width { GpgaWide##width val; GpgaWide##width xz; };

#endif  // GPGA_WIDE_H
