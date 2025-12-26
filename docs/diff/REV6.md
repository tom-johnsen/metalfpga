# REV6 - Completed test naming consistency

**Commit:** f68b855
**Date:** Thu Dec 25 16:20:17 2025 +0100
**Message:** forgot three

## Overview

Quick follow-up to REV5 completing the test file renaming task. Three test files in the root `verilog/` directory were missed during the bulk rename in REV5 and are now updated to follow the `test_` prefix convention.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes from v0.2 |
| **Elaborate** | ✓ Functional | No changes from v0.2 |
| **Codegen (2-state)** | ✓ MSL emission | No changes from v0.2 |
| **Codegen (4-state)** | ✓ MSL emission | No changes from v0.2 |
| **Host emission** | ✗ Stubbed only | No changes from v0.2 |
| **Runtime** | ✗ Not implemented | No changes from v0.2 |

## User-Visible Changes

**Test File Renames:**
- All test files now consistently use `test_` prefix
- Completes the naming standardization started in REV5

## File Renames

### Tests for Unimplemented Features (3 files)

All three files are in `verilog/` root (not in `pass/`) because these features are not yet implemented:

**for_loop.v → test_for_loop.v**
- Tests `for` loop construct in `initial` block
- Content: Loop to initialize array with indices
- Feature status: Parser doesn't support `for`, `initial`, or `integer` type (as of v0.2)

**function.v → test_function.v**
- Tests `function` declaration and function calls
- Content: Function that doubles input value via left shift
- Feature status: Parser doesn't support `function` keyword (as of v0.2)

**generate.v → test_generate.v**
- Tests `generate` block with `genvar` loop
- Content: Generate 4 wire declarations using loop
- Feature status: Parser doesn't support `generate` or `genvar` (as of v0.2)

## Implementation Notes

### Why These Were Missed in REV5

REV5 renamed 36 files in `verilog/pass/` directory but missed these 3 files in the root `verilog/` directory. The separate locations may have caused them to be overlooked during the bulk rename operation.

### Naming Pattern Consistency

**Before:**
- `verilog/pass/` - Mixed naming (`adder.v`, `test_*.v`)
- `verilog/` - Mixed naming (`for_loop.v`, `test_*.v`)

**After REV6:**
- `verilog/pass/` - All files use `test_` prefix (36 files)
- `verilog/` - All files use `test_` prefix (24 files total: 21 from REV5 + 3 from REV6)
- **100% consistency** across entire test suite

### Test Organization Maintained

**Location strategy:**
- `verilog/pass/` - Features that work in v0.2 (parse, elaborate, codegen)
- `verilog/` - Features not yet implemented (will fail at various stages)

**These three files in root because:**
- `test_for_loop.v` - Requires loop support, `initial` blocks, `integer` type
- `test_function.v` - Requires function/task support
- `test_generate.v` - Requires generate block and genvar support

## Test File Details

### test_for_loop.v
```verilog
module for_test;
  integer i;
  reg [7:0] arr [0:7];
  initial for (i = 0; i < 8; i = i + 1)
    arr[i] = i;
endmodule
```
**Features needed:** `integer` type, `initial` blocks, `for` loops, memory arrays

### test_function.v
```verilog
module func_test(input [7:0] a, output [7:0] y);
  function [7:0] double;
    input [7:0] x;
    double = x << 1;
  endfunction
  assign y = double(a);
endmodule
```
**Features needed:** `function` declarations, function calls, function-local inputs

### test_generate.v
```verilog
module gen_test;
  generate
    genvar i;
    for (i = 0; i < 4; i = i + 1) begin
      wire [7:0] data;
    end
  endgenerate
endmodule
```
**Features needed:** `generate` blocks, `genvar` declarations, generate loops

## Known Gaps and Limitations

Same as REV5 - no implementation changes.

**Note on these tests:**
- Originally added in REV3 ("More test cases" commit)
- Document expected behavior for unimplemented features
- Will be moved to `pass/` once features are implemented
- Parser will reject these files with syntax errors (as of v0.2)

## Statistics

- **Files changed**: 3
- **Lines added**: 0 (pure renames)
- **Lines removed**: 0 (pure renames)
- **Net change**: 0 lines

**Test suite totals after REV6:**
- `verilog/pass/`: 36 files (all with `test_` prefix)
- `verilog/`: 24 files (all with `test_` prefix)
- **Total**: 60 test files, 100% naming consistency

This commit achieves complete naming consistency across the entire test suite, making test discovery and organization clearer for future development.
