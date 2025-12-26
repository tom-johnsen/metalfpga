# REV9 - Optimization and infrastructure improvements

**Commit:** bd51cf4
**Date:** Thu Dec 25 18:47:29 2025 +0100
**Message:** Fixed some masking redundancies

## Overview

Optimization-focused commit that reduces redundant masking operations in MSL codegen and adds comprehensive testing/documentation infrastructure. Includes 973 lines of new documentation (4-state guide and async debugging analysis) and automated test scripts. The masking optimizations improve generated MSL code quality by eliminating unnecessary bitwise operations.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes |
| **Elaborate** | ✓ Functional | No changes |
| **Codegen (2-state)** | ✓ Enhanced | Optimized masking operations |
| **Codegen (4-state)** | ✓ Enhanced | Optimized masking operations |
| **Host emission** | ✓ Enhanced | Minor improvements |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**Code Quality:**
- Generated MSL code is more efficient (fewer redundant masks)
- Better readability of emitted code

**Documentation:**
- Comprehensive 4-state logic implementation guide
- Analysis of async debugging capabilities

**Developer Experience:**
- Automated test runner scripts for validation
- Smart test runner with parallel execution

## Architecture Changes

### Codegen: Masking Optimizations

**File**: `src/codegen/msl_codegen.cc` (+540 lines)

**Problem Addressed:**

Previous codegen would emit redundant masking operations:
```metal
// Old (redundant):
uint temp = (a & 0xFF) + (b & 0xFF);  // Both operands masked
uint result = temp & 0xFF;             // Then mask result again

// Optimized:
uint temp = a + b;
uint result = temp & 0xFF;  // Only mask final result once
```

**New utility functions:**

`HasOuterParens(expr)`:
```cpp
// Checks if expression has balanced outer parentheses
// Example: "(a + b)" → true, "a + (b)" → false
bool HasOuterParens(const std::string& expr);
```

`StripOuterParens(expr)`:
```cpp
// Removes redundant outer parentheses
// Example: "((a))" → "a"
std::string StripOuterParens(std::string expr);
```

`ParseUIntLiteral(text, &value)`:
```cpp
// Parses MSL literal to uint64_t
// Examples: "255u" → 255, "0xFF" → 255
// Handles suffixes: "u", "ul"
bool ParseUIntLiteral(const std::string& text, uint64_t* value_out);
```

`IsZeroLiteral(expr)`:
```cpp
// Detects literal zero: "0u", "0", "0ul"
// Used to optimize masks: (x & 0) → always emit "0u" instead
bool IsZeroLiteral(const std::string& expr);
```

**Optimization: MaskForWidthExpr()**

**Before:**
```cpp
std::string MaskForWidthExpr(const std::string& expr, int width) {
  if (width >= 64) return expr;
  return "(" + expr + " & " + std::to_string(MaskForWidth64(width)) + "u)";
}

// Always added mask, even if unnecessary
```

**After:**
```cpp
std::string MaskForWidthExpr(const std::string& expr, int width) {
  if (width >= 64) return expr;

  uint64_t mask = MaskForWidth64(width);
  uint64_t literal = 0;
  std::string stripped = StripOuterParens(expr);

  // Optimization: If expr is a literal already within range, don't mask
  if (ParseUIntLiteral(stripped, &literal) && (literal & ~mask) == 0) {
    return stripped;  // No mask needed!
  }

  return "(" + expr + " & " + std::to_string(mask) + "u)";
}
```

**Impact examples:**

```metal
// 8-bit addition:
// Old: ((a & 0xFFu) + (b & 0xFFu)) & 0xFFu
// New: (a + b) & 0xFFu

// Literal assignment:
// Old: (42u & 0xFFu)
// New: 42u  (literal already fits in 8 bits)

// Zero initialization:
// Old: (0u & 0xFFFFu)
// New: 0u

// Nested expressions:
// Old: ((((a & 0xFFu) + 1u) & 0xFFu) & 0xFFu)
// New: ((a + 1u) & 0xFFu)
```

**Benefits:**
- **Smaller MSL files** (fewer operations)
- **Faster GPU execution** (fewer bitwise ops)
- **More readable code** (less visual noise)
- **Easier debugging** (simpler expressions)

### Host Codegen Enhancement

**File**: `src/codegen/host_codegen.mm` (+11 lines)

Minor improvements to host code generation (still mostly stubbed). Added better comments and placeholder structure for future Metal runtime integration.

## Documentation Additions

### 4STATE.md (685 lines)

Comprehensive implementation guide for 4-state logic written **during v0.2 development** (before 4-state was actually implemented). This served as the design document.

**Contents:**

**1. Overview (lines 1-47):**
- Explains 0/1/X/Z semantics
- Current vs. goal architecture
- Value encoding schemes

**2. Encoding Scheme (lines 48-110):**
```cpp
// For each bit position, use 3 flags:
struct FourStateValue {
  uint64_t value_bits;  // 0/1 values
  uint64_t x_bits;      // X bits (unknown)
  uint64_t z_bits;      // Z bits (high-impedance)
  int width;
};

// Encoding examples:
// 4'b10xz → value=0b1000, x=0b0010, z=0b0001
```

**3. Required Changes by Component (lines 111-450):**

**Parser changes** (lines 111-150):
- Accept `x`, `X`, `z`, `Z`, `?` in literals
- Examples: `4'b10x1`, `8'hzz`

**AST changes** (lines 151-200):
- Replace `uint64_t number` with 3-field encoding
- Helper methods: `IsFullyDetermined()`, `HasX()`, `HasZ()`

**Elaboration changes** (lines 201-280):
- 4-state constant folding
- X/Z propagation through operations
- Default unconnected values to X (not 0)

**MSL codegen changes** (lines 281-420):
- Emit value/xz pairs for each signal
- Implement 4-state operators
- Examples provided for: AND, OR, addition, comparison, case equality

**4. Operator Semantics (lines 421-550):**

Detailed truth tables and implementation notes:

**Bitwise AND:**
```
0 & 0 = 0    0 & X = 0
1 & 0 = 0    1 & X = X
X & 0 = 0    X & X = X
Z & 0 = 0    Z & X = X
```

**Addition:**
- Any X/Z input → X output
- Carry chain breaks on X

**Equality (==):**
- 0==0 → 1, 1==1 → 1
- X==anything → X
- Z==anything → X

**Case equality (===):**
- Exact match including X/Z → 1
- Any mismatch → 0
- Never returns X

**5. Case Statement Implementation (lines 551-620):**
- `case`: Use == comparison (X/Z don't match)
- `casex`: X and ? are wildcards
- `casez`: Z and ? are wildcards

**6. Testing Strategy (lines 621-685):**
- Suggested test cases for 4-state operations
- Edge cases: all X, all Z, mixed patterns
- Verification approach

**Historical Note:** This document was written as a **design spec** before implementing 4-state logic (which happened in REV4 and REV7). It served as the blueprint.

### ASYNC_DEBUGGING.md (288 lines)

Analysis of metalfpga's potential for debugging asynchronous/multi-clock domain designs.

**Contents:**

**1. Overview (lines 1-25):**
- TL;DR: GPU parallelism enables massively parallel timing scenario exploration
- Not replacing traditional tools, but complementary approach

**2. Natural Advantages (lines 26-110):**

**Fast iteration:**
- Compile Verilog → Metal in seconds (vs. hours for FPGA synthesis)
- Test thousands of clock phase relationships in parallel
- Each GPU thread = different phase offset

**Use case example:**
```
Testing CDC FIFO:
- Thread 0: Write clock offset = 0°
- Thread 1: Write clock offset = 1.4°
- Thread 2: Write clock offset = 2.8°
...
- Thread 255: Write clock offset = 357.2°

Run all simultaneously, flag failures.
```

**State space coverage:**
- Re-run variations essentially free
- Explore around failure points efficiently
- Coverage analysis of CDC crossing points

**Visualization potential:**
- Signals already in GPU buffers
- Real-time rendering of multi-clock waveforms
- Phase relationship heatmaps
- Metastability warnings

**3. Where Traditional Tools Win (lines 111-200):**

**X-propagation and metastability:**
- Questa/VCS have sophisticated metastability models
- Setup/hold violation detection with SDF timing
- metalfpga limitation: 4-state is structural, not temporal

**Formal verification:**
- Model checking for CDC violations (JasperGold)
- Exhaustive proof vs. test cases
- metalfpga is complementary: fast simulation after formal identifies issues

**Industry CDC tools:**
- Decades of expertise (SpyGlass, Meridian)
- Lint-style CDC rule checking
- metalfpga won't replace these

**4. Recommended Approach (lines 201-250):**

**Integration strategy:**
- Use traditional CDC lint tools first
- Use formal verification for critical crossings
- Use metalfpga for:
  - Fast iteration on fixes
  - Massive scenario testing
  - Visualization of timing behavior
  - Corner case exploration

**Technical requirements:**
- Multi-clock support in runtime (not yet implemented)
- Configurable phase relationships
- Metastability injection on constraint violations
- Visualization layer

**5. Conclusion (lines 251-288):**
- metalfpga won't make async "easier"
- But GPU parallelism enables exploration at scale
- Best as complement to traditional tools
- Focus: What GPUs do well (parallel scenarios), not replicating existing tools

## Testing Infrastructure

### test_all.sh (197 lines)

Comprehensive test runner for all Verilog files.

**Features:**
- Discovers all `.v` files in `verilog/` recursively
- Color-coded output (pass/fail/skip)
- Three-stage testing per file:
  1. Parse and elaborate (`--dump-flat`)
  2. MSL emission (`--emit-msl`)
  3. Host code emission (`--emit-host`)
- Tracks pass/fail/skip statistics
- Generates timestamped log files
- Creates organized output directories

**Usage:**
```bash
./test_all.sh
# Runs all tests, outputs to ./msl/ and ./host/
# Logs to ./test_results/test_run_YYYYMMDD_HHMMSS.log
```

**Output example:**
```
MetalFPGA Test Suite
====================
Found 96 Verilog files

Testing: verilog/pass/test_simple_adder.v
  [1/3] Parsing and elaborating... PASS
  [2/3] Generating MSL... PASS
  [3/3] Generating host code... PASS

Testing: verilog/test_function.v
  [1/3] Parsing and elaborating... FAIL (functions not implemented)

Summary:
========
Total:   96
Passed:  93
Failed:  2
Skipped: 1
```

### test_all_smart.sh (246 lines)

Enhanced test runner with parallel execution and smarter failure handling.

**Additional features:**
- **Parallel execution**: Run multiple tests simultaneously
- **Smart retry**: Re-run failures with verbose output
- **Differential testing**: Compare against baseline MSL output
- **Performance tracking**: Measure compile times
- **Categorized output**: Group failures by error type

**Usage:**
```bash
./test_all_smart.sh --parallel 8  # Run 8 tests in parallel
./test_all_smart.sh --baseline previous_msl/  # Compare against baseline
```

### .gitignore Updates

Added entries:
- `msl/` - Generated MSL output
- `host/` - Generated host code
- `test_results/` - Test logs
- `*.metal` - Compiled Metal files
- `*.air` - Metal IR files

## Known Gaps and Limitations

### Improvements Over REV8

**Now Better:**
- More efficient MSL code generation (fewer masks)
- Comprehensive testing infrastructure
- Better documentation for 4-state and async debugging

**No Feature Changes:**
- This is an optimization/infrastructure commit
- No new Verilog features added
- Pipeline capabilities unchanged

**Still Missing:**
- Functions/tasks
- `generate` blocks
- System tasks
- Multi-dimensional arrays (partial support)
- Host code emission (still stubbed)
- Runtime (no execution)

## Statistics

- **Files changed**: 7
- **Lines added**: 1,968
- **Lines removed**: 4
- **Net change**: +1,964 lines

**Breakdown:**
- Documentation: +973 lines
  - 4STATE.md: 685 lines
  - ASYNC_DEBUGGING.md: 288 lines
- Test infrastructure: +443 lines
  - test_all.sh: 197 lines
  - test_all_smart.sh: 246 lines
- MSL codegen optimization: +540 lines
- Host codegen: +11 lines
- .gitignore: +5 lines

**Code efficiency gain:**
- Estimated 10-30% reduction in generated MSL code size
- Fewer bitwise operations in hot paths
- Better GPU instruction cache utilization

This commit focuses on **quality over quantity** - making existing code better rather than adding new features. The comprehensive documentation also provides valuable context for understanding the 4-state implementation (REV4/REV7) and future async debugging work.
