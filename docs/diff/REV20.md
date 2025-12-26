# REV20 - Code refactoring and 4-state library extraction

**Commit:** 464fd19
**Date:** Fri Dec 26 22:00:35 2025 +0100
**Message:** Added CSE, tidied up a bit

## Overview

Major refactoring commit that **extracted 4-state logic helpers into a reusable header file** (`include/gpga_4state.h`), added **read signal analysis** infrastructure (preparation for CSE optimization), updated test suite count to **225 passing tests**, and added comprehensive **analog simulation architecture documentation**. Despite the commit message mentioning "CSE", the primary work is infrastructure cleanup and library extraction. Net change: +1,341 lines (mainly new header file and documentation).

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes |
| **Elaborate** | ✓ Functional | No changes |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✓ Refactored | Uses `#include "gppa_4state.h"` instead of inline |
| **Host emission** | ✓ Functional | No changes |
| **Runtime** | ✓ Functional | No changes |

## User-Visible Changes

**Test Suite Update:**
- **225 passing tests** reported in README (up from 228 in REV19 - discrepancy likely typo)
- No actual test files changed (documentation-only update)

**Documentation Additions:**
- **ANALOG.md** (343 lines NEW): Comprehensive Verilog-AMS mixed-signal simulation architecture
  - Fixed-point arithmetic for analog signals (Q16.16 format)
  - Analog/digital conversion strategies
  - Future roadmap for mixed-signal support
  - Proof-of-concept RC circuit simulation

**README Updates:**
- Status updated: "v0.4+" (active development) from "v0.4 (in development)"
- Added **runtime validation disclaimer**: "MSL code is verbose and generally correct, but has not been thoroughly validated through kernel execution"
- Expanded feature list with system tasks, procedural tasks, switch-level modeling
- Updated test count: 225 passing tests
- Reorganized "Not Yet Implemented" section (reduced from 3 tiers to 2)

**MSL Code Generation:**
- 4-state helper functions now included via `#include "gpga_4state.h"` (was inline ~600 lines)
- Cleaner generated MSL (shorter preamble)

## Architecture Changes

### 4-State Logic Library Extraction (+612 lines NEW)

**File**: `include/gpga_4state.h`

Previously, every generated MSL file contained ~600 lines of 4-state helper functions inlined. This commit extracts them into a **reusable header library**:

**Header structure:**
```cpp
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
```

**Dual compilation support:**
- When compiled by Metal shader compiler: Full Metal stdlib
- When included in C++ tooling: Fallback typedefs for IDE support

**Complete function library (50+ functions):**

**Basic operations:**
```cpp
inline uint fs_mask32(uint width);              // Width mask (32-bit)
inline ulong fs_mask64(uint width);             // Width mask (64-bit)
inline FourState32 fs_make32(uint val, uint xz, uint width);
inline FourState64 fs_make64(ulong val, ulong xz, uint width);
inline FourState32 fs_allx32(uint width);       // All-X value
inline FourState64 fs_allx64(uint width);
inline FourState32 fs_resize32(FourState32 a, uint width);
inline FourState64 fs_resize64(FourState64 a, uint width);
```

**Sign extension:**
```cpp
inline FourState32 fs_sext32(FourState32 a, uint src_width, uint target_width);
inline FourState64 fs_sext64(FourState64 a, uint src_width, uint target_width);
```

**Merging (for unknown ternary conditions):**
```cpp
inline FourState32 fs_merge32(FourState32 a, FourState32 b, uint width);
inline FourState64 fs_merge64(FourState64 a, FourState64 b, uint width);
```

**Bitwise operations:**
```cpp
inline FourState32 fs_not32(FourState32 a, uint width);
inline FourState32 fs_and32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_or32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_xor32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_nand32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_nor32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_xnor32(FourState32 a, FourState32 b, uint width);
```

**Arithmetic operations:**
```cpp
inline FourState32 fs_add32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_sub32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_mul32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_div32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_mod32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_pow32(FourState32 a, FourState32 b, uint width);
```

**Comparison operations:**
```cpp
inline FourState32 fs_eq32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_ne32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_lt32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_gt32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_le32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_ge32(FourState32 a, FourState32 b, uint width);
```

**Shift operations:**
```cpp
inline FourState32 fs_shl32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_shr32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_sshl32(FourState32 a, FourState32 b, uint width);
inline FourState32 fs_sshr32(FourState32 a, FourState32 b, uint width);
```

**Reduction operations:**
```cpp
inline FourState32 fs_red_and32(FourState32 a, uint width);
inline FourState32 fs_red_or32(FourState32 a, uint width);
inline FourState32 fs_red_xor32(FourState32 a, uint width);
inline FourState32 fs_red_nand32(FourState32 a, uint width);
inline FourState32 fs_red_nor32(FourState32 a, uint width);
inline FourState32 fs_red_xnor32(FourState32 a, uint width);
```

**Ternary/mux:**
```cpp
inline FourState32 fs_mux32(FourState32 cond, FourState32 t, FourState32 f, uint width);
inline FourState64 fs_mux64(FourState64 cond, FourState64 t, FourState64 f, uint width);
```

**All operations have 64-bit variants** (`fs_*64` functions).

**Benefits of extraction:**
1. **Reduced generated code size**: Each MSL file saves ~600 lines
2. **Single source of truth**: Bug fixes apply to all generated code
3. **Better IDE support**: Header can be included in C++ tooling for syntax checking
4. **Compilation efficiency**: Metal compiler can optimize header once

### MSL Codegen: Read Signal Analysis (+216 lines)

**File**: `src/codegen/msl_codegen.cc`

Added infrastructure for analyzing which signals are **read** by statements and expressions. This is preparatory work for **Common Subexpression Elimination (CSE)** optimization (not fully implemented yet).

**Expression-level read analysis:**
```cpp
void CollectReadSignalsExpr(const Expr& expr,
                            std::unordered_set<std::string>* out) {
  if (!out) return;
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      out->insert(expr.ident);  // Track signal read
      return;
    case ExprKind::kUnary:
      if (expr.operand) CollectReadSignalsExpr(*expr.operand, out);
      return;
    case ExprKind::kBinary:
      if (expr.lhs) CollectReadSignalsExpr(*expr.lhs, out);
      if (expr.rhs) CollectReadSignalsExpr(*expr.rhs, out);
      return;
    case ExprKind::kTernary:
      if (expr.condition) CollectReadSignalsExpr(*expr.condition, out);
      if (expr.then_expr) CollectReadSignalsExpr(*expr.then_expr, out);
      if (expr.else_expr) CollectReadSignalsExpr(*expr.else_expr, out);
      return;
    case ExprKind::kSelect:
      if (expr.base) CollectReadSignalsExpr(*expr.base, out);
      if (expr.msb_expr) CollectReadSignalsExpr(*expr.msb_expr, out);
      if (expr.lsb_expr) CollectReadSignalsExpr(*expr.lsb_expr, out);
      return;
    // ... all expression kinds
  }
}
```

**Statement-level read analysis:**
```cpp
void CollectReadSignals(const Statement& stmt,
                        std::unordered_set<std::string>* out) {
  if (!out) return;
  if (stmt.kind == StatementKind::kAssign) {
    // Right-hand side is read
    if (stmt.assign.rhs) CollectReadSignalsExpr(*stmt.assign.rhs, out);
    // Index expressions are reads
    if (stmt.assign.lhs_index) CollectReadSignalsExpr(*stmt.assign.lhs_index, out);
    for (const auto& index : stmt.assign.lhs_indices) {
      if (index) CollectReadSignalsExpr(*index, out);
    }
    // Delay expressions are reads
    if (stmt.assign.delay) CollectReadSignalsExpr(*stmt.assign.delay, out);
    return;
  }
  if (stmt.kind == StatementKind::kIf) {
    if (stmt.condition) CollectReadSignalsExpr(*stmt.condition, out);
    for (const auto& inner : stmt.then_branch) CollectReadSignals(inner, out);
    for (const auto& inner : stmt.else_branch) CollectReadSignals(inner, out);
    return;
  }
  // ... all statement kinds (case, for, while, repeat, etc.)
}
```

**Use case (not yet fully utilized):**
```cpp
// Identify signals that are read by a block of code
std::unordered_set<std::string> reads;
CollectReadSignals(stmt, &reads);

// Future CSE optimization could use this to:
// 1. Detect when same expression is evaluated multiple times
// 2. Extract to temporary variable
// 3. Reuse temporary instead of recomputing
```

**CSE Example (theoretical):**

**Before CSE:**
```metal
// a + b computed twice
uint result1 = (a_val + b_val) & mask;
uint result2 = (a_val + b_val) & mask;
uint result3 = result1 | result2;
```

**After CSE:**
```metal
// Common subexpression extracted
uint tmp = (a_val + b_val) & mask;
uint result1 = tmp;
uint result2 = tmp;
uint result3 = result1 | result2;
```

**Current status**: Infrastructure added, optimization **not yet active** in codegen.

### MSL Codegen: Include Header Instead of Inline (~600 lines moved)

**File**: `src/codegen/msl_codegen.cc`

**Before REV20:**
```cpp
std::string EmitMSLStub(const Module& module, bool four_state) {
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  out << "struct GpgaParams { uint count; };\n\n";

  if (four_state) {
    // ~600 lines of inline 4-state functions
    out << "struct FourState32 { uint val; uint xz; };\n";
    out << "struct FourState64 { ulong val; ulong xz; };\n";
    out << "inline uint fs_mask32(uint width) {\n";
    // ... 50+ function definitions ...
  }
}
```

**After REV20:**
```cpp
std::string EmitMSLStub(const Module& module, bool four_state) {
  std::ostringstream out;
  out << "#include <metal_stdlib>\n";
  out << "using namespace metal;\n\n";
  if (four_state) {
    out << "#include \"gpga_4state.h\"\n\n";  // ← Single include instead of ~600 lines
  }
  out << "struct GpgaParams { uint count; };\n\n";
  out << "constexpr ulong __gpga_time = 0ul;\n\n";
  out << "// Placeholder MSL emitted by GPGA.\n\n";
}
```

**Impact:**
- Generated `.metal` files are **~600 lines shorter**
- Compilation faster (header precompiled by Metal compiler)
- All tests use same 4-state library (consistency)

### Minor Fix: Bit Select Update

**File**: `src/codegen/msl_codegen.cc`

Fixed bit-select update to properly mask RHS before shifting:

**Before:**
```cpp
std::string EmitBitSelectUpdate(const std::string& base_expr,
                                uint base_width,
                                const std::string& index_expr,
                                const std::string& rhs_expr) {
  std::string idx = "uint(" + index_expr + ")";
  std::string one = (base_width > 32) ? "1ul" : "1u";
  std::string cast = (base_width > 32) ? "(ulong)" : "(uint)";
  std::string clear = "~(" + one + " << " + idx + ")";
  std::string set = "((" + cast + rhs_expr + " & " + one + ") << " + idx + ")";  // Inline cast
  return "(" + base_expr + " & " + clear + ") | " + set;
}
```

**After:**
```cpp
std::string EmitBitSelectUpdate(const std::string& base_expr,
                                uint base_width,
                                const std::string& index_expr,
                                const std::string& rhs_expr) {
  std::string idx = "uint(" + index_expr + ")";
  std::string one = (base_width > 32) ? "1ul" : "1u";
  std::string cast = CastForWidth(base_width);           // ← Use helper
  std::string rhs_masked = MaskForWidthExpr(rhs_expr, 1); // ← Mask first
  std::string clear = "~(" + one + " << " + idx + ")";
  std::string set = "((" + cast + rhs_masked + ") << " + idx + ")";
  return "(" + base_expr + " & " + clear + ") | " + set;
}
```

**Why it matters:**
- Ensures single-bit writes don't leak higher bits
- Uses existing helper functions for consistency

## Test Coverage

**No test files changed** - this is a refactoring/infrastructure commit.

**README update:**
- Test count: **225 passing tests** (discrepancy from REV19's 228 - likely documentation correction)

**Test categories (unchanged from REV19):**
- System tasks: $display, $monitor, $strobe, $finish, $time, $readmemh/b, etc.
- Parameters: localparam, parameter override, genvar scope
- Timing: delays, event controls, wait conditions
- 4-state logic: X/Z propagation
- Switch-level modeling: transmission gates, MOS switches, drive strengths
- Procedural tasks with inputs/outputs

## Implementation Details

### ANALOG.md: Mixed-Signal Architecture (v0.4+)

**Status**: Architecturally supported, not yet implemented

**Key concepts:**

**Fixed-point analog representation:**
```metal
// Analog voltage as Q16.16 fixed-point (16 integer bits, 16 fractional bits)
// Range: 0x00000000 (0.0V) to 0x00034CCC (3.3V)
constant uint* voltage_val [[buffer(0)]];  // Fixed-point value
constant uint* voltage_xz [[buffer(1)]];   // 0 = known, 1 = undriven

// Existing 4-state operations work as-is on fixed-point values
FourState32 v1 = fs_make32(voltage_val[gid], voltage_xz[gid], 32u);
FourState32 v2 = fs_add32(v1, another_voltage, 32u);     // Analog addition
FourState32 scaled = fs_mul32(v1, gain_factor, 32u);     // Analog scaling
```

**Q16.16 format details:**
- **Bits**: 16 integer, 16 fractional
- **Range**: -32768.0 to +32767.99998
- **Resolution**: 0.0000152587890625 (1/65536)
- **Use case**: General-purpose analog signals

**Analog-to-digital conversion:**
```metal
// Schmitt trigger with hysteresis
FourState32 analog_to_digital(FourState32 analog, uint threshold_low, uint threshold_high) {
  if (analog.xz != 0u) return FourState32{0u, 1u};  // Unknown → X state

  if (analog.val > threshold_high) return FourState32{1u, 0u};  // HIGH
  else if (analog.val < threshold_low) return FourState32{0u, 0u};  // LOW
  else return FourState32{0u, 1u};  // Metastable → X
}

// Example: 0.8V/1.2V thresholds
uint VIL = 0x0000CCCC;  // 0.8V in Q16.16
uint VIH = 0x00013333;  // 1.2V in Q16.16
FourState32 digital = analog_to_digital(voltage, VIL, VIH);
```

**RC circuit example (proof of concept):**
```metal
// RC charging: V(t) = Vfinal * (1 - exp(-t/RC))
// Discrete approximation: V[n+1] = V[n] + (Vin - V[n]) * (dt/RC)
kernel void gpga_rc_charge(
  constant uint* vin_val [[buffer(0)]],
  constant uint* vin_xz [[buffer(1)]],
  device uint* vcap_val [[buffer(2)]],
  device uint* vcap_xz [[buffer(3)]],
  constant uint* rc_tau [[buffer(4)]],  // Time constant
  constant uint* dt [[buffer(5)]],      // Timestep
  uint gid [[thread_position_in_grid]]
) {
  FourState32 vin = fs_make32(vin_val[gid], vin_xz[gid], 32u);
  FourState32 vcap = fs_make32(vcap_val[gid], vcap_xz[gid], 32u);
  FourState32 tau = fs_make32(rc_tau[0], 0u, 32u);
  FourState32 timestep = fs_make32(dt[0], 0u, 32u);

  // ΔV = (Vin - Vcap) * (dt / RC)
  FourState32 delta_v = fs_sub32(vin, vcap, 32u);
  FourState32 ratio = fs_div32(timestep, tau, 32u);
  FourState32 step = fs_mul32(delta_v, ratio, 32u);
  FourState32 vcap_new = fs_add32(vcap, step, 32u);

  vcap_val[gid] = vcap_new.val;
  vcap_xz[gid] = vcap_new.xz;
}
```

**Why not implemented yet:**
1. Digital Verilog foundation must be perfected first
2. SystemVerilog features higher priority (more user demand)
3. Analog simulation requires iterative solver (GPU-hostile)
4. Unclear market demand for GPU-accelerated analog simulation
5. Verilog-AMS reference materials scarce

**Roadmap:**
- Phase 1: Foundation ✅ (4-state digital complete)
- Phase 2: SystemVerilog (next priority)
- Phase 3: Mixed-signal (future - basic analog primitives)
- Phase 4: Verilog-AMS (long-term - full analog solver)

### README: Runtime Validation Disclaimer

**New caveat added:**
> **Note**: The emitted Metal Shading Language (MSL) code is verbose and generally correct, but has not been thoroughly validated through kernel execution. The codegen produces structurally sound MSL that implements the intended semantics, though bugs may surface during actual GPU dispatch. Runtime validation is the next development phase.

**Translation**: The compiler generates MSL that:
- Compiles successfully (syntactically correct)
- Implements intended Verilog semantics (architecturally sound)
- **Has not been executed on GPU** to verify correctness

**Next phase**: Host driver to actually run kernels and validate behavior.

## Known Gaps and Limitations

### Parse Stage (v0.4+)
- No changes - parser complete for implemented features

### Elaborate Stage (v0.4+)
- No changes - elaboration working

### Codegen Stage (v0.4+)
- ✓ 4-state library extracted to header
- ✓ Read signal analysis infrastructure added
- ✗ CSE optimization not yet active (infrastructure only)
- ✗ No GPU execution/validation yet

### Runtime (v0.4+)
- ✓ Service record system defined
- ✗ No host driver implementation
- ✗ No actual kernel dispatch
- ✗ Generated MSL untested on real GPU

## Semantic Notes (v0.4+)

**4-State Library Design Decision:**

Previously, 4-state functions were **inlined** into every generated `.metal` file. This worked but had drawbacks:
- Generated files bloated with ~600 lines of boilerplate
- Bug fixes required regenerating all files
- Compilation slower (same code compiled repeatedly)

**REV20 solution**: Extract to `include/gpga_4state.h`:
- Generated files reference shared header
- Metal compiler can optimize/cache header compilation
- Single source of truth for 4-state operations
- Dual-compilation support (Metal + C++ tooling)

**CSE Infrastructure vs Implementation:**

Commit message says "Added CSE" but actual work is **preparatory infrastructure**:
- `CollectReadSignals()` / `CollectReadSignalsExpr()` added
- No actual CSE optimization pass implemented yet
- Code can identify what signals are read by statements
- Future work: detect redundant expressions and extract to temps

**Test Count Discrepancy:**

- REV19 documented: **228 passing tests**
- REV20 documents: **225 passing tests**
- No tests removed in commit
- Likely documentation correction (hand count vs script count)

## Statistics

- **Files changed**: 4
- **Lines added**: 2,243
- **Lines removed**: 902
- **Net change**: +1,341 lines

**Breakdown:**
- `ANALOG.md`: +343 lines (NEW documentation)
- `include/gpga_4state.h`: +612 lines (NEW library header)
- `README.md`: +47/-0 lines (status update)
- `src/codegen/msl_codegen.cc`: +1,254/-889 lines (net +365)
  - Added read signal analysis: +216 lines
  - Removed inline 4-state functions: ~-600 lines
  - Refactoring/cleanup: ~+750 lines

**Test suite:**
- 225 passing tests (reported - no files changed)
- 327 total tests (unchanged)
- 69% passing rate (225/327)

**Impact:**
- Generated MSL files **~600 lines shorter** (cleaner output)
- Compilation likely faster (header caching)
- Preparatory work for CSE optimization (not yet active)
- Comprehensive analog simulation architecture documented

This commit represents a **cleanup and infrastructure milestone**, setting the stage for optimization work (CSE) and documenting future capabilities (analog/mixed-signal simulation). The extraction of 4-state helpers to a reusable header is the most immediate practical benefit, improving generated code quality and maintainability.
