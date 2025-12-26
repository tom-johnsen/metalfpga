# REV2 - Fixed pretty-printing and undeclared wire warnings

**Commit:** a7071a1
**Date:** Tue Dec 23 18:14:27 2025 +0100
**Message:** Fixed pretty-printing of nested if/else and warning for undeclared wires

## Overview

Bug-fix commit improving diagnostics and output formatting. Adds warnings for undeclared clock signals and fixes pretty-printing of nested if/else chains to use `else if` format.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | Added wire initialization support (`wire x = value`) |
| **Elaborate** | ✓ Enhanced | Now warns about undeclared clock signals |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✗ Not implemented | No changes |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Warnings:**
- Clock signals in `always @(posedge clk)` now checked for declaration
- Warning issued if clock is not declared as port or net

**Improved Verilog Features:**
- Wire initialization now supported: `wire [7:0] temp = 8'h00;`
- Parser converts wire initialization to implicit continuous assignment

**Better Output Formatting:**
- `--dump-flat` now formats nested if/else as `else if` chains
- Previously printed deeply nested `} else { if {` structures

## Implementation Details

### Parser Enhancement: Wire Initialization

**File:** `src/frontend/verilog_parser.cc`

Added support for wire declarations with initialization:
```verilog
wire [7:0] temp = 8'h00;  // now parsed correctly
```

**Implementation:**
- After parsing wire name and width, check for `=` symbol
- If found, parse initializer expression
- Create implicit `Assign` entry: `assign temp = 8'h00;`
- Add both the net declaration and the assignment to the module

This maintains compatibility with v0.1 elaboration (continuous assignments only).

### Elaboration Enhancement: Clock Signal Validation

**File:** `src/core/elaboration.cc`

**New functions:**

`IsDeclaredSignal(module, name)`:
- Checks if a signal name exists in ports or nets
- Used to validate clock signal declarations

`WarnUndeclaredClocks(flat, diagnostics)`:
- Iterates all `always` blocks in flattened module
- For each block, checks if `block.clock` is declared
- Issues warning if clock signal not found in ports or nets

**Integration:**
- Called at end of `Elaborate()` after single-driver validation
- Non-fatal (warning only) - doesn't block compilation

**Rationale:**
Common error is to use `@(posedge clk)` without declaring `clk` as input/wire. This catches the mistake early.

### CLI Enhancement: Pretty-Print If/Else Chains

**File:** `src/main.mm`

**Old behavior:**
```
if (cond1) {
  ...
} else {
  if (cond2) {
    ...
  } else {
    ...
  }
}
```

**New behavior:**
```
if (cond1) {
  ...
} else if (cond2) {
  ...
} else {
  ...
}
```

**Implementation:**
- `DumpStatement()` now handles if/else chains specially
- Uses while loop to detect single-statement else branches containing if
- Emits `} else if (` instead of `} else {\n  if (`
- Continues chain until reaching non-if else branch or end

**Algorithm:**
1. Start with current if statement
2. Print `if (cond) {`
3. Print then branch
4. Check else branch:
   - If empty: print `}` and done
   - If single-statement and is `kIf`: print `} else if (` and continue loop
   - Otherwise: print `} else {`, dump else branch, print `}`, done

## Test Impact

No new test files. Changes improve diagnostics for existing tests.

**Tests affected:**
- `test_undeclared_clock.v` (if it exists) would now produce warning
- Any test with nested if/else chains now produces cleaner `--dump-flat` output

## Known Gaps and Limitations

Same as REV1 - no new limitations introduced.

**Note:** Wire initialization is syntactic sugar only:
- `wire x = expr;` becomes `wire x; assign x = expr;`
- No semantic checking of initialization expression at parse time
- Elaboration will catch errors (multi-driver, width mismatch, etc.)

## Statistics

- **Files changed**: 3
- **Lines added**: 62
- **Lines removed**: 9
- **Net change**: +53 lines

**Changes by file:**
- `src/core/elaboration.cc`: +25 lines (clock validation)
- `src/frontend/verilog_parser.cc`: +10 lines (wire initialization)
- `src/main.mm`: +27 lines (if/else formatting)
