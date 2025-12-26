# REV3 - More test cases

**Commit:** eadf447
**Date:** Tue Dec 23 18:56:51 2025 +0100
**Message:** More test cases

## Overview

Test-only commit adding coverage for features not yet implemented in the compiler. These tests document expected behavior for future implementation of `always @(*)`, `case`, loops, functions, and `generate`.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes to parser |
| **Elaborate** | ✓ Functional | No changes to elaboration |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✗ Not implemented | No changes |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Test Files (14 total, +73 lines):**

### Tests for Unimplemented Features (will fail or be skipped)
These document target behavior for future implementation:

**Combinational sensitivity:**
- `verilog/always_star.v` - `always @(*)` automatic sensitivity list
  ```verilog
  always @(*) y = a & b;  // sensitivity inferred from RHS
  ```

**Control flow:**
- `verilog/case_statement.v` - Case statement with default
  ```verilog
  case (sel)
    0: out = 8'h00;
    1: out = 8'hFF;
    default: out = 8'h55;
  endcase
  ```

**Loops:**
- `verilog/for_loop.v` - For loop in initial block
  ```verilog
  initial for (i = 0; i < 8; i = i + 1)
    arr[i] = i;
  ```

**Procedural constructs:**
- `verilog/function.v` - Function declaration and call
- `verilog/generate.v` - Generate block construct

**Arrays:**
- `verilog/array.v` - Array declaration and access

**Hierarchy:**
- `verilog/recursive_module.v` - Recursive module instantiation (should error)

**Diagnostics:**
- `verilog/undeclared_clock.v` - Tests warning from REV2 for undeclared clock

### Tests for Existing Features (should pass)
Located in `verilog/pass/`:

- `blocking_nonblocking.v` - Mix of `=` and `<=` in always blocks
- `comparison_all.v` - All comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- `multi_always.v` - Multiple always blocks in same module
- `negedge.v` - Negedge-triggered always block
- `param_complex_expr.v` - Parameters with complex constant expressions
- `wire_with_init.v` - Wire initialization (tests REV2 feature)

## Test Organization

**Directory structure:**
- `verilog/pass/` - Tests that should parse, elaborate, and codegen successfully
- `verilog/` (root) - Tests for features not yet implemented (parse may fail)

**Purpose:**
- Document expected syntax and semantics
- Provide test cases for incremental feature implementation
- Validate existing features continue working

## Implementation Notes

**No code changes** - This commit only adds test files.

**Test categorization:**
- 8 tests in `verilog/` root for unimplemented features
- 6 tests in `verilog/pass/` for existing features

**Expected behavior (as of REV3):**
- Tests in `verilog/pass/` should compile successfully
- Tests in `verilog/` root will fail at various stages:
  - `always_star.v` - Parser doesn't support `@(*)`
  - `case_statement.v` - Parser doesn't support `case`
  - `for_loop.v` - Parser doesn't support `for`, `initial`, or `integer`
  - `function.v` - Parser doesn't support `function`
  - `generate.v` - Parser doesn't support `generate`
  - `array.v` - Parser may accept but elaboration/codegen incomplete
  - `recursive_module.v` - Should error during elaboration
  - `undeclared_clock.v` - Should issue warning (REV2 feature)

## Known Gaps and Limitations

Same as REV2 - this commit adds tests but doesn't implement features.

**Features documented but not implemented:**
- `always @(*)` combinational sensitivity
- `case` / `casex` / `casez` statements
- `for` / `while` / `repeat` loops
- `function` / `task` declarations
- `generate` blocks
- `initial` blocks
- `integer` type
- Array/memory operations

## Statistics

- **Files changed**: 14 (all new)
- **Lines added**: 73
- **Test organization**:
  - 8 tests for unimplemented features (documentation)
  - 6 tests for existing features (validation)
