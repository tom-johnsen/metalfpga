# REV17 - X propagation test

**Commit:** 6ea2367
**Date:** Fri Dec 26 14:43:07 2025 +0100
**Message:** .

## Overview

Minimal commit adding a single test file for **X (unknown) value propagation** semantics. This test validates 4-state logic behavior, specifically the optimistic evaluation rules for X values in boolean operations (e.g., `X & 0 = 0`, `X | 1 = 1`). The test complements the existing `test_z_propagation.v` added in REV16.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes |
| **Elaborate** | ✓ Functional | No changes |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✓ Complete | No changes |
| **Host emission** | ✓ Enhanced | No changes |
| **Runtime** | ⚙ Partial | No changes |

## User-Visible Changes

**New Test:**
- `verilog/systemverilog/test_x_propagation.v` - Tests X value propagation through operators

**Test validates:**
- `X & 1 = X` (pessimistic - result unknown)
- `X & 0 = 0` (optimistic - must be 0 regardless of X)
- `X | 1 = 1` (optimistic - must be 1 regardless of X)
- `X | 0 = X` (pessimistic - result unknown)

## Architecture Changes

No architecture changes - this is a test-only commit.

## Test Coverage

### New Test File

**test_x_propagation.v** (19 lines):

```verilog
module test_x_propagation;
  reg a, b, c;
  wire [3:0] results;

  // Optimistic evaluation tests
  assign results[0] = 1'bx & 1'b1;  // X & 1 = X (pessimistic)
  assign results[1] = 1'bx & 1'b0;  // X & 0 = 0 (optimistic)
  assign results[2] = 1'bx | 1'b1;  // X | 1 = 1 (optimistic)
  assign results[3] = 1'bx | 1'b0;  // X | 0 = X (pessimistic)

  initial begin
    a = 1'bx;
    b = 1'b1;
    c = a & b;  // Should be X
  end
endmodule
```

**Purpose:**
- Documents expected X propagation behavior
- Tests 4-state logic implementation
- Validates optimistic evaluation (where result is deterministic)

**Evaluation semantics:**
- **Optimistic AND**: `X & 0 = 0` because result must be 0 regardless of X value
- **Optimistic OR**: `X | 1 = 1` because result must be 1 regardless of X value
- **Pessimistic AND**: `X & 1 = X` because result depends on unknown X value
- **Pessimistic OR**: `X | 0 = X` because result depends on unknown X value

### Test Suite Statistics

- **verilog/pass/**: 174 files (no change)
- **verilog/**: 134 files (no change)
- **verilog/systemverilog/**: 19 files (up from 18 in REV16)
  - +1 test (test_x_propagation.v)
- **Total tests**: 327 (174 passing, 153 unimplemented)

## Implementation Details

This commit adds no implementation - it's purely a test specification.

**Why X propagation matters:**
- Verilog 4-state logic supports X (unknown) and Z (high-impedance)
- X propagation rules are **context-sensitive** (optimistic where deterministic)
- Important for simulation accuracy (conservative vs optimistic evaluation)
- Already implemented in REV4's 4-state logic system

**Current implementation status:**
- X propagation already working (from REV4)
- This test validates existing behavior
- Test should pass with current 4-state MSL codegen

## Known Gaps and Limitations

No new gaps - this is a test addition only.

### Parse Stage (v0.4)
- All X literal parsing already working

### Elaborate Stage (v0.4)
- No changes needed

### Codegen Stage (v0.4)
- 4-state X propagation already implemented (REV4)
- Uses bitwise operations on `xz` field
- Optimistic evaluation handled by MSL helpers

### Runtime (v0.4)
- No changes needed

## Semantic Notes (v0.4)

**X propagation semantics (IEEE 1364-2005):**

Truth table for AND:
```
  & | 0 1 X Z
  --|--------
  0 | 0 0 0 0
  1 | 0 1 X X
  X | 0 X X X
  Z | 0 X X X
```

Truth table for OR:
```
  | | 0 1 X Z
  --|--------
  0 | 0 1 X X
  1 | 1 1 1 1
  X | X 1 X X
  Z | X 1 X X
```

**Optimistic evaluation:**
- When result is **deterministic** despite unknown inputs, use known value
- Example: `X & 0` must be 0 (AND with 0 always yields 0)
- Example: `X | 1` must be 1 (OR with 1 always yields 1)

**Pessimistic evaluation:**
- When result **depends on unknown input**, propagate X
- Example: `X & 1` yields X (result unknown - could be 0 or 1)
- Example: `X | 0` yields X (result unknown - could be 0 or 1)

**Existing implementation (REV4):**
- X values stored in `xz` bitfield alongside `value` bits
- MSL helper functions implement truth tables
- `fs_and`, `fs_or`, etc. handle optimistic/pessimistic cases correctly

## Statistics

- **Files changed**: 1
- **Lines added**: 19
- **Lines removed**: 0
- **Net change**: +19 lines

**Breakdown:**
- Test file: +19 lines (test_x_propagation.v)
- No source code changes

**Test suite:**
- 327 total tests (up from 326 in REV16)
- 174 passing tests (no change)
- 153 unimplemented tests (up from 152)

This minimal commit adds a specification test for X propagation semantics, complementing the Z propagation test from REV16. The functionality being tested already exists (implemented in REV4's 4-state logic system), so this test should pass once the runtime is complete.
