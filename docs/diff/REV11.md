# REV11 - Generate blocks and genvar implementation (v0.3)

**Commit:** dad07ab
**Date:** Thu Dec 25 22:12:35 2025 +0100
**Message:** v0.3 Generate and GenVar is now added, more tests added

## Overview

Major v0.3 milestone implementing Verilog `generate` blocks with `genvar` support. Generate blocks enable compile-time elaboration of hardware structures using loops and conditionals. This allows parameterized designs to create variable numbers of instances, wires, or logic blocks. The implementation adds sophisticated parser support for generate syntax, elaboration-time unrolling similar to loops, and comprehensive testing with 7 new tests. Also includes licensing files (AGPL-3.0 with commercial option) and function implementation (inlined at elaboration).

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Added `generate`/`genvar`/`function` parsing (+1,551 lines) |
| **Elaborate** | ✓ Enhanced | Generate block elaboration with parameter substitution (+446 lines) |
| **Codegen (2-state)** | ✓ MSL emission | Enhanced for generated constructs (+191 lines) |
| **Codegen (4-state)** | ✓ Complete | Works with generated hardware |
| **Host emission** | ✗ Stubbed only | Minor updates |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**New Verilog Features:**
- **Generate blocks**: `generate ... endgenerate` for compile-time hardware generation
- **Genvar declarations**: `genvar i, j;` for generate loop variables
- **Generate for loops**: `for (i = 0; i < N; i = i + 1) begin : label ... end`
- **Generate if/else**: `if (condition) begin : label ... end else begin ... end`
- **Generate labels**: Named scopes for generated blocks (required in many contexts)
- **Functions**: `function [width-1:0] name; ... endfunction` (inlined at elaboration)
- **Localparam in generate**: Local parameters within generate blocks

**Generate Capabilities:**
- Create arrays of module instances
- Conditionally include/exclude hardware based on parameters
- Generate wires, regs, assigns, always blocks, and instances
- Nested generate blocks (loops within loops, if within loop, etc.)
- Multiple generate blocks in same module
- Complex parameter expressions in generate conditions

**CLI/Documentation:**
- README updated to reflect v0.3 status
- Test count: 114 passing tests (up from 102)
- Added LICENSE (AGPL-3.0) and commercial licensing option

## Architecture Changes

### Frontend: AST Extensions for Generate

**File**: `src/frontend/ast.hh` (+3 lines)

The AST structure itself didn't require new top-level types because generate blocks are elaborated away before code generation. However, parser-internal structures were added to represent generate constructs during parsing and elaboration.

**No new public AST fields** - generate blocks are fully expanded into regular `Module` elements (nets, assigns, instances, always blocks) during elaboration phase.

**Key insight**: Generate blocks are a **meta-language feature** - they describe how to create hardware at compile time, not runtime hardware structures. After elaboration, generated code becomes indistinguishable from hand-written code.

### Frontend: Parser Implementation

**File**: `src/frontend/verilog_parser.cc` (+1,551 lines, -130 lines = +1,421 net)

This is the largest change - comprehensive generate syntax support.

**Generate block parsing:**

```cpp
// Verilog:
// generate
//   genvar i;
//   for (i = 0; i < 4; i = i + 1) begin : inst
//     wire [7:0] data;
//   end
// endgenerate

// Parser recognizes:
// 1. "generate" keyword (optional but supported)
// 2. genvar declarations
// 3. for loops with genvar
// 4. begin/end blocks with labels
// 5. "endgenerate" keyword
```

**Genvar declaration:**
```verilog
genvar i;           // Single genvar
genvar i, j, k;     // Multiple genvars in one declaration
```

**Generate for loop:**
```verilog
for (i = 0; i < WIDTH; i = i + 1) begin : bit_proc
  assign out[i] = ~in[i];
end

// Components:
// - Init: i = 0 (genvar assignment)
// - Condition: i < WIDTH (constant expression with parameters)
// - Step: i = i + 1 (increment genvar)
// - Label: "bit_proc" (required for proper scoping)
// - Body: Any valid module items (nets, assigns, instances, always, etc.)
```

**Generate if/else:**
```verilog
generate
  if (USE_FEATURE) begin : enabled
    // Hardware when USE_FEATURE = 1
    assign out = in << 1;
  end else begin : disabled
    // Hardware when USE_FEATURE = 0
    assign out = in;
  end
endgenerate
```

**Generate labels:**
- Required for loops and conditionals in Verilog-2001
- Creates hierarchical scope for generated identifiers
- Format: `begin : label_name ... end`

**Internal parser structures** (not in public AST):

```cpp
struct GenerateItem {
  // Can be: net declaration, assign, instance, always block,
  // localparam, nested generate block
};

struct GenerateBlock {
  std::string label;
  std::vector<GenerateItem> items;
};

struct GenerateForLoop {
  std::string genvar_name;
  int init_value;
  Expr* condition;
  Expr* step;
  GenerateBlock body;
};

struct GenerateIf {
  Expr* condition;
  GenerateBlock then_block;
  GenerateBlock else_block;
};
```

**Parser functions added:**
- `ParseGenerateBlock()` - Entry point for generate blocks
- `ParseGenerateBlockBody()` - Parse generate contents
- `ParseGenerateForLoop()` - Parse generate for with genvar
- `ParseGenerateIf()` - Parse generate if/else
- `ParseGenerateNetDecl()` - Parse wire/reg in generate
- `ParseGenerateAssign()` - Parse assign in generate
- `ParseGenerateInstance()` - Parse module instance in generate
- `ParseGenerateLocalparam()` - Parse localparam in generate

**Function parsing:**

```verilog
function [7:0] double;
  input [7:0] x;
  double = x << 1;
endfunction

// Parser extracts:
// - Return width: 8 bits
// - Function name: "double"
// - Input arguments: x [7:0]
// - Body: assignment to function name
```

Functions are parsed into `Function` AST structure and inlined during elaboration (similar to C macros).

### Elaboration: Generate Block Expansion

**File**: `src/core/elaboration.cc` (+446 lines)

Generate blocks are **elaborated at compile time** - they create regular module elements.

**Generate for loop elaboration:**

```verilog
// Input:
generate
  genvar i;
  for (i = 0; i < 4; i = i + 1) begin : bit_inv
    assign out[i] = ~in[i];
  end
endgenerate

// Elaboration process:
// 1. Evaluate init: i = 0
// 2. Check condition with i=0: (0 < 4) → true
// 3. Instantiate block body with i=0:
//    - Replace all references to i with constant 0
//    - Create: assign out[0] = ~in[0];
// 4. Increment: i = 1
// 5. Repeat until condition false

// Output (added to module):
assign out[0] = ~in[0];
assign out[1] = ~in[1];
assign out[2] = ~in[2];
assign out[3] = ~in[3];
// (4 separate continuous assignments)
```

**Generate if elaboration:**

```verilog
// Input:
generate
  if (USE_FAST) begin : fast_path
    assign out = in;
  end else begin : slow_path
    assign out = ~in;
  end
endgenerate

// With USE_FAST parameter = 1:
// 1. Evaluate condition: USE_FAST → 1 → true
// 2. Select then_block (fast_path)
// 3. Add contents: assign out = in;
// 4. Discard else_block entirely

// Output (USE_FAST=1):
assign out = in;

// Output (USE_FAST=0):
assign out = ~in;
```

**Nested generate blocks:**

```verilog
// Input: 2D array of inverters
generate
  genvar i, j;
  for (i = 0; i < ROWS; i = i + 1) begin : row
    for (j = 0; j < COLS; j = j + 1) begin : col
      assign out[i*COLS + j] = ~in[i*COLS + j];
    end
  end
endgenerate

// Elaboration:
// Outer loop (i): Creates ROWS iterations
// Inner loop (j): Creates COLS iterations per outer
// Total: ROWS * COLS assignments generated

// Example with ROWS=2, COLS=2:
assign out[0] = ~in[0];  // i=0, j=0
assign out[1] = ~in[1];  // i=0, j=1
assign out[2] = ~in[2];  // i=1, j=0
assign out[3] = ~in[3];  // i=1, j=1
```

**Generate with module instances:**

```verilog
// Input:
generate
  genvar i;
  for (i = 0; i < 8; i = i + 1) begin : inv_array
    inverter u_inv (
      .a(in[i]),
      .b(out[i])
    );
  end
endgenerate

// Output: 8 separate module instances created
// Instance names: inv_array[0].u_inv, inv_array[1].u_inv, ..., inv_array[7].u_inv
// Each with unique port connections
```

**Genvar scope and substitution:**
- Genvars are only valid within their generate block
- During elaboration, genvar references are replaced with constant values
- Allows use in: array indices, expressions, port connections, parameter values

**Function elaboration (inlining):**

```verilog
// Function definition:
function [7:0] double;
  input [7:0] x;
  double = x << 1;
endfunction

// Function call:
assign y = double(a);

// Elaboration (inlining):
// 1. Find function "double" in module
// 2. Create temporary for input: x = a
// 3. Inline body: temp = a << 1
// 4. Replace call with result: y = temp

// After elaboration:
assign y = a << 1;  // Function call completely removed
```

**Elaboration algorithm:**
1. Parse all generate blocks in module
2. Evaluate all parameter expressions (needed for conditions/bounds)
3. Process generate blocks:
   - For generate loops: unroll with genvar substitution
   - For generate ifs: evaluate condition, select branch
4. Add generated elements to parent module
5. Continue with normal elaboration (module flattening, etc.)

### Codegen: Enhanced MSL Emission

**File**: `src/codegen/msl_codegen.cc` (+191 lines)

**No special generate codegen needed** - by the time codegen runs, generate blocks are fully expanded into regular module elements.

The +191 lines are:
- Better handling of complex expressions from generated code
- Support for generated instance arrays
- Improved parameter substitution in port connections
- Edge case handling for deeply nested generated structures

Generated code is indistinguishable from hand-written Verilog after elaboration, so existing codegen handles it automatically.

## Implementation Details

### Generate vs. Regular Loops

**Regular loops (for/while/repeat):**
- Execute at **simulation time** (in `initial` or `always` blocks)
- Body executes sequentially
- Used for: initialization, testbench control, procedural logic

**Generate loops:**
- Execute at **elaboration time** (compile time)
- Body creates **hardware structures**
- Used for: parameterizable hardware, instance arrays, conditional compilation

**Example contrasts:**

```verilog
// Regular for loop (REV8 feature):
initial for (i = 0; i < 8; i = i + 1)
  arr[i] = i;  // Sequential assignments at sim time

// Generate for loop (REV11 feature):
generate
  for (i = 0; i < 8; i = i + 1) begin : bits
    assign out[i] = ~in[i];  // Creates 8 parallel assigns
  end
endgenerate
```

### Generate Elaboration vs. Loop Unrolling

Both use similar algorithms (iterate, substitute, clone), but:

**Loop unrolling (REV8):**
- Creates sequential `Statement` nodes
- Used in `always` or `initial` blocks
- Produces: sequence of assignments

**Generate elaboration (REV11):**
- Creates module-level elements (`Net`, `Assign`, `Instance`, `AlwaysBlock`)
- Used at module scope
- Produces: hardware structures

### Genvar Scoping Rules

**Verilog-2001 requirements:**
- Genvar must be declared before use: `genvar i;`
- Genvar scope is the generate block
- Multiple generate blocks can reuse same genvar name (different scopes)
- Genvar values are compile-time constants only

**Implementation:**
```cpp
// Parser tracks genvar names in current scope
std::unordered_set<std::string> genvars;

// During elaboration:
std::unordered_map<std::string, int64_t> genvar_values;
// Maps genvar name → current iteration value

// Substitute genvar references:
// Expression: out[i] with i=3 → out[3]
```

### Function Inlining

**Functions in Verilog:**
- Pure functions: all inputs declared, single return value
- Body is a single assignment to function name
- Can call other functions (recursive inlining)
- No timing, no procedural blocks, no side effects

**Inlining strategy:**
```cpp
// Before inlining:
assign y = func(a, b);

// Inline steps:
// 1. Create parameter bindings: {arg1 → a, arg2 → b}
// 2. Clone function body with substitution
// 3. Replace function call with inlined expression

// After inlining:
assign y = <inlined_expression>;
```

This is similar to C++ `inline` functions or C preprocessor macros, but with type safety.

## Test Coverage

### Generate Tests (7 new files, 803 lines)

**test_generate.v** (moved from root, 8 lines):
```verilog
// Simple generate loop creating wires
generate
  genvar i;
  for (i = 0; i < 4; i = i + 1) begin
    wire [7:0] data;
  end
endgenerate
```

**test_generate_loops.v** (54 lines):
- Basic generate for loops
- Multiple genvars
- Different step sizes
- Assign statements in generate

**test_generate_conditional.v** (90 lines):
- Generate if with single branch
- Generate if/else
- Multiple if/else chains
- Complex condition expressions
- Nested generate if

**test_generate_nested.v** (98 lines):
- 2D nested loops (rows × columns)
- 3D nested loops (i × j × k)
- Generate if containing loop
- Generate loop containing if
- Multiple separate generate blocks

**test_generate_params.v** (110 lines):
- Parameter-driven generate conditions
- Localparams in generate blocks
- WIDTH-parameterized designs
- Feature flags (enable/disable hardware)

**test_generate_genvar_ranges.v** (132 lines):
- Different genvar ranges: [0:N), [N:0) descending
- Non-zero start indices
- Large iteration counts
- Edge cases (empty ranges, single iteration)

**test_generate_mixed_constructs.v** (233 lines):
```verilog
// Generate with assigns
generate
  genvar i;
  for (i = 0; i < WIDTH; i = i + 1) begin : assign_bits
    assign out[i] = ~in[i];
  end
endgenerate

// Generate with instances
generate
  genvar i;
  for (i = 0; i < WIDTH; i = i + 1) begin : inv_array
    inverter u_inv (.a(in[i]), .b(out[i]));
  end
endgenerate

// Generate with always blocks
generate
  genvar i;
  for (i = 0; i < WIDTH; i = i + 1) begin : flop_array
    always @(posedge clk) begin
      data_out[i] <= data_in[i];
    end
  end
endgenerate

// Mixed: assigns + instances + always in same generate
```

**Complex test: Ripple-carry adder chain:**
```verilog
module gen_assign_instance #(parameter WIDTH = 4) (
  input [WIDTH-1:0] a, b,
  input cin,
  output [WIDTH-1:0] sum,
  output cout
);
  wire [WIDTH:0] carry;
  assign carry[0] = cin;
  assign cout = carry[WIDTH];

  generate
    genvar i;
    for (i = 0; i < WIDTH; i = i + 1) begin : add_stage
      full_adder u_fa (
        .a(a[i]),
        .b(b[i]),
        .cin(carry[i]),
        .sum(sum[i]),
        .cout(carry[i+1])
      );
    end
  endgenerate
endmodule
// Creates WIDTH full-adder instances with chained carries
```

**test_generate_complex.v** (140 lines):
- Complex parameter expressions
- Multiple nested conditions
- Mixed generate constructs

## Documentation Updates

### README.md (+56 lines, -23 lines)

**Status updated:**
- Old: "v0.2+ - Core pipeline with reduction operators, signed arithmetic, and 4-state logic support. 79 passing test cases"
- New: "v0.3 - Full generate/loop coverage, signed arithmetic, and 4-state logic support. 114 passing test cases in `verilog/pass/`"

**Features moved to "Working":**
- Generate blocks with genvar and for/if-generate
- Functions (inputs + single return assignment, inlined during elaboration)
- Initial blocks (already working in REV8, now documented)
- For/while/repeat loops (already working in REV8, now documented)

**Examples expanded:**
- Generate for loop example
- Generate if/else example
- Function declaration and usage

**Feature list clarifications:**
- Port declarations work with and without `--4state`
- Multi-dimensional arrays explicitly mentioned as supported
- Indexed part-selects (`[i +: 4]`, `[i -: 4]`) documented

### Licensing (713 lines)

**LICENSE** (661 lines):
- AGPL-3.0 (GNU Affero General Public License v3.0)
- Copyleft license requiring source disclosure
- Network use clause: modified versions used over network must provide source
- Chosen for open development while protecting against proprietary forks

**LICENSE.COMMERCIAL** (52 lines):
- Commercial licensing option available
- Allows proprietary use without AGPL obligations
- Contact information for licensing inquiries
- Dual-licensing model: open-source (AGPL) + commercial option

**Implications:**
- Open-source projects: Free use under AGPL
- Commercial/proprietary projects: Need commercial license
- Contribution requirement: Contributors grant license to use their code
- Patent grant: Users get patent license for covered claims

## Known Gaps and Limitations

### Improvements Over REV10

**Now Working:**
- Generate blocks (for loops, if/else, labels)
- Genvar declarations and scoping
- Functions with inlining
- Nested generate constructs
- Generate with all module elements (nets, assigns, instances, always)
- 114 passing tests (up from 102)

**Generate Limitations (v0.3):**
- Generate conditions must be **constant-evaluable** at elaboration time
- Cannot use runtime signals in generate conditions
- Generate case statements not implemented (only if/else)
- No `disable` of generate blocks
- Labels required for proper scoping (Verilog-2001 requirement)

**Function Limitations (v0.3):**
- Only simple functions (inputs + single assignment return)
- No automatic functions (with variables)
- No timing controls in functions
- Recursive functions may cause elaboration issues (not tested)
- Task support still missing (tasks have side effects, functions don't)

**Still Missing:**
- System tasks (`$display`, `$monitor`, `$finish`, etc.)
- Tasks (procedural blocks with side effects)
- Timing controls (`#` delay)
- Some advanced Verilog features (attributes, configurations, etc.)
- Host code emission - still stubbed
- Runtime - no execution

### Semantic Notes (v0.3)

**Generate elaboration:**
- Happens **after** parameter resolution but **before** module flattening
- Generated elements become indistinguishable from hand-written code
- Large generate loops create many module elements (watch elaboration memory)

**Function vs. Task:**
- Function: Returns value, no side effects, can be used in expressions
- Task: No return value, can have side effects (reg assignments), statement only
- v0.3 implements functions only

**Genvar vs. Integer:**
- Genvar: Generate-time only, must be constant
- Integer: Simulation-time variable, used in for/while loops in always blocks
- Cannot mix: genvar in generate, integer in procedural loops

## Statistics

- **Files changed**: 18
- **Lines added**: 3,733
- **Lines removed**: 179
- **Net change**: +3,554 lines

**Breakdown:**
- Parser: +1,551 lines (generate/genvar/function parsing, expression cloning)
- Elaboration: +446 lines (generate expansion, function inlining, genvar substitution)
- Codegen: +191 lines (enhanced expression handling)
- LICENSE files: +713 lines (AGPL-3.0 + commercial option)
- README: +56 lines (v0.3 status, feature list updates)
- AST header: +3 lines (minor additions)
- Documentation: +67 lines (keyword updates)

**Test suite:**
- `verilog/pass/`: 114 files (up from 102 in REV10)
  - 7 new generate tests added (803 lines)
  - 1 test moved from root (test_generate.v)
- `verilog/`: 3 files (down from 4 in REV10)
  - test_generate.v moved to pass/

**Code complexity:**
- Parser complexity increased significantly (+1,551 lines for generate syntax)
- Elaboration phase now handles 3 types of unrolling:
  1. Regular loops (for/while/repeat in procedural blocks)
  2. Generate loops (for in generate blocks)
  3. Function inlining (expression substitution)

This commit elevates metalfpga to v0.3 with **mature parameterizable hardware support**. Generate blocks are essential for real-world RTL designs (creating instance arrays, conditional hardware inclusion, parameterized modules). Combined with multi-dimensional arrays (REV10) and loops (REV8), metalfpga now handles a substantial subset of synthesizable Verilog used in production designs.
