#ifndef GPGA_4STATE_H
#define GPGA_4STATE_H

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

// Four-state helpers for MSL.
struct FourState32 { uint val; uint xz; };
struct FourState64 { ulong val; ulong xz; };
inline int fs_sign32(uint val, uint width);
inline long fs_sign64(ulong val, uint width);
inline uint fs_mask32(uint width) {
  return (width >= 32u) ? 0xFFFFFFFFu : ((1u << width) - 1u);
}
inline ulong fs_mask64(uint width) {
  return (width >= 64u) ? 0xFFFFFFFFFFFFFFFFul : ((1ul << width) - 1ul);
}
inline FourState32 fs_make32(uint val, uint xz, uint width) {
  uint mask = fs_mask32(width);
  FourState32 out = {val & mask, xz & mask};
  return out;
}
inline FourState64 fs_make64(ulong val, ulong xz, uint width) {
  ulong mask = fs_mask64(width);
  FourState64 out = {val & mask, xz & mask};
  return out;
}
inline FourState32 fs_allx32(uint width) {
  uint mask = fs_mask32(width);
  FourState32 out = {0u, mask};
  return out;
}
inline FourState64 fs_allx64(uint width) {
  ulong mask = fs_mask64(width);
  FourState64 out = {0ul, mask};
  return out;
}
inline FourState32 fs_resize32(FourState32 a, uint width) {
  return fs_make32(a.val, a.xz, width);
}
inline FourState64 fs_resize64(FourState64 a, uint width) {
  return fs_make64(a.val, a.xz, width);
}
inline FourState64 fs_resize64(FourState32 a, uint width) {
  FourState64 widened = {static_cast<ulong>(a.val), static_cast<ulong>(a.xz)};
  return fs_resize64(widened, width);
}
inline FourState32 fs_sext32(FourState32 a, uint src_width, uint target_width) {
  if (target_width == 0u || src_width == 0u) return fs_make32(0u, 0u, target_width);
  if (target_width <= src_width) return fs_make32(a.val, a.xz, target_width);
  uint src_mask = fs_mask32(src_width);
  uint tgt_mask = fs_mask32(target_width);
  uint val = a.val & src_mask;
  uint xz = a.xz & src_mask;
  uint sign_mask = 1u << (src_width - 1u);
  uint sign_xz = xz & sign_mask;
  uint sign_val = val & sign_mask;
  uint ext_mask = tgt_mask & ~src_mask;
  uint ext_val = sign_val ? ext_mask : 0u;
  uint ext_xz = sign_xz ? ext_mask : 0u;
  return fs_make32(val | ext_val, xz | ext_xz, target_width);
}
inline FourState64 fs_sext64(FourState64 a, uint src_width, uint target_width) {
  if (target_width == 0u || src_width == 0u) return fs_make64(0ul, 0ul, target_width);
  if (target_width <= src_width) return fs_make64(a.val, a.xz, target_width);
  ulong src_mask = fs_mask64(src_width);
  ulong tgt_mask = fs_mask64(target_width);
  ulong val = a.val & src_mask;
  ulong xz = a.xz & src_mask;
  ulong sign_mask = 1ul << (src_width - 1u);
  ulong sign_xz = xz & sign_mask;
  ulong sign_val = val & sign_mask;
  ulong ext_mask = tgt_mask & ~src_mask;
  ulong ext_val = sign_val ? ext_mask : 0ul;
  ulong ext_xz = sign_xz ? ext_mask : 0ul;
  return fs_make64(val | ext_val, xz | ext_xz, target_width);
}
inline FourState32 fs_merge32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  uint ak = (~ax) & mask;
  uint bk = (~bx) & mask;
  uint same = ~(a.val ^ b.val) & ak & bk & mask;
  FourState32 out = {a.val & same, mask & ~same};
  return out;
}
inline FourState64 fs_merge64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  ulong ak = (~ax) & mask;
  ulong bk = (~bx) & mask;
  ulong same = ~(a.val ^ b.val) & ak & bk & mask;
  FourState64 out = {a.val & same, mask & ~same};
  return out;
}
inline FourState32 fs_not32(FourState32 a, uint width) {
  uint mask = fs_mask32(width);
  FourState32 out = {(~a.val) & mask, a.xz & mask};
  return out;
}
inline FourState64 fs_not64(FourState64 a, uint width) {
  ulong mask = fs_mask64(width);
  FourState64 out = {(~a.val) & mask, a.xz & mask};
  return out;
}
inline FourState32 fs_and32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  uint a0 = (~a.val) & ~ax & mask;
  uint b0 = (~b.val) & ~bx & mask;
  uint a1 = a.val & ~ax & mask;
  uint b1 = b.val & ~bx & mask;
  uint known0 = a0 | b0;
  uint known1 = a1 & b1;
  uint unknown = mask & ~(known0 | known1);
  FourState32 out = {known1, unknown};
  return out;
}
inline FourState64 fs_and64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  ulong a0 = (~a.val) & ~ax & mask;
  ulong b0 = (~b.val) & ~bx & mask;
  ulong a1 = a.val & ~ax & mask;
  ulong b1 = b.val & ~bx & mask;
  ulong known0 = a0 | b0;
  ulong known1 = a1 & b1;
  ulong unknown = mask & ~(known0 | known1);
  FourState64 out = {known1, unknown};
  return out;
}
inline FourState32 fs_or32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  uint a0 = (~a.val) & ~ax & mask;
  uint b0 = (~b.val) & ~bx & mask;
  uint a1 = a.val & ~ax & mask;
  uint b1 = b.val & ~bx & mask;
  uint known1 = a1 | b1;
  uint known0 = a0 & b0;
  uint unknown = mask & ~(known0 | known1);
  FourState32 out = {known1, unknown};
  return out;
}
inline FourState64 fs_or64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  ulong a0 = (~a.val) & ~ax & mask;
  ulong b0 = (~b.val) & ~bx & mask;
  ulong a1 = a.val & ~ax & mask;
  ulong b1 = b.val & ~bx & mask;
  ulong known1 = a1 | b1;
  ulong known0 = a0 & b0;
  ulong unknown = mask & ~(known0 | known1);
  FourState64 out = {known1, unknown};
  return out;
}
inline FourState32 fs_xor32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint unknown = (a.xz | b.xz) & mask;
  FourState32 out = {(a.val ^ b.val) & ~unknown & mask, unknown};
  return out;
}
inline FourState64 fs_xor64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong unknown = (a.xz | b.xz) & mask;
  FourState64 out = {(a.val ^ b.val) & ~unknown & mask, unknown};
  return out;
}
inline FourState32 fs_add32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  return fs_make32(a.val + b.val, 0u, width);
}
inline FourState64 fs_add64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  return fs_make64(a.val + b.val, 0ul, width);
}
inline FourState32 fs_sub32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  return fs_make32(a.val - b.val, 0u, width);
}
inline FourState64 fs_sub64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  return fs_make64(a.val - b.val, 0ul, width);
}
inline FourState32 fs_mul32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  return fs_make32(a.val * b.val, 0u, width);
}
inline FourState64 fs_mul64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  return fs_make64(a.val * b.val, 0ul, width);
}
inline FourState32 fs_pow32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  uint mask = fs_mask32(width);
  uint base = a.val & mask;
  uint exp = b.val & mask;
  uint result = 1u;
  while (exp != 0u) {
    if (exp & 1u) {
      result *= base;
    }
    base *= base;
    exp >>= 1u;
  }
  return fs_make32(result, 0u, width);
}
inline FourState64 fs_pow64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  ulong mask = fs_mask64(width);
  ulong base = a.val & mask;
  ulong exp = b.val & mask;
  ulong result = 1ul;
  while (exp != 0ul) {
    if (exp & 1ul) {
      result *= base;
    }
    base *= base;
    exp >>= 1ul;
  }
  return fs_make64(result, 0ul, width);
}
inline FourState32 fs_spow32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  uint mask = fs_mask32(width);
  int exp = fs_sign32(b.val & mask, width);
  if (exp < 0) return fs_make32(0u, 0u, width);
  uint base = a.val & mask;
  uint result = 1u;
  uint exp_u = uint(exp);
  while (exp_u != 0u) {
    if (exp_u & 1u) {
      result *= base;
    }
    base *= base;
    exp_u >>= 1u;
  }
  return fs_make32(result, 0u, width);
}
inline FourState64 fs_spow64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  ulong mask = fs_mask64(width);
  long exp = fs_sign64(b.val & mask, width);
  if (exp < 0) return fs_make64(0ul, 0ul, width);
  ulong base = a.val & mask;
  ulong result = 1ul;
  ulong exp_u = ulong(exp);
  while (exp_u != 0ul) {
    if (exp_u & 1ul) {
      result *= base;
    }
    base *= base;
    exp_u >>= 1ul;
  }
  return fs_make64(result, 0ul, width);
}
inline FourState32 fs_div32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u || b.val == 0u) return fs_allx32(width);
  return fs_make32(a.val / b.val, 0u, width);
}
inline FourState64 fs_div64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul || b.val == 0ul) return fs_allx64(width);
  return fs_make64(a.val / b.val, 0ul, width);
}
inline FourState32 fs_mod32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u || b.val == 0u) return fs_allx32(width);
  return fs_make32(a.val % b.val, 0u, width);
}
inline FourState64 fs_mod64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul || b.val == 0ul) return fs_allx64(width);
  return fs_make64(a.val % b.val, 0ul, width);
}
inline FourState32 fs_cmp32(uint value, bool pred) {
  FourState32 out = {pred ? 1u : 0u, 0u};
  return out;
}
inline FourState64 fs_cmp64(ulong value, bool pred) {
  FourState64 out = {pred ? 1ul : 0ul, 0ul};
  return out;
}
inline FourState32 fs_eq32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val == b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_eq64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val == b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_ne32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val != b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_ne64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val != b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_lt32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val < b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_lt64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val < b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_gt32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val > b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_gt64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val > b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_le32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val <= b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_le64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val <= b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_ge32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  return fs_make32((a.val >= b.val) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_ge64(FourState64 a, FourState64 b, uint width) {
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  return fs_make64((a.val >= b.val) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_shl32(FourState32 a, FourState32 b, uint width) {
  if (b.xz != 0u) return fs_allx32(width);
  uint mask = fs_mask32(width);
  uint shift = b.val;
  if (shift >= width) return fs_make32(0u, 0u, width);
  FourState32 out = {(a.val << shift) & mask, (a.xz << shift) & mask};
  return out;
}
inline FourState64 fs_shl64(FourState64 a, FourState64 b, uint width) {
  if (b.xz != 0ul) return fs_allx64(width);
  ulong mask = fs_mask64(width);
  ulong shift = b.val;
  if (shift >= width) return fs_make64(0ul, 0ul, width);
  FourState64 out = {(a.val << shift) & mask, (a.xz << shift) & mask};
  return out;
}
inline FourState32 fs_shr32(FourState32 a, FourState32 b, uint width) {
  if (b.xz != 0u) return fs_allx32(width);
  uint mask = fs_mask32(width);
  uint shift = b.val;
  if (shift >= width) return fs_make32(0u, 0u, width);
  FourState32 out = {(a.val >> shift) & mask, (a.xz >> shift) & mask};
  return out;
}
inline FourState64 fs_shr64(FourState64 a, FourState64 b, uint width) {
  if (b.xz != 0ul) return fs_allx64(width);
  ulong mask = fs_mask64(width);
  ulong shift = b.val;
  if (shift >= width) return fs_make64(0ul, 0ul, width);
  FourState64 out = {(a.val >> shift) & mask, (a.xz >> shift) & mask};
  return out;
}
inline FourState32 fs_mux32(FourState32 cond, FourState32 t, FourState32 f, uint width) {
  if (cond.xz != 0u) return fs_merge32(t, f, width);
  return (cond.val != 0u) ? fs_resize32(t, width) : fs_resize32(f, width);
}
inline FourState64 fs_mux64(FourState64 cond, FourState64 t, FourState64 f, uint width) {
  if (cond.xz != 0ul) return fs_merge64(t, f, width);
  return (cond.val != 0ul) ? fs_resize64(t, width) : fs_resize64(f, width);
}

inline FourState32 fs_red_and32(FourState32 a, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint a0 = (~a.val) & ~ax & mask;
  uint a1 = a.val & ~ax & mask;
  if (a0 != 0u) return fs_make32(0u, 0u, 1u);
  if (a1 == mask) return fs_make32(1u, 0u, 1u);
  return fs_allx32(1u);
}
inline FourState64 fs_red_and64(FourState64 a, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong a0 = (~a.val) & ~ax & mask;
  ulong a1 = a.val & ~ax & mask;
  if (a0 != 0ul) return fs_make64(0ul, 0ul, 1u);
  if (a1 == mask) return fs_make64(1ul, 0ul, 1u);
  return fs_allx64(1u);
}
inline FourState32 fs_red_or32(FourState32 a, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint a0 = (~a.val) & ~ax & mask;
  uint a1 = a.val & ~ax & mask;
  if (a1 != 0u) return fs_make32(1u, 0u, 1u);
  if (a0 == mask) return fs_make32(0u, 0u, 1u);
  return fs_allx32(1u);
}
inline FourState64 fs_red_or64(FourState64 a, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong a0 = (~a.val) & ~ax & mask;
  ulong a1 = a.val & ~ax & mask;
  if (a1 != 0ul) return fs_make64(1ul, 0ul, 1u);
  if (a0 == mask) return fs_make64(0ul, 0ul, 1u);
  return fs_allx64(1u);
}
inline FourState32 fs_red_xor32(FourState32 a, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz & mask) != 0u) return fs_allx32(1u);
  uint parity = popcount(a.val & mask) & 1u;
  return fs_make32(parity, 0u, 1u);
}
inline FourState64 fs_red_xor64(FourState64 a, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz & mask) != 0ul) return fs_allx64(1u);
  ulong val = a.val & mask;
  uint lo = uint(val);
  uint hi = uint(val >> 32u);
  uint parity = (popcount(lo) + popcount(hi)) & 1u;
  return fs_make64(ulong(parity), 0ul, 1u);
}

inline int fs_sign32(uint val, uint width) {
  if (width >= 32u) return int(val);
  uint shift = 32u - width;
  return int(val << shift) >> shift;
}
inline long fs_sign64(ulong val, uint width) {
  if (width >= 64u) return long(val);
  uint shift = 64u - width;
  return long(val << shift) >> shift;
}
inline FourState32 fs_slt32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  return fs_make32((sa < sb) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_slt64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  return fs_make64((sa < sb) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_sle32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  return fs_make32((sa <= sb) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_sle64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  return fs_make64((sa <= sb) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_sgt32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  return fs_make32((sa > sb) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_sgt64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  return fs_make64((sa > sb) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_sge32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(1u);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  return fs_make32((sa >= sb) ? 1u : 0u, 0u, 1u);
}
inline FourState64 fs_sge64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(1u);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  return fs_make64((sa >= sb) ? 1ul : 0ul, 0ul, 1u);
}
inline FourState32 fs_sdiv32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  if (sb == 0) return fs_allx32(width);
  int res = sa / sb;
  return fs_make32(uint(res), 0u, width);
}
inline FourState64 fs_sdiv64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  if (sb == 0) return fs_allx64(width);
  long res = sa / sb;
  return fs_make64(ulong(res), 0ul, width);
}
inline FourState32 fs_smod32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  int sa = fs_sign32(a.val & mask, width);
  int sb = fs_sign32(b.val & mask, width);
  if (sb == 0) return fs_allx32(width);
  int res = sa % sb;
  return fs_make32(uint(res), 0u, width);
}
inline FourState64 fs_smod64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if ((a.xz | b.xz) != 0ul) return fs_allx64(width);
  long sa = fs_sign64(a.val & mask, width);
  long sb = fs_sign64(b.val & mask, width);
  if (sb == 0) return fs_allx64(width);
  long res = sa % sb;
  return fs_make64(ulong(res), 0ul, width);
}
inline FourState32 fs_sar32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  if (b.xz != 0u) return fs_allx32(width);
  uint shift = b.val;
  if (width == 0u) return fs_make32(0u, 0u, 0u);
  uint sign_mask = 1u << (width - 1u);
  if ((a.xz & sign_mask) != 0u) return fs_allx32(width);
  uint sign = (a.val & sign_mask) ? mask : 0u;
  if (shift >= width) return fs_make32(sign, 0u, width);
  uint fill_mask = (shift == 0u) ? 0u : (~0u << (width - shift));
  uint shifted_val = (a.val >> shift) | (sign & fill_mask);
  uint shifted_xz = (a.xz >> shift) & mask;
  return fs_make32(shifted_val, shifted_xz, width);
}
inline FourState64 fs_sar64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  if (b.xz != 0ul) return fs_allx64(width);
  ulong shift = b.val;
  if (width == 0u) return fs_make64(0ul, 0ul, 0u);
  ulong sign_mask = 1ul << (width - 1u);
  if ((a.xz & sign_mask) != 0ul) return fs_allx64(width);
  ulong sign = (a.val & sign_mask) ? mask : 0ul;
  if (shift >= width) return fs_make64(sign, 0ul, width);
  ulong fill_mask = (shift == 0u) ? 0ul : (~0ul << (width - shift));
  ulong shifted_val = (a.val >> shift) | (sign & fill_mask);
  ulong shifted_xz = (a.xz >> shift) & mask;
  return fs_make64(shifted_val, shifted_xz, width);
}

inline FourState32 fs_log_not32(FourState32 a, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint known1 = a.val & ~ax & mask;
  if (known1 != 0u) return fs_make32(0u, 0u, 1u);
  if (ax == 0u && (a.val & mask) == 0u) return fs_make32(1u, 0u, 1u);
  return fs_allx32(1u);
}
inline FourState64 fs_log_not64(FourState64 a, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong known1 = a.val & ~ax & mask;
  if (known1 != 0ul) return fs_make64(0ul, 0ul, 1u);
  if (ax == 0ul && (a.val & mask) == 0ul) return fs_make64(1ul, 0ul, 1u);
  return fs_allx64(1u);
}
inline FourState32 fs_log_and32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  uint a_known1 = a.val & ~ax & mask;
  uint b_known1 = b.val & ~bx & mask;
  bool a_true = a_known1 != 0u;
  bool b_true = b_known1 != 0u;
  bool a_false = (ax == 0u && (a.val & mask) == 0u);
  bool b_false = (bx == 0u && (b.val & mask) == 0u);
  if (a_false || b_false) return fs_make32(0u, 0u, 1u);
  if (a_true && b_true) return fs_make32(1u, 0u, 1u);
  return fs_allx32(1u);
}
inline FourState64 fs_log_and64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  ulong a_known1 = a.val & ~ax & mask;
  ulong b_known1 = b.val & ~bx & mask;
  bool a_true = a_known1 != 0ul;
  bool b_true = b_known1 != 0ul;
  bool a_false = (ax == 0ul && (a.val & mask) == 0ul);
  bool b_false = (bx == 0ul && (b.val & mask) == 0ul);
  if (a_false || b_false) return fs_make64(0ul, 0ul, 1u);
  if (a_true && b_true) return fs_make64(1ul, 0ul, 1u);
  return fs_allx64(1u);
}
inline FourState32 fs_log_or32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  uint a_known1 = a.val & ~ax & mask;
  uint b_known1 = b.val & ~bx & mask;
  bool a_true = a_known1 != 0u;
  bool b_true = b_known1 != 0u;
  bool a_false = (ax == 0u && (a.val & mask) == 0u);
  bool b_false = (bx == 0u && (b.val & mask) == 0u);
  if (a_true || b_true) return fs_make32(1u, 0u, 1u);
  if (a_false && b_false) return fs_make32(0u, 0u, 1u);
  return fs_allx32(1u);
}
inline FourState64 fs_log_or64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  ulong a_known1 = a.val & ~ax & mask;
  ulong b_known1 = b.val & ~bx & mask;
  bool a_true = a_known1 != 0ul;
  bool b_true = b_known1 != 0ul;
  bool a_false = (ax == 0ul && (a.val & mask) == 0ul);
  bool b_false = (bx == 0ul && (b.val & mask) == 0ul);
  if (a_true || b_true) return fs_make64(1ul, 0ul, 1u);
  if (a_false && b_false) return fs_make64(0ul, 0ul, 1u);
  return fs_allx64(1u);
}

inline bool fs_case_eq32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint ax = a.xz & mask;
  uint bx = b.xz & mask;
  if ((ax ^ bx) != 0u) return false;
  uint known = (~(ax | bx)) & mask;
  return ((a.val ^ b.val) & known) == 0u;
}
inline bool fs_case_eq64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong ax = a.xz & mask;
  ulong bx = b.xz & mask;
  if ((ax ^ bx) != 0ul) return false;
  ulong known = (~(ax | bx)) & mask;
  return ((a.val ^ b.val) & known) == 0ul;
}
inline bool fs_casez32(FourState32 a, FourState32 b, uint ignore_mask, uint width) {
  uint mask = fs_mask32(width);
  uint ignore = ignore_mask & mask;
  uint cared = (~ignore) & mask;
  if ((a.xz & cared) != 0u) return false;
  return ((a.val ^ b.val) & cared) == 0u;
}
inline bool fs_casez64(FourState64 a, FourState64 b, ulong ignore_mask, uint width) {
  ulong mask = fs_mask64(width);
  ulong ignore = ignore_mask & mask;
  ulong cared = (~ignore) & mask;
  if ((a.xz & cared) != 0ul) return false;
  return ((a.val ^ b.val) & cared) == 0ul;
}
inline bool fs_casex32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  uint cared = (~(a.xz | b.xz)) & mask;
  return ((a.val ^ b.val) & cared) == 0u;
}
inline bool fs_casex64(FourState64 a, FourState64 b, uint width) {
  ulong mask = fs_mask64(width);
  ulong cared = (~(a.xz | b.xz)) & mask;
  return ((a.val ^ b.val) & cared) == 0ul;
}

#endif  // GPGA_4STATE_H
