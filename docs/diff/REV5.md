# REV5 - Test suite reorganization and expansion

**Commit:** 1d25793
**Date:** Thu Dec 25 16:19:05 2025 +0100
**Message:** Fixed verilog test naming

## Overview

Test-only commit focusing on consistent naming conventions and comprehensive coverage expansion. Renamed 36 existing tests with `test_` prefix for consistency and added 21 new test suites targeting reduction operators, signed arithmetic, and multi-file module support. No source code changes.

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

**Test Organization:**
- All 36 passing tests in `verilog/pass/` now follow `test_<descriptive_name>.v` naming convention
- Previously had inconsistent names like `adder.v`, `everything.v`, `lit_matrix.v`
- Now have clear names like `test_simple_adder.v`, `test_mixed_operations.v`, `test_literal_formats.v`

**New Test Coverage:**
- **Reduction operators**: 7 comprehensive test suites (basic, nested, slices, wide buses, case, ternary, always blocks)
- **Signed arithmetic**: 10 comprehensive test suites (division, shifts, edge cases, width extension, mixed operations)
- **Multi-file modules**: 4 test files demonstrating same-file multi-module definitions
- All new tests in `verilog/` root (unimplemented features, not in `pass/`)

## Test Renaming Details

### File Renames (36 files)

**Arithmetic and Operations:**
- `adder.v` → `test_simple_adder.v` - Basic addition
- `aluadder.v` → `test_alu_with_flags.v` - ALU with flag outputs
- `shifter.v` → `test_shift_ops.v` - Shift operations
- `shiftmaskconcat.v` → `test_shift_mask_concat.v` - Combined shift/mask/concat
- `shiftprescedence.v` → `test_shift_precedence.v` - Operator precedence

**Control Flow and Logic:**
- `case_statement.v` → `test_case_statement.v`
- `always_star.v` → `test_always_star.v` - Combinational always @(*)
- `negedge.v` → `test_negedge.v` - Negative edge triggering
- `multi_always.v` → `test_multi_always.v` - Multiple always blocks
- `sequcomb.v` → `test_sequential_combinational.v` - Mixed sequential/combinational

**Data Manipulation:**
- `selects.v` → `test_bit_selects.v` - Bit/part selection
- `concat_nesting.v` → `test_concat_nesting.v` - Nested concatenation
- `repl_nesting.v` → `test_replication_nesting.v` - Nested replication
- `nestedternary.v` → `test_nested_ternary.v` - Nested ternary operators

**Memory:**
- `array.v` → `test_memory_array.v` - Memory array declarations
- `memwrite.v` → `test_memory_write.v` - Memory write operations

**Module Hierarchy:**
- `nestedmodules.v` → `test_nested_modules.v` - Module instantiation hierarchy
- `ghostinstancechain.v` → `test_instance_chain.v` - Chain of instances
- `recursive_module.v` → `test_recursive_module.v` - Recursive module instantiation

**Parameters:**
- `param_complex_expr.v` → `test_param_complex_expr.v` - Complex parameter expressions
- `paramshadow.v` → `test_param_shadow.v` - Parameter shadowing
- `zerowidth.v` → `test_zero_width_param.v` - Zero-width parameter handling

**Edge Cases:**
- `everything.v` → `test_mixed_operations.v` - Mixed operation stress test
- `lit_matrix.v` → `test_literal_formats.v` - Various literal formats
- `nowhitespace.v` → `test_no_whitespace.v` - Parsing without whitespace
- `overshift.v` → `test_overshift.v` - Shift beyond width
- `parens.v` → `test_parentheses.v` - Parenthesis handling
- `unarychains.v` → `test_unary_chains.v` - Chained unary operators
- `unsized_vs_sized.v` → `test_unsized_vs_sized.v` - Sized/unsized literal mixing

**Diagnostics:**
- `blocking_nonblocking.v` → `test_blocking_nonblocking.v` - Assignment types
- `comb_loop.v` → `test_comb_loop.v` - Combinational loop detection
- `comparison_all.v` → `test_comparison_all.v` - All comparison operators
- `multidriver.v` → `test_multidriver.v` - Multi-driver detection
- `undeclared_clock.v` → `test_undeclared_clock.v` - Clock validation
- `widthmismatch.v` → `test_width_mismatch.v` - Width mismatch handling
- `wire_with_init.v` → `test_wire_init.v` - Wire initialization

## New Test Coverage

### Reduction Operator Tests (8 files, 243 lines)

**test_reduction_comprehensive.v** (61 lines):
```verilog
// Tests all 6 reduction operators: &, |, ^, ~&, ~|, ~^
// With various widths: 1-bit, 4-bit, 16-bit, 32-bit
assign and_results[0] = &wide_data;   // All bits must be 1
assign or_results[0] = |wide_data;    // Any bit is 1
assign xor_results[0] = ^wide_data;   // Odd parity
assign nand_results[0] = ~&wide_data; // NOT all bits are 1
```

**test_reduction_nested.v** (28 lines):
- Reduction operators on nested expressions
- Example: `&(a | b)`, `|(a & b)`, `^(~a)`

**test_reduction_slices.v** (26 lines):
- Reduction on part-selects and bit-slices
- Example: `&data[7:4]`, `|bus[15:8]`

**test_reduction_wide_buses.v** (36 lines):
- 32-bit, 64-bit, and 128-bit reduction operations
- Tests performance with large bit widths

**test_reduction_case.v** (19 lines):
- Reduction operators in case statement expressions
- Example: `case(&data) 1'b0: ... 1'b1: ... endcase`

**test_reduction_ternary.v** (22 lines):
- Reduction in ternary operator conditions
- Example: `assign out = (&sel) ? a : b;`

**test_reduction_in_always.v** (24 lines):
- Reduction operators in sequential always blocks
- Example: `always @(posedge clk) if (|enable) data <= in;`

**test_combined_reduction_signed.v** (27 lines):
- Mixed reduction and signed arithmetic
- Example: `assign out = (&flags) ? signed_a + signed_b : 0;`

### Signed Arithmetic Tests (10 files, 320 lines)

**test_signed_comprehensive.v** (47 lines):
```verilog
// Signed operations at various widths
input signed [7:0] a, b;
assign add_8bit = a + b;      // Signed addition
assign mul_8bit = a * b;      // Signed multiplication
assign cmp_lt_8 = a < b;      // Signed comparison
```

**test_signed_division.v** (31 lines):
- Signed division and modulo: `a / b`, `a % b`
- Negative dividend and divisor handling

**test_signed_shift.v** (29 lines):
- Arithmetic right shift: `>>>`
- Sign extension behavior
- Example: `8'sb11110000 >>> 2` → `8'sb11111100` (sign extends)

**test_signed_edge_cases.v** (36 lines):
- Most negative value: `-128` for 8-bit
- Overflow conditions
- Mixed signed/unsigned expressions

**test_signed_width_extension.v** (35 lines):
- Sign extension when widths mismatch
- Example: `assign [15:0] wide = signed [7:0] narrow;` (sign extends)

**test_signed_mixed.v** (31 lines):
- Mixed signed and unsigned operations
- Casting behavior

**test_signed_unary.v** (32 lines):
- Unary minus on signed values: `-a`
- Two's complement behavior

**test_signed_case.v** (30 lines):
- Signed values in case statements
- Negative literal matching

**test_signed_ternary.v** (19 lines):
- Signed arithmetic in ternary operators
- Example: `(a < 0) ? -a : a` (absolute value)

**test_signed_in_always.v** (30 lines):
- Signed operations in sequential blocks
- Register sign preservation

### Multi-File Module Tests (3 files, 49 lines)

**test_multifile_combined.v** (24 lines):
```verilog
// Multiple modules in one file
module adder_mod(
  input wire [7:0] a, b,
  output wire [7:0] sum
);
  assign sum = a + b;
endmodule

module top(...);
  // Reference adder_mod from same file
  adder_mod u_add(.a(x), .b(y), .sum(result));
endmodule
```

**test_multifile_a.v** (9 lines):
- Standalone module A definition

**test_multifile_b.v** (16 lines):
- Standalone module B definition

## Implementation Notes

### Naming Convention Rationale

**Old inconsistencies:**
- `adder.v` - generic noun
- `everything.v` - vague name
- `lit_matrix.v` - abbreviated
- `ghostinstancechain.v` - compound word

**New convention:**
- Prefix: `test_` to clearly identify as test file
- Descriptive name: explains what is being tested
- Underscores: separate words for readability
- Examples: `test_simple_adder.v`, `test_mixed_operations.v`, `test_literal_formats.v`, `test_instance_chain.v`

### Test Organization Strategy

**Location-based classification:**
- `verilog/pass/` - Tests that successfully parse, elaborate, and emit MSL (36 files)
- `verilog/` - Tests for features under development or not yet implemented (21 new files)

**Why new tests NOT in pass/:**
Reduction and signed arithmetic tests placed in root because:
- Reduction operators parsed (v0.2) but codegen may be incomplete
- Signed arithmetic parsed (v0.2) but noted as "treated as unsigned in most contexts" (REV4)
- Multi-file module support not confirmed working
- Requires validation before moving to `pass/`

### Test Coverage Strategy

**Reduction operators:**
- Comprehensive coverage: all 6 operators (&, |, ^, ~&, ~|, ~^)
- Width diversity: 1-bit to 128-bit
- Context diversity: assigns, case, ternary, always blocks, nested expressions

**Signed arithmetic:**
- Full operation coverage: +, -, *, /, %, comparisons
- Special cases: arithmetic shift (>>>), unary minus
- Edge cases: overflow, most negative, width extension
- Integration: with case, ternary, always blocks

## Known Gaps and Limitations

Same as REV4 - no new implementation.

**Note on new tests:**
- These tests document EXPECTED behavior for reduction/signed features
- Parser supports syntax (REV4)
- Elaboration and codegen implementation status unclear
- Tests serve as specification for future implementation validation

## Statistics

- **Files changed**: 57
- **Lines added**: 612 (all test code)
- **Lines removed**: 0 (renames don't count as deletions)
- **Net change**: +612 lines

**Test breakdown:**
- Renamed: 36 files (consistency improvement)
- Reduction tests: 8 files, 243 lines
- Signed arithmetic tests: 10 files, 320 lines
- Multi-file tests: 3 files, 49 lines

**Total test count:**
- `verilog/pass/`: 36 passing tests
- `verilog/`: 21 new feature tests
- Total: 57 test files (+37.5% expansion)

This commit establishes consistent naming and provides comprehensive test coverage for v0.2 features, creating a foundation for validating future implementation work.
