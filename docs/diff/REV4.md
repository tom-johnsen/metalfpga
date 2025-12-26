# REV4 - v0.2 More keywords, 4-State logic, enhanced MSL

**Commit:** 0e0f09b
**Date:** Thu Dec 25 15:59:54 2025 +0100
**Message:** v0.2 - More keywords added, 4-State added, MSL code enhanced, still no host emission or simulation.

## Overview

Major v0.2 release implementing 4-state logic (0/1/X/Z), case statements, constant folding, and massively expanded MSL codegen. This is the largest commit by far (+5,276 lines) and represents a fundamental shift from 2-state to 4-state Verilog semantics.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added `case`/`casex`/`casez`, `always @(*)`, memory arrays, reduction ops |
| **Elaborate** | ✓ Enhanced | Constant propagation, 4-state evaluation, better parameter handling |
| **Codegen (2-state)** | ✓ MSL emission | Still supported but deprecated |
| **Codegen (4-state)** | ✓ MSL emission | **NEW**: Full 4-state logic with X/Z propagation |
| **Host emission** | ✗ Stubbed only | Minimal progress |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Verilog Features:**
- **Case statements**: `case`, `casex`, `casez` with default
- **4-state literals**: `4'b101x`, `8'hzz`, `1'bz`
- **Reduction operators**: `&`, `|`, `^`, `~&`, `~|`, `~^` (unary)
- **Signed operators**: `>>>` (arithmetic right shift)
- **Combinational always**: `always @(*)` sensitivity inference
- **Memory arrays**: `reg [7:0] mem [0:255];`

**Enhanced Features:**
- Constant folding at elaboration time (parameters computed)
- 4-state constant propagation
- Better handling of X/Z in expressions
- Part-select improvements

**New Documentation:**
- `docs/VERILOG_REFERENCE.md` - Comprehensive 362-line Verilog language reference covering 4-state logic, operators, and semantics

## Architecture Changes

### 4-State Value Representation

**New struct** `FourStateValue` in `ast.hh`:
```cpp
struct FourStateValue {
  uint64_t value_bits;  // Actual 0/1 values
  uint64_t x_bits;      // 1 = X (unknown)
  uint64_t z_bits;      // 1 = Z (high-impedance)
  int width;            // Bit width
};
```

**Encoding:**
- Each bit position has 3 flags: value, x, z
- If `x_bits[i] == 1`: bit is X (unknown)
- If `z_bits[i] == 1`: bit is Z (high-Z)
- Otherwise: bit is determined by `value_bits[i]` (0 or 1)

**Example**: `4'b10xz`
- `value_bits = 0b1000` (bit 3 set)
- `x_bits     = 0b0010` (bit 1 is X)
- `z_bits     = 0b0001` (bit 0 is Z)

## Implementation Details

### Frontend: AST Extensions

**File**: `src/frontend/ast.hh` (+60 lines)

**New enums and structs:**
```cpp
enum class CaseKind { kCase, kCaseZ, kCaseX };

struct CaseItem {
  std::vector<std::unique_ptr<Expr>> labels;  // Match values
  std::vector<Statement> body;                // Statements if match
};
```

**Extended Statement:**
```cpp
struct Statement {
  // ... existing fields ...
  CaseKind case_kind;
  std::unique_ptr<Expr> case_expr;           // Expression to match
  std::vector<CaseItem> case_items;          // Case branches
  std::vector<Statement> default_case;       // Default branch
};
```

**File**: `src/frontend/ast.cc` (+461 lines, was empty)

**New utility functions:**
- `CloneExpr()` - Deep copy expressions
- `EvaluateConstExpr()` - Constant-fold expressions to 4-state values
- `MaskForWidth()`, `MinimalWidth()` - Width utilities
- `ResizeValue()`, `NormalizeUnknown()` - 4-state value manipulation
- `MergeUnknown()` - Combine two 4-state values (for X propagation)

**4-State arithmetic**:
Implements full Verilog semantics:
- `X + anything = X`
- `Z + anything = X`
- Bitwise ops propagate X/Z
- Logical ops: `X && 0 = 0`, `X && 1 = X`, etc.

### Frontend: Parser Extensions

**File**: `src/frontend/verilog_parser.cc` (+639 lines)

**New parsing:**
- **Case statements**: Parses `case (expr)` with multiple labels per item
  - `casex`: X values are wildcards in labels
  - `casez`: Z and X values are wildcards in labels
- **4-state literals**: `4'bx01z`, `8'hzz`, etc.
- **Reduction operators**: `&a`, `|b`, `^c` (unary, not binary)
- **Always @(*)**: Automatic sensitivity
- **Memory declarations**: `reg [width] name [range];`
- **Signed shifts**: `>>>` arithmetic right shift

**Example case statement parsing:**
```verilog
case (sel)
  2'b00, 2'b01: out = 8'h00;  // Multiple labels
  2'bx1:        out = 8'hFF;  // Wildcard (casex)
  default:      out = 8'h55;
endcase
```

### Elaboration: Constant Folding

**File**: `src/core/elaboration.cc` (+1,132 lines!)

**Major new capabilities:**

`FoldConstantExpr()`:
- Evaluates expressions at elaborate-time if all operands are constants
- Returns `FourStateValue` result
- Handles:
  - Arithmetic: `+`, `-`, `*`, `/`, `%`
  - Bitwise: `&`, `|`, `^`, `~`
  - Shifts: `<<`, `>>`, `>>>`
  - Comparison: `==`, `!=`, `<`, `>`, etc. (with X propagation)
  - Ternary: `cond ? a : b` (X in cond → X result)
  - Concat/replication

**Parameter evaluation:**
- Parameters computed during elaboration
- Substituted into expressions before flattening
- Allows `parameter WIDTH = 8; wire [WIDTH-1:0] bus;`

**Constant propagation:**
- Detects `assign wire = constant;`
- Propagates constant through uses
- Simplifies downstream logic

### Codegen: 4-State MSL

**File**: `src/codegen/msl_codegen.cc` (+2,131 lines!)

This is the biggest change - complete rewrite of MSL emission for 4-state.

**Value encoding in MSL:**
Each Verilog signal becomes TWO MSL values:
```metal
uint value;  // 0/1 bits
uint xz;     // X/Z bits (1 = unknown/hi-z)
```

**Operator emission examples:**

**Bitwise AND:**
```metal
// Verilog: c = a & b
// MSL:
c_value = a_value & b_value;
c_xz = a_xz | b_xz;  // X/Z propagates
```

**Addition:**
```metal
// Verilog: c = a + b
// MSL:
c_xz = a_xz | b_xz;  // Any unknown → result unknown
c_value = (c_xz) ? 0 : (a_value + b_value);
```

**Equality (logical, not case):**
```metal
// Verilog: c = (a == b)
// MSL:
c_xz = a_xz | b_xz;  // Unknown operands → unknown result
c_value = (c_xz) ? 0 : (a_value == b_value);
```

**Case equality (===):**
```metal
// Verilog: c = (a === b)
// MSL (exact match including X/Z):
c_value = ((a_value == b_value) && (a_xz == b_xz)) ? 1 : 0;
c_xz = 0;  // Always returns 0 or 1, never X
```

**Case statement emission:**
```metal
// Verilog:
// case (sel)
//   0: out = 8'h00;
//   1: out = 8'hFF;
//   default: out = 8'h55;
// endcase

// MSL:
if ((sel_xz == 0) && (sel_value == 0)) {
  out_value = 0x00; out_xz = 0;
} else if ((sel_xz == 0) && (sel_value == 1)) {
  out_value = 0xFF; out_xz = 0;
} else {
  out_value = 0x55; out_xz = 0;
}
```

**Casex/casez emission:**
Wildcards implemented as bitwise masks:
```metal
// casex (sel) 2'bx1: ...
// Match if bit 0 is 1, bit 1 is don't-care
uint mask = 0b01;  // Only check bit 0
if ((sel_value & mask) == (0b01 & mask)) { ... }
```

### CLI Enhancements

**File**: `src/main.mm` (+164 lines)

**Improved --dump-flat:**
- Shows 4-state literal values: `4'bx01z`
- Pretty-prints case statements
- Displays constant-folded parameter values

## Test Coverage

**New 4-state tests (7 files):**
- `test_case_4state.v` - Case with X/Z values
- `test_case_basic.v`, `test_case_mixed.v`
- `test_casex.v`, `test_casex_simple.v` - X wildcards
- `test_casez.v`, `test_casez_simple.v` - Z wildcards

**Other new tests (14 files):**
- Memory operations, reduction ops, signed ops
- Large widths, part selects, empty ports
- Initial blocks, inout ports, multi-dim arrays

**Moved to pass/ (5 files):**
- `always_star.v`, `array.v`, `case_statement.v`, `recursive_module.v`, `undeclared_clock.v`

## Known Gaps and Limitations

### v0.2 Improvements Over v0.1
**Fixed:**
- 4-state logic fully implemented
- Case statements working
- Constant folding operational
- Better parameter handling

**Still Missing:**
- Loops (`for`, `while`, `repeat`) - parser doesn't support yet
- Functions/tasks - not implemented
- `generate` blocks - not implemented
- Multi-dimensional arrays - parsed but codegen incomplete
- Memories - partially supported
- Signed arithmetic - parsed but not fully implemented in codegen
- Host code emission - still stubbed
- Runtime - no execution

### Semantic Simplifications (v0.2)
- **Signed ops**: Parsed but treated as unsigned in most contexts
- **Shift overflow**: Still has edge cases
- **X/Z in shifts**: May not match Verilog spec exactly

## Statistics

- **Files changed**: 43
- **Lines added**: 5,276
- **Lines removed**: 323
- **Net change**: +4,953 lines

**Major components:**
- MSL codegen: +2,131 lines (4-state operators, case statements)
- Elaboration: +1,132 lines (constant folding, 4-state eval)
- Parser: +639 lines (case, reduction ops, 4-state literals)
- AST implementation: +461 lines (constant evaluation)
- Documentation: +362 lines (Verilog reference)

**Test coverage:**
- 21 new test files
- 5 tests moved to pass/

This is the pivotal commit transforming metalfpga from a 2-state toy to a legitimate 4-state Verilog compiler.
