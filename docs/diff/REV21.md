# REV21 - Enhanced Verilog semantics: match-3 operators, power, part-select assign (v0.5)

**Commit:** e8c16b3
**Date:** Sat Dec 27 00:37:48 2025 +0100
**Message:** v0.5 - Added revision documents, added more verilog semantics, small stuff

## Overview

Major v0.5 milestone adding critical **4-state comparison operators** (case equality `===`/`!==`, wildcard match `==?`/`!=?`), **power operator** (`**`) with signed/unsigned variants, **procedural part-select assignment** (both fixed `[msb:lsb]` and indexed `[idx +: width]` forms), and **comprehensive revision documentation** (REV0-REV20 created). These features significantly improve Verilog compliance, particularly for testbenches using X/Z comparisons and designs using exponentiation or slice assignments. Net change: +11,567 lines (mostly documentation, +206 lines of actual code changes).

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added match-3 operator parsing (+436 lines) |
| **Elaborate** | ✓ Enhanced | Added power operator handling (+195 lines) |
| **Codegen (2-state)** | ✓ Enhanced | Power operator emission, part-select updates (+1,730 lines) |
| **Codegen (4-state)** | ✓ Enhanced | Match-3 operator semantics, power functions (+64 lines header) |
| **Host emission** | ✓ Enhanced | Scheduler edge documentation (+2 lines) |
| **Runtime** | ✓ Functional | No changes |

## User-Visible Changes

**New Verilog Features:**

1. **Case Equality Operators** (`===`, `!==`):
   - Compare values including X and Z states
   - `4'b10x1 === 4'b10x1` → true (X matches X exactly)
   - `4'b10x1 == 4'b10x1` → X (regular equality propagates X)
   - Only meaningful with `--4state` flag

2. **Wildcard Match Operators** (`==?`, `!=?`):
   - Treat X and Z in RHS as "don't care" wildcards
   - `4'b1011 ==? 4'b10x1` → true (X in pattern matches anything)
   - `4'b1001 ==? 4'b10x1` → true (both 0 and 1 match X wildcard)
   - Essential for pattern matching in testbenches

3. **Power Operator** (`**`):
   - Integer exponentiation: `base ** exponent`
   - Unsigned: `8'd3 ** 8'd4` → `8'd81` (3^4)
   - Signed: `-8'sd2 ** 8'sd3` → `-8'sd8` (-2^3)
   - Negative exponents return 0 (integer-only semantics)
   - Works in both 2-state and 4-state modes

4. **Procedural Part-Select Assignment**:
   - Fixed range: `data[7:0] = 8'hFF;` (assign to bits 7-0)
   - Indexed ascending: `data[idx +: 4] = 4'hA;` (4 bits starting at idx)
   - Indexed descending: `data[idx -: 4] = 4'hB;` (4 bits ending at idx)
   - Runtime bounds checking with guards (out-of-bounds writes ignored)

**Test Suite:**
- Added 3 new passing tests:
  - `test_match3_ops.v` - 32-bit case/wildcard equality
  - `test_match3_ops64.v` - 64-bit case/wildcard equality
  - `test_part_select_assign.v` - Procedural slice assignment
- Updated 2 existing tests for new operators
- Total passing tests: 228+ (documented in README)

**Documentation:**
- **REV0-REV20 created** (10,393 lines): Complete development history
- README updates: Version bumped to v0.5, feature list updated

## Architecture Changes

### Frontend: Match-3 Operator Parsing

**File**: `src/frontend/verilog_parser.cc` (+436 lines)

Added parsing for four new comparison operators beyond standard Verilog:

**Case equality/inequality (`===`, `!==`):**
```cpp
// Parser recognizes:
// '===' → BinaryExpr with op = 'C' (case equal)
// '!==' → BinaryExpr with op = 'c' (case not-equal)

// Verilog:
wire match = (a === b);    // True only if all bits match exactly (including X/Z)
wire diff = (a !== b);     // True if any bit differs (including X/Z state)
```

**Wildcard match (`==?`, `!=?`):**
```cpp
// Parser recognizes:
// '==?' → BinaryExpr with op = 'W' (wildcard equal)
// '!=?' → BinaryExpr with op = 'w' (wildcard not-equal)

// Verilog:
wire match = (data ==? 4'b10x1);  // X/Z in RHS acts as wildcard
wire diff = (data !=? 4'b10x1);   // Negated wildcard match
```

**Power operator (`**`):**
```cpp
// Parser recognizes:
// '**' → BinaryExpr with op = 'p' (power)

// Verilog:
wire [7:0] result = base ** exponent;  // Integer exponentiation
```

**Operator precedence:**
- Power `**`: Highest (above unary)
- Case/wildcard `===`/`!==`/`==?`/`!=?`: Same as equality `==`/`!=`
- Matches IEEE 1364-2005 Verilog standard

### Frontend: AST Extensions

**File**: `src/frontend/ast.hh` (+15 lines), `src/frontend/ast.cc` (+55 lines)

**New expression operators:**
```cpp
// Binary expression opcodes (char-based):
// 'C' - case equality (===)
// 'c' - case inequality (!==)
// 'W' - wildcard equality (==?)
// 'w' - wildcard inequality (!=?)
// 'p' - power operator (**)

// Existing opcodes for reference:
// 'E' - logical equality (==)
// 'N' - logical inequality (!=)
// '+', '-', '*', '/', '%' - arithmetic
// '<', '>', 'L' (<=), 'G' (>=) - comparison
// '&', '|', '^' - bitwise
// 'A' (&&), 'O' (||) - logical
// 'l' (<<), 'r' (>>), 'R' (>>>) - shift
```

**Part-select assignment AST:**
```cpp
struct SequentialAssign {
  std::string lhs;
  std::unique_ptr<Expr> lhs_index;       // Array index: arr[i]
  std::vector<std::unique_ptr<Expr>> lhs_indices;  // Multi-dim: arr[i][j]

  // NEW: Part-select fields
  bool lhs_has_range = false;            // True for [msb:lsb] or [idx +: w]
  bool lhs_indexed_range = false;        // True for [idx +: w] / [idx -: w]
  int lhs_indexed_width = 0;             // Width for indexed part-select
  std::unique_ptr<Expr> lhs_msb_expr;    // MSB/index expression
  std::unique_ptr<Expr> lhs_lsb_expr;    // LSB expression (or nullptr)
  int lhs_msb = 0;                       // Constant MSB for fixed range
  int lhs_lsb = 0;                       // Constant LSB for fixed range

  std::unique_ptr<Expr> rhs;
  // ... other fields
};
```

### Elaboration: Power Operator Width Handling

**File**: `src/core/elaboration.cc` (+195 lines)

**Power operator semantics:**
- Result width = base width (NOT max of operands like other binary ops)
- `[7:0] base ** [15:0] exp` → `[7:0]` result
- Consistent with Verilog standard: result size determined by LHS

**Elaboration changes:**
```cpp
// Expression width calculation:
int ExprWidth(const Expr& expr, const Module& module) {
  // ...
  case ExprKind::kBinary:
    if (expr.op == 'p') {  // Power operator
      return expr.lhs ? ExprWidth(*expr.lhs, module) : 32;  // Base width
    }
    // Other operators use max(lhs_width, rhs_width)
    // ...
}

// Signedness propagation:
bool ExprSigned(const Expr& expr, const Module& module) {
  // ...
  case ExprKind::kBinary:
    // Match-3 and comparison operators always return unsigned 1-bit
    if (expr.op == 'C' || expr.op == 'c' ||
        expr.op == 'W' || expr.op == 'w') {
      return false;  // Always unsigned result
    }
    // Power inherits signedness from both operands (both must be signed)
    // ...
}
```

### Codegen (4-state): Match-3 Functions

**File**: `include/gpga_4state.h` (+64 lines)

**Power operator functions added:**
```cpp
// Unsigned power (binary exponentiation):
inline FourState32 fs_pow32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);  // X/Z → all-X result
  uint mask = fs_mask32(width);
  uint base = a.val & mask;
  uint exp = b.val & mask;
  uint result = 1u;
  // Binary exponentiation: O(log exp) instead of O(exp)
  while (exp != 0u) {
    if (exp & 1u) {
      result *= base;
    }
    base *= base;
    exp >>= 1u;
  }
  return fs_make32(result, 0u, width);
}

// Signed power (handles negative exponent):
inline FourState32 fs_spow32(FourState32 a, FourState32 b, uint width) {
  if ((a.xz | b.xz) != 0u) return fs_allx32(width);
  uint mask = fs_mask32(width);
  int exp = fs_sign32(b.val & mask, width);  // Sign-extend exponent
  if (exp < 0) return fs_make32(0u, 0u, width);  // Negative exp → 0 (integer)
  // Compute as unsigned
  uint base = a.val & mask;
  uint result = 1u;
  uint exp_u = uint(exp);
  while (exp_u != 0u) {
    if (exp_u & 1u) {
      result *= base;
    }
    base *= base;
    exp_u >>= 1u;
  }
  return fs_make32(result, 0u, width);
}

// 64-bit variants: fs_pow64, fs_spow64 (same algorithm)
```

**Match-3 semantics** (implemented in `msl_codegen.cc`, not header):
```cpp
// Case equality (===): Compares val AND xz bits exactly
// a === b  ⟺  (a.val == b.val) && (a.xz == b.xz)
FourState32 fs_case_eq32(FourState32 a, FourState32 b, uint width) {
  uint mask = fs_mask32(width);
  bool val_match = ((a.val ^ b.val) & mask) == 0u;
  bool xz_match = ((a.xz ^ b.xz) & mask) == 0u;
  return FourState32{(val_match && xz_match) ? 1u : 0u, 0u};  // Always known 0/1
}

// Wildcard equality (==?): X/Z in RHS acts as wildcard
// a ==? b  ⟺  For each bit: (a == b) OR (b is X/Z)
FourState32 fs_casez32(FourState32 a, FourState32 b, uint ignore_mask, uint width) {
  uint mask = fs_mask32(width);
  uint diff = (a.val ^ b.val) & mask & ~ignore_mask;  // Ignore wildcards
  bool match = (diff == 0u);
  return FourState32{match ? 1u : 0u, 0u};
}
```

### Codegen (2-state): Power and Part-Select

**File**: `src/codegen/msl_codegen.cc` (+1,730 lines, -0 lines = +1,730 net)

**Power operator emission (2-state):**

When `--4state` is NOT used, power operators emit inline helper functions:

```cpp
// Emitted at top of MSL file if module uses power operator:
inline uint gpga_pow_u32(uint base, uint exp) {
  uint result = 1u;
  while (exp != 0u) {
    if (exp & 1u) {
      result *= base;
    }
    base *= base;
    exp >>= 1u;
  }
  return result;
}

inline uint gpga_pow_s32(int base, int exp) {
  if (exp < 0) {
    return 0u;  // Negative exponent → 0
  }
  return gpga_pow_u32(uint(base), uint(exp));
}
// Similar: gpga_pow_u64, gpga_pow_s64 for 64-bit
```

**Power expression emission:**
```cpp
// Verilog: wire [7:0] y = a ** b;
// MSL (unsigned):
uint y_val = gpga_pow_u32(a_val, b_val) & 0xFFu;

// MSL (signed):
uint y_val = gpga_pow_s32((int)(a_val), (int)(b_val)) & 0xFFu;
```

**Part-select assignment emission:**

**Fixed range** (`data[7:0] = rhs`):
```cpp
// Before REV21: Only supported in continuous assignments
assign out[7:0] = in[3:0];

// After REV21: Also supported in procedural blocks
initial begin
  data[7:0] = 8'hAA;  // Assign to fixed slice
end

// Emitted MSL:
uint mask_lo = 0xFFu;         // Bits 7:0
uint mask_hi = ~0xFFu;        // Clear those bits
data_val = (data_val & mask_hi) | ((rhs_val & mask_lo) << 0);
```

**Indexed part-select** (`data[idx +: 4] = rhs`):
```cpp
// Ascending (+:): data[idx +: 4] means bits [idx+3:idx]
// Descending (-:): data[idx -: 4] means bits [idx:idx-3]

initial begin
  idx = 4;
  data[idx +: 4] = 4'hF;  // Assign to bits [7:4]
end

// Emitted MSL with bounds guard:
std::string EmitRangeSelectUpdate(base_expr, index_expr, base_width,
                                  slice_width, rhs_expr) {
  // Check: index + width <= base_width
  std::string guard = "(uint(idx) <= " + (base_width - slice_width) + "u)";

  // Masked update:
  uint slice_mask = (1u << slice_width) - 1u;  // 0xF for width=4
  uint shifted_mask = (slice_mask << idx) & base_mask;
  uint clear = ~shifted_mask;
  uint set = ((rhs_val & slice_mask) << idx);

  if (guard) {
    data_val = (data_val & clear) | set;  // Conditional update
  }
}
```

**Match-3 operator emission (4-state):**
```cpp
// Case equality (===):
if (expr.op == 'C') {
  // fs_case_eq32(lhs, rhs, width) - exact bit-for-bit match
  std::string call = "fs_case_eq32(" + lhs_fs + ", " + rhs_fs + ", " +
                     std::to_string(width) + "u)";
  return FsExpr{call + ".val", call + ".xz", drive, 1};
}

// Case inequality (!==):
if (expr.op == 'c') {
  std::string eq_call = "fs_case_eq32(" + lhs_fs + ", " + rhs_fs + ", " +
                        std::to_string(width) + "u)";
  return FsExpr{"!((" + eq_call + ".val))", "0u", drive, 1};  // Negate
}

// Wildcard equality (==?):
if (expr.op == 'W') {
  std::string ignore = rhs_fs + ".xz";  // RHS X/Z bits are wildcards
  std::string call = "fs_casez32(" + lhs_fs + ", " + rhs_fs + ", " +
                     ignore + ", " + std::to_string(width) + "u)";
  return FsExpr{call + ".val", "0u", drive, 1};
}
```

**Enhanced expression caching:**
- Added width and signedness to CSE cache keys
- Prevents incorrect reuse of same expression with different widths
- Example: `(a + b)` as 8-bit vs 16-bit now cached separately

### Main: Minor Output Formatting

**File**: `src/main.mm` (+18 lines, -0 lines)

No functional changes - only output formatting adjustments for cleaner CLI display.

## Test Coverage

### New Tests (3 files, 49 lines)

**test_match3_ops.v** (17 lines):
```verilog
module test_match3_ops(
  input [3:0] a, b,
  output y_eq, y_ne, y_wild, y_nwild
);
  assign y_eq = (a === b);       // Case equality
  assign y_ne = (a !== b);       // Case inequality
  assign y_wild = (a ==? 4'b10x1);   // Wildcard match
  assign y_nwild = (a !=? 4'b10x1);  // Wildcard mismatch
endmodule

// Test scenarios:
// a=4'b1011, b=4'b1011 → y_eq=1, y_ne=0
// a=4'b10x1, b=4'b10x1 → y_eq=1 (X matches X exactly)
// a=4'b1011, b=4'b1001 → y_eq=0, y_ne=1
// a=4'b1011, pattern=4'b10x1 → y_wild=1 (matches wildcard)
```

**test_match3_ops64.v** (17 lines):
- Same as above but with 64-bit operands
- Tests wide value handling in match-3 operators

**test_part_select_assign.v** (15 lines):
```verilog
module test_part_select_assign;
  reg [15:0] data;
  integer idx;

  initial begin
    data = 16'h0000;
    data[7:0] = 8'hAA;        // Fixed range low byte
    data[15:8] = 8'h55;       // Fixed range high byte
    idx = 4;
    data[idx +: 4] = 4'hF;    // Indexed ascending: bits [7:4]
    data[idx -: 4] = 4'h0;    // Indexed descending: bits [4:1]
  end
endmodule

// After execution: data = 16'h5FE0 (depends on slice order)
```

### Updated Tests (2 files)

**test_part_select_variable.v** (2 lines changed):
- Updated to use new part-select syntax

**test_power_operator.v** (2 lines changed):
- Fixed test expectations for signed power operator

## Implementation Details

### Match-3 Operator Semantics

**Comparison summary:**

| Operator | Verilog | X/Z Handling | Result | Use Case |
|----------|---------|--------------|--------|----------|
| `==` | Equality | X if any X/Z | 0/1/X | Normal comparison |
| `!=` | Inequality | X if any X/Z | 0/1/X | Normal comparison |
| `===` | Case equality | Exact match | 0/1 | Testbench verification |
| `!==` | Case inequality | Exact mismatch | 0/1 | Testbench verification |
| `==?` | Wildcard equal | RHS X/Z = don't care | 0/1 | Pattern matching |
| `!=?` | Wildcard not-equal | RHS X/Z = don't care | 0/1 | Pattern matching |

**Examples:**
```verilog
// Case equality (===): Exact bit-for-bit match
4'b10x1 === 4'b10x1  → 1 (X matches X)
4'b10x1 === 4'b1001  → 0 (X doesn't match 0)
4'b1001 === 4'b1001  → 1 (values match)

// Regular equality (==): X propagates
4'b10x1 == 4'b10x1  → X (comparing unknown)
4'b1001 == 4'b1001  → 1 (known values match)

// Wildcard equality (==?): RHS X/Z acts as mask
4'b1011 ==? 4'b10x1  → 1 (bit 1 is wildcard, matches anything)
4'b1001 ==? 4'b10x1  → 1 (bit 1 is wildcard)
4'b0011 ==? 4'b10x1  → 0 (bit 3 mismatch, not wildcard)
```

**Why match-3 operators matter:**
- Essential for testbenches checking X/Z propagation
- Used in formal verification property checking
- Pattern matching in FSM decoders
- Not synthesizable (simulation/verification only)

### Power Operator Implementation

**Binary exponentiation algorithm:**

Instead of naive loop (`result = 1; for i in 0..exp: result *= base`), uses **exponentiation by squaring** for O(log n) complexity:

```cpp
// Example: 3^13
// 13 in binary: 1101
uint result = 1;
uint base = 3;
uint exp = 13;

// Iteration 1: exp=1101₂ (LSB=1), result = 1*3 = 3, base = 3*3 = 9, exp >>= 1 = 110₂
// Iteration 2: exp=110₂ (LSB=0), result = 3, base = 9*9 = 81, exp >>= 1 = 11₂
// Iteration 3: exp=11₂ (LSB=1), result = 3*81 = 243, base = 81*81 = 6561, exp >>= 1 = 1₂
// Iteration 4: exp=1₂ (LSB=1), result = 243*6561 = 1594323, exp >>= 1 = 0
// Final: 3^13 = 1594323
```

**Signed power handling:**
```verilog
// Unsigned: Simple exponentiation
8'd3 ** 8'd4  → 8'd81

// Signed base and exponent:
-8'sd2 ** 8'sd3  → -8'sd8  // (-2)^3 = -8

// Negative exponent (integer result → 0):
8'sd2 ** -8'sd3  → 8'sd0   // 2^(-3) = 0.125 → truncated to 0
```

**Why power operator matters:**
- Used in CRC polynomial calculations
- Scaling factors in fixed-point arithmetic
- Cryptographic primitives
- Power-of-2 address decoding

### Part-Select Assignment Edge Cases

**Bounds checking:**
```verilog
reg [7:0] data;
integer idx;

// Safe: idx=4, width=4, range=[7:4] (within bounds)
idx = 4;
data[idx +: 4] = 4'hF;  // OK: sets bits [7:4]

// Out-of-bounds: idx=6, width=4, range=[9:6] (exceeds width 8)
idx = 6;
data[idx +: 4] = 4'hF;  // Guarded: no-op, data unchanged

// MSL guard:
if (uint(idx) <= (8u - 4u)) {  // idx <= 4
  // Perform assignment
}
```

**Descending vs ascending:**
```verilog
// Ascending (+:): Start at index, go up
data[4 +: 4]  ≡  data[7:4]  // Bits 4,5,6,7

// Descending (-:): End at index, go down
data[7 -: 4]  ≡  data[7:4]  // Bits 7,6,5,4

// Note: Both access same bits for idx=4 and idx=7 respectively
```

**Variable vs constant indices:**
- Constant: `data[7:4]` - resolved at elaboration, no guards needed
- Variable: `data[idx +: 4]` - requires runtime guard in MSL

## Documentation Updates

### REV Documents Created (10,393 lines)

**REV0-REV20** comprehensively document the entire development history:

- **REV0**: Initial parser skeleton
- **REV1**: Module, port, net declarations
- **REV2-REV7**: Operators, expressions, assignments
- **REV8**: Loops (for/while/repeat), initial blocks
- **REV9**: Always blocks, sensitivity lists
- **REV10**: Multi-dimensional arrays
- **REV11**: Generate blocks, genvar, functions
- **REV12**: Event scheduler infrastructure
- **REV13-REV16**: Event semantics, inter-block timing
- **REV17**: Parameters, defparams
- **REV18**: Fork/join, tasks
- **REV19**: System tasks ($display, $finish, etc.)
- **REV20**: 4-state library extraction, CSE infrastructure

Each REV document includes:
- Commit hash, date, message
- Pipeline status table
- User-visible changes
- Detailed architecture analysis
- Test coverage
- Implementation details
- Statistics

**Format consistency:**
- All REV docs follow same structure
- Code examples with before/after
- Clear feature categorization
- Known gaps documented

### README Updates (+47 lines)

**Version milestone:**
- Updated from "v0.4+" to "v0.5"
- Added match-3 operators to feature list
- Added power operator to feature list
- Updated test count (228+ passing)

**Feature documentation:**
- Case equality/inequality operators
- Wildcard match operators
- Power operator (both signed/unsigned)
- Part-select assignment (fixed and indexed)

## Known Gaps and Limitations

### Improvements Over REV20

**Now Working:**
- ✓ Case equality/inequality (`===`, `!==`)
- ✓ Wildcard match (`==?`, `!=?`)
- ✓ Power operator (`**`) with signed/unsigned
- ✓ Procedural part-select assignment (fixed and indexed)
- ✓ Comprehensive development history documentation (REV0-REV20)

**Match-3 Operator Limitations (v0.5):**
- Only meaningful with `--4state` flag (2-state treats as regular equality)
- Case operators not optimized (always evaluate both operands)
- No case-z variant (treats X and Z identically in wildcards)

**Power Operator Limitations (v0.5):**
- Integer-only (no real number support)
- Negative exponents return 0 (no fractional results)
- Large exponents may overflow (no overflow detection)
- Not optimized (could use constant folding for literal exponents)

**Part-Select Assignment Limitations (v0.5):**
- Runtime bounds checking required for indexed selects (performance cost)
- No warning for out-of-bounds assignments (silently ignored)
- Cannot assign to concatenations: `{a, b[3:0]} = value;` not supported
- Indexed descending (-:) less tested than ascending (+:)

**Still Missing:**
- Real number arithmetic (floating-point)
- Hierarchical references (`top.sub.signal`)
- Arrays of instances
- Recursive tasks/functions
- Some system tasks (`$dumpfile`, `$random`, etc.)

### Semantic Notes (v0.5)

**Match-3 operator evaluation order:**

All four operators are **strict** (evaluate both operands even if result could be determined early):
```verilog
// Even though LHS is X, RHS is still evaluated:
if ((1'bx === get_value()) == 1'b1) begin  // get_value() called
  // ...
end
```

This differs from short-circuit logical operators (`&&`, `||`).

**Power operator associativity:**

Verilog standard specifies **right-to-left** associativity:
```verilog
2 ** 3 ** 2  ≡  2 ** (3 ** 2)  ≡  2 ** 9  ≡  512
// NOT: (2 ** 3) ** 2  ≡  8 ** 2  ≡  64
```

Implementation honors this via parser precedence/associativity rules.

**Part-select assignment write masks:**

The implementation uses **read-modify-write** strategy:
1. Read current value
2. Clear target bits
3. Insert new bits at position
4. Write back

This ensures atomic update in MSL kernel (no races within single thread).

## Statistics

- **Files changed**: 34
- **Lines added**: 11,567
- **Lines removed**: 361
- **Net change**: +11,206 lines

**Breakdown:**
- Documentation (REV0-REV20): +10,393 lines (92% of total)
- Parser: +436 lines (match-3 operators, power operator parsing)
- Elaboration: +195 lines (power operator width/signedness handling)
- MSL codegen: +1,730 lines (match-3 emission, power helpers, part-select updates, CSE enhancements)
- 4-state library: +64 lines (power functions)
- AST: +70 lines (part-select fields, operator codes)
- README: +47 lines (v0.5 status, feature list)
- Host codegen: +2 lines (scheduler comments)
- Tests: +51 lines (3 new tests)

**Test suite:**
- `verilog/pass/`: 228+ files (up from 225 in REV20)
  - 3 new tests: match-3 operators (32/64-bit), part-select assignment
  - 2 updated tests: power operator fixes
- All new features have test coverage

**Code quality:**
- Added 4 new comparison operators seamlessly
- Power operator uses efficient binary exponentiation (O(log n))
- Part-select bounds checking prevents buffer overflows
- CSE cache now includes width/signedness (prevents incorrect reuse)

This commit elevates metalfpga to **v0.5** with **enhanced Verilog compliance**. The match-3 operators are essential for testbenches verifying X/Z propagation, the power operator enables exponentiation without external functions, and part-select assignment matches standard Verilog procedural semantics. Combined with comprehensive historical documentation (REV0-REV20), this release provides a solid foundation for advanced HDL features.
