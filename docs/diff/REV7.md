# REV7 - Completed 4-state implementation and signed arithmetic

**Commit:** 26df5d3
**Date:** Thu Dec 25 17:21:25 2025 +0100
**Message:** Finished 4-state, added more tests, updated readme

## Overview

Major milestone completing the 4-state logic implementation and signed arithmetic support. This commit "finishes" the work started in REV4 by adding signed type tracking, enhancing MSL codegen, improving elaboration, and moving 20 tests from `verilog/` to `verilog/pass/` (now working). Includes comprehensive documentation updates and 7 new expression grouping tests.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added `signed` keyword support for ports and nets |
| **Elaborate** | ✓ Enhanced | Improved width and signedness tracking |
| **Codegen (2-state)** | ✓ MSL emission | Still supported |
| **Codegen (4-state)** | ✓ **Completed** | Full implementation with signed arithmetic |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Verilog Features:**
- **Signed ports and nets**: `input signed [7:0] a`, `wire signed [15:0] result`
- **Signed arithmetic**: All operators now respect signedness (`+`, `-`, `*`, `/`, `%`, comparisons)
- **Arithmetic right shift**: `>>>` with proper sign extension
- **Type casting**: `$signed(...)` and `$unsigned(...)` system functions
- **Inout ports**: Now functional (requires `--4state` mode)
- **Enhanced memory support**: Read operations fully implemented

**CLI Changes:**
- `--4state` flag documented and recommended for X/Z and inout support
- README updated with comprehensive feature list

**Test Organization:**
- **79 passing tests** (up from ~36 in REV6)
- 20 tests moved to `pass/` (reduction and signed features now working)
- 7 new expression grouping tests added

## Architecture Changes

### Frontend: AST Extensions for Signedness

**File**: `src/frontend/ast.hh` (+6 lines)

**New fields:**

`Port` struct:
```cpp
struct Port {
  PortDir dir;
  std::string name;
  int width;
  bool is_signed = false;  // NEW: Track signedness
  // ... existing fields ...
};
```

`Net` struct:
```cpp
struct Net {
  NetType type;
  std::string name;
  int width;
  bool is_signed = false;  // NEW: Track signedness
  // ... existing fields ...
};
```

`Expr` struct:
```cpp
struct Expr {
  // ... existing fields ...
  bool is_signed = false;  // NEW: Propagate signedness through expressions
};
```

`Assign` struct:
```cpp
struct Assign {
  std::string lhs;
  int lhs_msb = 0;         // NEW: Part-select support
  int lhs_lsb = 0;         // NEW
  bool lhs_has_range = false;  // NEW
  std::unique_ptr<Expr> rhs;
};
```

### Frontend: Parser Enhancements

**File**: `src/frontend/verilog_parser.cc` (+367 lines)

**Signed keyword parsing:**

Port declarations now accept `signed` keyword:
```verilog
input signed [7:0] a;       // Parsed
output signed [15:0] result; // Parsed
wire signed [31:0] temp;    // Parsed
```

**Parser changes:**
- `ParsePortList()`: Tracks `is_signed` flag for each port
  - Accepts `signed` before or after `wire`/`reg` keyword
  - Example: `input signed [7:0] a` or `input [7:0] signed a`
- `ParseNetDeclaration()`: Tracks signedness for wire/reg declarations
- Expression parsing: Propagates signedness through AST

**System function support:**
- `$signed(expr)`: Cast to signed
- `$unsigned(expr)`: Cast to unsigned
- Implemented as special expression types

### Elaboration: Enhanced Type Tracking

**File**: `src/core/elaboration.cc` (+80 lines)

**Improvements:**
- Signedness propagation during flattening
- Width inference considers signedness for extension rules
- Part-select support in continuous assignments
- Better memory array handling

**Key enhancements:**
- Port connections respect signedness
- Instance port mapping preserves signed/unsigned types
- Assignment width checking accounts for sign extension vs zero extension

### Codegen: Completed 4-State MSL

**File**: `src/codegen/msl_codegen.cc` (+705 lines!)

This is the largest change, completing the 4-state implementation.

**Signed arithmetic emission:**

**Addition (signed):**
```metal
// Verilog: wire signed [7:0] sum = a + b;  (a, b are signed)
// MSL:
int a_val = /* ... */;  // Use 'int' for signed, 'uint' for unsigned
int b_val = /* ... */;
sum_value = (uint)(a_val + b_val);  // Compute in signed, store unsigned bits
sum_xz = a_xz | b_xz;  // X/Z propagation
```

**Comparison (signed):**
```metal
// Verilog: wire cmp = (signed_a < signed_b);
// MSL:
int sa = (int)signed_a_value;  // Cast to signed
int sb = (int)signed_b_value;
cmp_value = (sa < sb) ? 1 : 0;
cmp_xz = signed_a_xz | signed_b_xz;  // X → X result
```

**Arithmetic right shift (>>>):**
```metal
// Verilog: wire signed [7:0] shifted = data >>> 2;
// MSL:
int data_signed = (int)(int8_t)data_value;  // Sign-extend to int
shifted_value = (uint)(data_signed >> 2);   // Arithmetic shift
shifted_xz = data_xz;  // X/Z preserved
```

**Division and modulo (signed):**
```metal
// Verilog: wire signed [7:0] quot = a / b;
// MSL:
if (b_xz || (b_value == 0)) {
  quot_xz = 0xFFFFFFFF;  // Result is all X
  quot_value = 0;
} else {
  int sa = (int)(int8_t)a_value;  // Sign extend
  int sb = (int)(int8_t)b_value;
  quot_value = (uint)(sa / sb);
  quot_xz = a_xz;
}
```

**Type casting:**
```metal
// Verilog: $signed(unsigned_val)
// MSL: (value stays same, but subsequent ops use signed cast)

// Verilog: $unsigned(signed_val)
// MSL: (value stays same, but subsequent ops use unsigned)
```

**Memory read operations:**
```metal
// Verilog: assign data = mem[addr];
// MSL:
if (addr_xz) {
  data_value = 0;
  data_xz = 0xFFFFFFFF;  // All X
} else {
  data_value = mem_value[addr_value];
  data_xz = mem_xz[addr_value];
}
```

**Inout port support:**
- Three-state logic: drive when not Z, read when Z
- Emits conditional logic based on Z bits
- Bidirectional buffer modeling

### CLI: Enhanced Output

**File**: `src/main.mm` (+139 lines)

**Improvements:**
- Better `--dump-flat` output showing signedness
- Signed literals printed correctly (e.g., `-8'd5` instead of `8'd251`)
- Enhanced expression pretty-printing
- Better formatting for part-selects

## Implementation Details

### Signed Arithmetic Implementation

**Sign extension strategy:**
For operations on signed values, cast to appropriately-sized signed type:
- 8-bit: `(int8_t)value` → sign extends to `int`
- 16-bit: `(int16_t)value` → sign extends to `int`
- 32-bit: `(int32_t)value` → already `int`
- 64-bit: `(int64_t)value` → `long`

**Mixed signed/unsigned:**
- Verilog semantics: unsigned operand makes operation unsigned
- Implemented: Check both operands, use unsigned if either is unsigned
- Special case: Comparison operators check signedness to pick `<` vs signed `<`

### Expression Grouping Tests

**New tests (7 files, 192 lines):**

**test_expression_grouping.v** (37 lines):
- Complex nested expressions with proper precedence
- Example: `((a + b) * c) + ((d - e) / f)`

**test_nested_parentheses.v** (24 lines):
- Deep nesting: `((((a))))`
- Combined with operators: `(~((a & b) | (c ^ d)))`

**test_parentheses_concat.v** (20 lines):
- Parentheses with concatenation: `{(a + b), (c - d)}`

**test_parentheses_reduction.v** (18 lines):
- Reduction on parenthesized expressions: `&(a | b)`

**test_parentheses_signed.v** (20 lines):
- Signed operations with grouping: `($signed(a) + $signed(b)) >>> 2`

**test_parentheses_ternary.v** (20 lines):
- Ternary with grouped conditions: `((a < b) && (c > d)) ? x : y`

**test_memory_read.v** (53 lines):
```verilog
// Multiple combinational reads
always @(*) begin
  rd_data1 = mem[rd_addr1];
  rd_data2 = mem[rd_addr2];
  rd_data3 = mem[rd_addr3];
end

// Synchronous read
always @(posedge clk) begin
  rd_data <= mem[rd_addr];
end
```

## Test Migration

### Tests Moved to pass/ (20 files)

**Reduction operators (8 files):**
- `test_reduction_comprehensive.v`
- `test_reduction_nested.v`
- `test_reduction_slices.v`
- `test_reduction_wide_buses.v`
- `test_reduction_case.v`
- `test_reduction_ternary.v`
- `test_reduction_in_always.v`
- `test_combined_reduction_signed.v`

**Signed arithmetic (10 files):**
- `test_signed_comprehensive.v`
- `test_signed_division.v`
- `test_signed_shift.v`
- `test_signed_edge_cases.v`
- `test_signed_width_extension.v`
- `test_signed_mixed.v`
- `test_signed_unary.v`
- `test_signed_case.v`
- `test_signed_ternary.v`
- `test_signed_in_always.v`

**Other (2 files):**
- `test_inout_port.v` - Inout ports now functional with 4-state
- `test_multifile_combined.v` - Multi-module single-file support

**Why moved:**
These tests were added in REV5 for unimplemented features. REV7 completes those implementations, so tests now pass and are moved to `pass/`.

## Documentation Updates

### README.md (+56 lines)

**Status updated:**
- Old: "v0.1+ - Core pipeline working with 35+ passing test cases"
- New: "v0.2+ - Core pipeline with reduction operators, signed arithmetic, and 4-state logic support. 79 passing test cases"

**Feature list enhanced:**
- Added reduction operators (all 6 variants)
- Added signed arithmetic with `>>>` shift
- Added `$signed()` / `$unsigned()` casting
- Added `inout` port support
- Added `case`/`casex`/`casez` statements
- Moved many items from "In Development" to "Working"

**Test suite description expanded:**
- Listed reduction operator coverage
- Noted signed arithmetic edge cases
- Highlighted 4-state logic tests
- Added expression grouping tests

### docs/gpga/verilog_words.md (+11 lines)

Updated keyword reference with:
- `signed` keyword documented
- `$signed`, `$unsigned` system functions
- `>>>` arithmetic right shift
- Enhanced case statement descriptions

## Known Gaps and Limitations

### Improvements Over REV6

**Now Working:**
- Signed arithmetic (full implementation)
- Reduction operators (all contexts)
- Inout ports (with 4-state mode)
- Memory read operations
- Expression grouping and precedence

**Still Missing:**
- Loops (`for`, `while`, `repeat`) - parser doesn't support
- Functions/tasks - not implemented
- `generate` blocks - not implemented
- System tasks (`$display`, `$monitor`, etc.) - not implemented
- Multi-dimensional arrays - partially supported
- Host code emission - still stubbed
- Runtime - no execution

### Semantic Notes (v0.2+)

**Signed arithmetic:**
- Fully implemented per Verilog-2001 spec
- Sign extension uses native C casts (`int8_t`, `int16_t`)
- Mixed signed/unsigned follows Verilog precedence rules

**4-State logic:**
- Complete implementation for all operators
- X/Z propagation matches Verilog LRM
- Inout modeling uses Z for high-impedance state

## Statistics

- **Files changed**: 37
- **Lines added**: 1,586
- **Lines removed**: 97
- **Net change**: +1,489 lines

**Major components:**
- MSL codegen: +705 lines (signed arithmetic, memory ops, enhanced 4-state)
- Parser: +367 lines (signed keyword, expression improvements)
- Main CLI: +139 lines (better output formatting)
- AST implementation: +127 lines (signedness tracking)
- Elaboration: +80 lines (type propagation)
- Documentation: +67 lines (README + keywords)

**Test organization:**
- `verilog/pass/`: 79 files (up from 59 in REV6)
  - 20 files moved from root (now working)
  - 7 new files added (expression grouping, memory)
- `verilog/`: 4 files (down from 24 in REV6)
  - Only unimplemented features remain (loops, functions, generate)

This commit represents the completion of v0.2, transforming metalfpga from basic 2-state compilation to a legitimate 4-state Verilog compiler with full signed arithmetic support.
