# CRlibm -> MSL IEEE-754 Binary64 Port Plan

This document is the shared checklist and map for a full CRlibm port into
Metal Shading Language (MSL), using gpga's software double (binary64) stack.
Goal: bit-correct IEEE-754 double results with correctly rounded elementary
functions and Verilog-true semantics.

## Goals

- Full binary64 IEEE-754 compliance for all math used by Verilog `real`.
- Correctly rounded results for supported functions (CRlibm guarantees).
- MSL header library, similar to `include/gpga_4state.h`, so codegen emits
  `#include "gpga_ieee754.h"` rather than inlining huge blocks into every MSL.
- Clear mapping from CRlibm source to MSL code so we can track coverage.
- No down-conversion to float. Everything stays in binary64 bits.

## Non-Goals (for now)

- SystemVerilog-only math, except where it overlaps with Verilog `real`.
- Platform-specific CRlibm variants (Itanium, x86/extended). We use portable
  CRlibm code paths.

## Constraints (Metal / gpga)

- MSL does not support `double`. We represent binary64 as `ulong` bits.
- No global rounding-mode controls. Rounding is software-controlled per op.
- No 128-bit integer type. Any wide arithmetic must be manual.
- No recursion, limited stack. Prefer straight-line helpers and small structs.
- Constant tables must be embedded in MSL (static constants), or in separate
  headers included by the generated MSL.

## Source of Truth

- Library root: `thirdparty/crlibm/`
- Licensing: LGPL, see `thirdparty/crlibm/COPYING` and `COPYING.LIB`.
  All ports must keep license headers and notice in the shipped header(s).

## Proposed MSL Header Layout

Single public header:

- `include/gpga_real.h`
  - Contains the full binary64 core, CRlibm internals, coefficient tables, and
    public math API (all rounding modes).
  - Sections are grouped internally (core, dd/td/scs, tables, math) but live
    in one file to make inclusion simple and foolproof.

MSL codegen should emit one include for real usage:

```
#include "gpga_real.h"
```

## Porting Strategy (High-Level)

1) **Lock down gpga_double core** (gpga_real.h section: core)
   - Bit layout, pack/unpack, sign/exp/mantissa.
   - Classification: is_zero, is_inf, is_nan, is_subnormal.
   - Integer conversions: int/uint <-> gpga_double.
   - Rounding helpers for RN/RD/RU/RZ.
   - Exact add/sub/mul/div/sqrt in software using integer ops.

2) **Port CRlibm support layers** (gpga_real.h section: internals)
   - double-double and triple-double arithmetic.
   - Small cumulative sum (SCS) library.
   - Argument reduction helpers.

3) **Port CRlibm functions** (gpga_real.h section: math)
   - Use portable variants only (no CPU-specific/extended).
   - One-to-one mapping: same coefficients, same rounding tests.

4) **Integrate with gpga**
   - Codegen includes headers when `real` is used.
   - Runtime output formatting uses gpga_real bits (already in place).

5) **Validation**
   - Cross-check against CRlibm on CPU for random vectors.
   - Bit-exact or correctly rounded ULP <= 0.5 vs CRlibm reference.
   - Run Verilog tests that use real math.

## CRlibm Source Map (Portable Only)

Use these files as the authoritative reference. CPU-specific optimized
variants (itanium, pentium, etc) are not needed for MSL.

### Core Helpers

- `crlibm_private.c/.h`
  - Bit tricks, rounding tests (TEST_AND_RETURN_* macros), constants.
  - Port into `gpga_crlibm_internal.h` as inline functions.

- `double-extended.h`
  - CPU extended precision; skip for MSL.

- `triple-double.c/.h`
  - Triple-double arithmetic used by accurate trig/log.

- `scs_lib/` (entire folder)
  - SCS arithmetic used for argument reduction and high precision sums.

### Range Reduction / Trig

- `rem_pio2_accurate.c/.h`
  - Core reduction for sin/cos/tan.

- `trigo_accurate.c/.h`
  - Accurate sin/cos/tan using reduction + polynomials.

- `trigpi.c/.h`
  - sinpi/cospi/tanpi and asinpi/acospi/atanpi.

- `asincos.c/.h`
  - asin/acos core algorithms.

- `atan_accurate.c/.h`
  - atan core algorithms.

### Exponential / Logarithms

- `exp-td.c/.h`
- `expm1.c/.h`
- `log-td.c/.h`
- `log1p.c/.h`
- `log2-td.c/.h`
- `log10-td.c/.h`

### Pow

- `pow.c/.h`
  - Marked "under development" in CRlibm; still port for parity.

### Hyperbolic

- `csh_fast.c/.h`
  - cosh/sinh

## Required MSL Primitives (gpga_real.h: core section)

### Types

- `typedef ulong gpga_double;`
- `struct gpga_dd { gpga_double hi; gpga_double lo; }` (double-double)
- `struct gpga_td { gpga_double hi; gpga_double mid; gpga_double lo; }`
- `struct gpga_scs { gpga_double* data; uint n; }` (small cumulative sum)

### Bit Helpers

- pack/unpack sign/exp/mant
- canonical NaN, signaling NaN policy
- next_up / next_down (used in directed rounding)

### Arithmetic (binary64)

- add/sub/mul/div
- sqrt
- fma (optional but recommended for accuracy)

### Rounding Modes

- RN (nearest-even)
- RD (toward -inf)
- RU (toward +inf)
- RZ (toward zero)

CRlibm uses explicit rounding in many "test-and-return" macros.
We should implement these as inline functions that operate on gpga_double
bits and the computed low-order error term.

### Classification

- is_nan, is_inf, is_zero, is_subnormal
- signbit
- isnormal

## MSL Porting Notes

- Replace `double` with `gpga_double` and all arithmetic with gpga helpers.
- Replace `long double` with `gpga_dd` or `gpga_td` as required.
- Replace `uint64_t` with `ulong`.
- Replace `int64_t` with `long` where safe; otherwise keep manual sign ops.
- Eliminate all FPU control (init/exit, fenv). Rounding is software-controlled.
- Avoid macros that rely on C type punning or union tricks; use explicit
  bit operations.

## Integration Points in metalfpga

- `src/codegen/msl_codegen.cc`
  - Stop inlining softfloat64 code when real is used.
  - Emit `#include "gpga_real.h"`.

- `src/runtime/metal_runtime.mm`
  - No changes expected unless new service arg kinds are added.

## Verification Plan

### 1) CRlibm Reference

- Build CRlibm on CPU and generate reference outputs for a random vector.
- Compare gpga MSL results (via CLI run or host pipeline) bit-for-bit.

### 2) IEEE Edge Cases

- NaN, +/-Inf, +/-0, subnormals
- Powers of two, boundary exponents
- Signed zero handling in odd functions (tan, atan)

### 3) Verilog Tests

- `verilog/test_real_math.v`
- `verilog/pass/test_real_*`
- New targeted tests for each function (sin/cos/tan/log/exp/etc).

### 4) ULP Audit

- For each function, compute max ULP error vs CRlibm. Target: 0.5 ULP.

## Deliverables Checklist

### Core

- [ ] `include/gpga_real.h` with gpga_double core, internals, tables, and math

### Function Coverage (CRlibm API)

- [ ] exp_rn/rd/ru/rz
- [ ] log_rn/rd/ru/rz
- [ ] log2_rn/rd/ru/rz
- [ ] log10_rn/rd/ru/rz
- [ ] log1p_rn/rd/ru/rz
- [ ] expm1_rn/rd/ru/rz
- [ ] sin_rn/rd/ru/rz
- [ ] cos_rn/rd/ru/rz
- [ ] tan_rn/rd/ru/rz
- [ ] asin_rn/rd/ru/rz
- [ ] acos_rn/rd/ru/rz
- [ ] atan_rn/rd/ru/rz
- [ ] sinh_rn/rd/ru/rz
- [ ] cosh_rn/rd/ru/rz
- [ ] sinpi_rn/rd/ru/rz
- [ ] cospi_rn/rd/ru/rz
- [ ] tanpi_rn/rd/ru/rz
- [ ] asinpi_rn/rd/ru/rz
- [ ] acospi_rn/rd/ru/rz
- [ ] atanpi_rn/rd/ru/rz
- [ ] pow_rn (CRlibm notes: not proven fully rounded)
- [ ] exp2_rn/rd/ru (optional but in CRlibm)

### Codegen / Integration

- [ ] Codegen includes new headers when real is used.
- [ ] Remove inlined softfloat code in generated MSL.
- [ ] Update include path handling so MSL compiler sees `include/`.

### Tests

- [ ] Add unit tests for each math function (real math suite)
- [ ] Add golden comparison vs CPU CRlibm (host-side test harness)

## Port Order (Recommended)

1) `gpga_ieee754.h`: classification + rounding + add/sub/mul/div/sqrt
2) `triple-double`, `scs_lib`: internal precision helpers
3) `log/exp/log1p/expm1` (core for many ops)
4) `rem_pio2_accurate` + `trigo_accurate`
5) `asin/acos/atan`
6) `pow` and `trigpi`
7) Hyperbolic (`sinh/cosh`)

## Notes on IEEE Compliance Statement

We should only claim "100% IEEE-754 binary64 correctly rounded" once:

- All functions are bit-correct vs CRlibm for RN (and RD/RU/RZ if exposed).
- Edge cases match IEEE requirements (NaN propagation, signed zero, etc).
- Tests confirm ULP <= 0.5 for all functions across a large corpus.

## Where This Lives

- Document: `docs/CRLIBM_PORTING.md` (this file)
- Source: `thirdparty/crlibm/`
- Target headers: `include/`
