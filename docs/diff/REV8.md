# REV8 - Loop constructs implementation (for/while/repeat)

**Commit:** d6a8f1b
**Date:** Thu Dec 25 18:07:02 2025 +0100
**Message:** Added for/while/repeat and tests

## Overview

Major feature addition implementing all three Verilog loop constructs: `for`, `while`, and `repeat`. Loops are unrolled at elaboration time, converting them into block statements. Supports nested loops, negative stepping, and complex conditions. Includes 17 new comprehensive test cases and moves the original for-loop test to `pass/`.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added `for`, `while`, `repeat`, `initial` block parsing |
| **Elaborate** | ✓ Enhanced | Loop unrolling with constant evaluation |
| **Codegen (2-state)** | ✓ MSL emission | No changes (loops already unrolled) |
| **Codegen (4-state)** | ✓ Complete | No changes (loops already unrolled) |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Verilog Features:**
- **For loops**: `for (i = 0; i < 8; i = i + 1) ...`
- **While loops**: `while (condition) ...`
- **Repeat loops**: `repeat (N) ...`
- **Initial blocks**: `initial begin ... end` (for loop execution context)
- **Integer type**: `integer i;` (32-bit signed variable for loop counters)

**Loop Capabilities:**
- Nested loops (for within for, while within for, etc.)
- Negative stepping: `i = i - 1`, `i = i - 2`
- Complex conditions: `while ((i < 10) && (flag == 1))`
- Memory initialization patterns
- Use in `initial` blocks and `always` blocks

**Limitations:**
- Loops must be **compile-time unrollable** (constant bounds/conditions)
- Maximum 100,000 iterations per loop (safety limit)
- Loop variables must be parameters or integer types
- Cannot use runtime signals in loop conditions

## Architecture Changes

### Frontend: AST Extensions for Loops

**File**: `src/frontend/ast.hh` (+14 lines)

**New statement kinds:**
```cpp
enum class StatementKind {
  kAssign,
  kIf,
  kBlock,
  kCase,
  kFor,      // NEW
  kWhile,    // NEW
  kRepeat,   // NEW
};
```

**For loop fields:**
```cpp
struct Statement {
  // ... existing fields ...

  // For loop: for (init_lhs = init_rhs; condition; step_lhs = step_rhs)
  std::string for_init_lhs;           // Loop variable name (e.g., "i")
  std::unique_ptr<Expr> for_init_rhs; // Initial value (e.g., 0)
  std::unique_ptr<Expr> for_condition; // Continue condition (e.g., i < 8)
  std::string for_step_lhs;           // Step variable (must match init_lhs)
  std::unique_ptr<Expr> for_step_rhs; // Step expression (e.g., i + 1)
  std::vector<Statement> for_body;    // Loop body statements
```

**While loop fields:**
```cpp
  // While loop: while (condition) body
  std::unique_ptr<Expr> while_condition; // Continue condition
  std::vector<Statement> while_body;     // Loop body
```

**Repeat loop fields:**
```cpp
  // Repeat loop: repeat (count) body
  std::unique_ptr<Expr> repeat_count;    // Number of iterations
  std::vector<Statement> repeat_body;    // Loop body
};
```

**New edge kind:**
```cpp
enum class EdgeKind {
  kPosedge,
  kNegedge,
  kCombinational,
  kInitial,  // NEW: For initial blocks
};
```

### Frontend: Parser Implementation

**File**: `src/frontend/verilog_parser.cc` (+164 lines)

**For loop parsing:**
```cpp
// Verilog: for (i = 0; i < 8; i = i + 1) arr[i] = i;

// Parsing steps:
// 1. Match "for" keyword
// 2. Parse init: "i = 0"
// 3. Parse condition: "i < 8"
// 4. Parse step: "i = i + 1"
// 5. Parse body (single statement or begin/end block)
```

**While loop parsing:**
```cpp
// Verilog: while (i < 8) begin ... end

// Parsing steps:
// 1. Match "while" keyword
// 2. Parse condition expression
// 3. Parse body (single statement or begin/end block)
```

**Repeat loop parsing:**
```cpp
// Verilog: repeat (8) begin ... end

// Parsing steps:
// 1. Match "repeat" keyword
// 2. Parse count expression (must be constant)
// 3. Parse body (single statement or begin/end block)
```

**Initial block parsing:**
```cpp
// Verilog: initial begin ... end

// Parsing:
// - Creates AlwaysBlock with edge kind kInitial
// - Contains statements to execute once at simulation start
// - Used for loop-based initialization
```

**Integer type parsing:**
```cpp
// Verilog: integer i, j, k;

// Treated as: reg signed [31:0] i, j, k;
// 32-bit signed registers for loop counters and general use
```

### Elaboration: Loop Unrolling

**File**: `src/core/elaboration.cc` (+502 lines!)

This is the core implementation - loops are **unrolled at elaboration time**.

**For loop unrolling algorithm:**

```cpp
// Input: for (i = 0; i < 8; i = i + 1) arr[i] = i;

// Unrolling process:
// 1. Evaluate init expression: i = 0
// 2. Create parameter binding: {i → 0}
// 3. Evaluate condition with params: (0 < 8) → true
// 4. Clone loop body with i=0: arr[0] = 0
// 5. Evaluate step expression: i = i + 1 → 1
// 6. Repeat from step 3 with i=1
// ... until condition false

// Output (flattened block):
// {
//   arr[0] = 0;
//   arr[1] = 1;
//   arr[2] = 2;
//   ...
//   arr[7] = 7;
// }
```

**Implementation details:**

`UnrollForLoop()`:
- Evaluates init, condition, and step as constant expressions
- Uses parameter substitution for loop variable
- Clones body for each iteration
- Validates step updates loop variable
- Enforces 100,000 iteration limit

**While loop unrolling:**

```cpp
// Input: while (i < 8) begin arr[i] = i; i = i + 1; end

// Unrolling:
// 1. Assume i is a parameter with current value
// 2. Evaluate condition: (i < 8)
// 3. If true: clone body, execute to get new i value
// 4. Repeat until condition false or iteration limit

// Challenge: Must track variable updates within loop body
// Solution: Statement execution simulation during elaboration
```

`UnrollWhileLoop()`:
- More complex than for loops (no explicit step)
- Simulates loop body execution to track variable changes
- Requires constant-evaluable condition
- Same 100,000 iteration limit

**Repeat loop unrolling:**

```cpp
// Input: repeat (8) arr[i] = i;

// Simplest case:
// 1. Evaluate count: 8
// 2. Clone body 8 times
// No condition checking needed

// Output:
// {
//   arr[i] = i;  // iteration 1
//   arr[i] = i;  // iteration 2
//   ...
//   arr[i] = i;  // iteration 8
// }
```

`UnrollRepeatLoop()`:
- Simplest implementation
- Just duplicates body N times
- Count must be constant expression

**Nested loop handling:**

```cpp
// Verilog:
// for (i = 0; i < 4; i = i + 1)
//   for (j = 0; j < 4; j = j + 1)
//     mem[i][j] = i * 4 + j;

// Unrolling:
// Outer loop unrolls → 4 iterations
// Each iteration contains inner for loop
// Inner loop unrolls → 4 iterations each
// Total: 16 unrolled assignments

// Parameter context maintained:
// Outer iteration 0: {i → 0}
//   Inner iteration 0: {i → 0, j → 0}
//   Inner iteration 1: {i → 0, j → 1}
//   ...
// Outer iteration 1: {i → 1}
//   ...
```

**Safety mechanisms:**

```cpp
// Maximum iteration limit:
const int kMaxIterations = 100000;

// Prevents infinite loops from hanging compiler:
// - Typo in condition: while (i < 80) but incrementing by 1 from 0
// - Logic error: while (flag) but flag never changes
// - Accidental infinite: while (1) with no break

// Error message:
// "for-loop exceeds iteration limit"
```

### AST: Loop Cloning and Evaluation

**File**: `src/frontend/ast.cc` (+81 lines)

**New functions:**

`CloneStatement()`:
- Deep copies Statement structures
- Handles all statement kinds including new loops
- Used during loop body duplication

`EvalConstExprWithParams()`:
- Evaluates expressions with parameter substitution
- Supports loop variable references
- Returns constant int64_t values
- Used for:
  - Loop init: `i = 0` → 0
  - Loop condition: `i < 8` → true/false
  - Loop step: `i + 1` → next value

`MakeNumberExprSignedWidth()`:
- Creates expression node from constant value
- Used to inject loop variable values
- Example: Create `Expr` for `i=5` to use in body cloning

### Codegen and CLI

**File**: `src/codegen/msl_codegen.cc` (+18 lines)
- Minor adjustments for handling unrolled blocks
- No special loop emission (loops already gone after elaboration)

**File**: `src/main.mm` (+2 lines)
- Pretty-printing support for loop structures (for `--dump-flat` before elaboration)

## Implementation Details

### Loop Unrolling Strategy

**Why unroll at elaboration:**
- Verilog simulation semantics: loops execute sequentially
- GPU parallelism: better suited to data-parallel operations
- Simplifies codegen: only needs to handle assignments and conditionals
- Allows aggressive optimization on unrolled code

**Compile-time requirements:**
- All loop bounds must be constant-evaluable
- Loop variables must be parameters or integers
- Conditions cannot depend on runtime signals (wires/regs with non-constant values)

**Example transformations:**

**For loop - array initialization:**
```verilog
// Input:
integer i;
reg [7:0] arr [0:7];
for (i = 0; i < 8; i = i + 1)
  arr[i] = i;

// After unrolling:
arr[0] = 0;
arr[1] = 1;
arr[2] = 2;
arr[3] = 3;
arr[4] = 4;
arr[5] = 5;
arr[6] = 6;
arr[7] = 7;
```

**While loop - countdown:**
```verilog
// Input:
integer i;
i = 8;
while (i > 0) begin
  arr[i-1] = 8 - i;
  i = i - 1;
end

// After unrolling:
i = 8;
arr[7] = 0;  // i=8
i = 7;
arr[6] = 1;  // i=7
i = 6;
arr[5] = 2;  // i=6
...
```

**Repeat loop - fill pattern:**
```verilog
// Input:
integer i = 0;
repeat (4) begin
  arr[i] = 8'hFF;
  i = i + 1;
end

// After unrolling:
i = 0;
arr[i] = 8'hFF;
i = i + 1;
arr[i] = 8'hFF;
i = i + 1;
arr[i] = 8'hFF;
i = i + 1;
arr[i] = 8'hFF;
i = i + 1;
```

### Integer Type Implementation

**Declaration:**
```verilog
integer i, j, count;
```

**Internal representation:**
- Treated as `reg signed [31:0]`
- 32-bit signed register
- Can hold values from -2,147,483,648 to 2,147,483,647

**Usage:**
- Loop counters (primary use case)
- Temporary variables in initial blocks
- General-purpose signed arithmetic

## Test Coverage

### For Loop Tests (6 new + 1 moved = 7 files)

**test_for_loop.v** (moved from root):
```verilog
initial for (i = 0; i < 8; i = i + 1)
  arr[i] = i;
```

**test_for_loop_always.v**:
- For loop in always @(posedge clk) block
- Updates memory array each clock cycle

**test_for_negative.v**:
- Reverse iteration: `for (i = 7; i >= 0; i = i - 1)`

**test_for_negative_step2.v**:
- Negative step by 2: `i = i - 2`

**test_for_reverse_fill.v**:
- Fill array in reverse order

**test_for_step2.v**:
- Forward iteration stepping by 2: `i = i + 2`

**test_nested_for_loop.v**:
```verilog
for (i = 0; i < 4; i = i + 1)
  for (j = 0; j < 4; j = j + 1)
    mem[i*4 + j] = i * 4 + j;
```

### While Loop Tests (8 files)

**test_while_simple.v**:
```verilog
i = 0;
while (i < 8) begin
  arr[i] = i;
  i = i + 1;
end
```

**test_while_always.v**:
- While loop in always block

**test_while_complex.v**:
- Complex condition: `while ((i < 8) && (flag == 1))`

**test_while_countdown.v**:
- Countdown: `while (i > 0)`

**test_while_dual_countdown.v**:
- Two variables: `while ((i > 0) && (j > 0))`

**test_while_negative.v**:
- Negative decrement values

**test_while_negative_step2.v**:
- Decrement by 2

**test_while_step2.v**:
- Increment by 2

### Repeat Loop Tests (3 files)

**test_repeat_simple.v**:
```verilog
i = 0;
repeat (8) begin
  arr[i] = i;
  i = i + 1;
end
```

**test_repeat_always.v**:
- Repeat in always block

**test_repeat_nested.v**:
```verilog
repeat (4)
  repeat (4)
    count = count + 1;
// count = 16 after execution
```

## Documentation Updates

**File**: `docs/gpga/verilog_words.md` (+6 lines)

Added keywords:
- `for` - For loop construct
- `while` - While loop construct
- `repeat` - Repeat loop construct
- `initial` - Initial block (execution once at start)
- `integer` - 32-bit signed integer type

## Known Gaps and Limitations

### Improvements Over REV7

**Now Working:**
- For loops (with complex init/condition/step)
- While loops
- Repeat loops
- Nested loops (any combination)
- Initial blocks
- Integer type

**Limitations (v0.2+):**
- Loops must be **compile-time constant**
- Cannot use runtime signals in loop conditions
- 100,000 iteration limit (safety check)
- No `break` or `continue` statements (not in Verilog-2001)
- No `disable` statement for loop exit

**Still Missing:**
- Functions/tasks - not implemented
- `generate` blocks - not implemented
- System tasks (`$display`, etc.) - not implemented
- Multi-dimensional arrays - partially supported
- Host code emission - still stubbed
- Runtime - no execution

### Semantic Notes

**Loop unrolling implications:**
- Large loops create large flattened code
- 10,000 iteration loop → 10,000 unrolled statements
- May impact elaboration time and memory
- Codegen sees fully expanded code

**Integer type:**
- Always 32-bit signed (`int32_t` equivalent)
- Cannot declare different widths
- Follows Verilog-2001 spec

## Statistics

- **Files changed**: 25
- **Lines added**: 940
- **Lines removed**: 31
- **Net change**: +909 lines

**Major components:**
- Elaboration: +502 lines (loop unrolling, variable tracking, iteration limits)
- Parser: +164 lines (for/while/repeat/initial/integer parsing)
- AST implementation: +81 lines (cloning, constant evaluation with params)
- AST header: +14 lines (loop statement fields, kInitial edge)
- Codegen: +18 lines (minor adjustments)
- Documentation: +6 lines (keyword reference)

**Test coverage:**
- For loops: 7 tests (basic, negative, step variants, nested, always block)
- While loops: 8 tests (simple, countdown, complex conditions, step variants)
- Repeat loops: 3 tests (simple, nested, always block)
- **Total**: 18 loop tests (17 new + 1 moved)

**Test suite totals:**
- `verilog/pass/`: 96 files (up from 79 in REV7)
- `verilog/`: 3 files (down from 4 - for_loop moved to pass)

This commit transforms metalfpga into a compiler capable of handling realistic Verilog testbenches and initialization code, not just combinational/sequential logic.
