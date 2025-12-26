# REV1 - v0.1 Flattening works, subset of keywords implemented

**Commit:** 3075e64
**Date:** Tue Dec 23 17:58:15 2025 +0100
**Message:** v0.1 - Flattening works, subset of keywords implemented

## Overview

This is the foundational commit that establishes the complete v0.1 architecture. It implements a working Verilog-to-Metal (MSL) compiler pipeline from parsing through code generation. The system can parse a Verilog-2001 subset, flatten module hierarchies, and emit MSL code. No runtime execution is implemented yet.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | Verilog-2001 subset: module, ports, wire/reg, assign, always, operators, parameters, instances |
| **Elaborate** | ✓ Functional | Flattens hierarchy, validates single-driver rule, connects ports |
| **Codegen (2-state)** | ✓ MSL emission | Generates combinational and sequential kernels, no 4-state support |
| **Codegen (4-state)** | ✗ Not implemented | All values are 2-state (0/1), no X/Z |
| **Host emission** | ✗ Stubbed only | Headers exist but no implementation |
| **Runtime** | ✗ Not implemented | No Metal kernel execution |

## User-Visible Changes

**New CLI:**
- `metalfpga_cli <input.v>` - Parse and elaborate Verilog
- `--dump-flat` - Pretty-print flattened module
- `--emit-msl <path>` - Write generated MSL code
- `--top <module>` - Specify top-level module

**Supported Verilog Features:**
- Module declarations with ports (input/output/inout)
- Wire and reg declarations with bit widths
- Continuous assignments (`assign`)
- Always blocks (`@(posedge clk)`, `@(negedge clk)`)
- Sequential statements (blocking `=` and non-blocking `<=`)
- If/else with begin/end blocks
- Expressions: arithmetic, bitwise, logical, comparison, shift, ternary
- Bit/part selects: `a[3]`, `a[7:0]`
- Concatenation and replication: `{a, b}`, `{N{a}}`
- Parameters and localparam
- Module instantiation with port connections and parameter overrides
- Based literals: `8'd255`, `4'b1010`, `16'hCAFE`

## Architecture

The system follows this pipeline:
1. **Frontend** - Parse Verilog → AST
2. **Elaboration** - Flatten AST → Single top module
3. **Codegen** - Emit MSL kernels (combinational + sequential)
4. **Runtime** - (placeholder for Metal execution)

---

## Build System

### `CMakeLists.txt`
- C++17 required, standard compliant
- Builds static library `metalfpga` containing all core components
- Builds CLI executable `metalfpga_cli` linked against the library
- Source organization:
  - Frontend: AST and Verilog parser
  - Core: Elaboration (flattening)
  - IR: Intermediate representation (placeholder)
  - Codegen: MSL and host code generation
  - Runtime: Metal runtime (placeholder)
  - Utils: Diagnostics

---

## Documentation

### `README.md`
Quick start guide pointing to the compiler pipeline. Documents basic usage:
```sh
cmake -S . -B build
cmake --build build
./build/metalfpga_cli path/to/design.v --dump-flat
```

### `docs/gpga/README.md`
Describes the GPGA (GPU-backed FPGA simulation) concept:
- **Goal**: Translate Verilog subset to MSL compute kernels
- **Pipeline**: Verilog → AST → Elaboration → IR → Codegen (MSL + Host) → Runtime
- **Versioning**: v0 = Verilog-2001 subset, v1 = more features, v2 = SystemVerilog

### `docs/gpga/verilog_words.md`
Documents implemented and planned Verilog features:
- **Implemented**: module, ports (input/output/inout), wire, reg, assign, always, posedge/negedge, if/else, operators, bit-selects, parameters, ternary, concat/replication
- **Planned**: generate/genvar, case statements, loops, functions/tasks

### `docs/gpga/ir_invariants.md`
Specifies guarantees about the flattened netlist (elaboration output).

### `docs/gpga/roadmap.md`
Project milestone plan.

---

## Frontend: Parsing

### `src/frontend/ast.hh`
Defines the complete AST structure for Verilog subset.

**Key data structures:**

```cpp
enum class PortDir { kInput, kOutput, kInout };
struct Port { PortDir dir; std::string name; int width; };

enum class NetType { kWire, kReg };
struct Net { NetType type; std::string name; int width; };

enum class ExprKind { kIdentifier, kNumber, kUnary, kBinary, kTernary, kSelect, kConcat };
struct Expr {
  ExprKind kind;
  std::string ident;              // for kIdentifier
  uint64_t number;                // for kNumber
  int number_width;               // sized literal width
  bool has_width, has_base;
  char base_char;                 // 'b', 'h', 'd', 'o'
  char op;                        // binary operator
  char unary_op;                  // unary operator
  std::unique_ptr<Expr> lhs, rhs; // for kBinary
  std::unique_ptr<Expr> operand;  // for kUnary
  std::unique_ptr<Expr> condition, then_expr, else_expr; // for kTernary
  std::unique_ptr<Expr> base;     // for kSelect
  int msb, lsb;                   // bit select range
  bool has_range;
  std::vector<std::unique_ptr<Expr>> elements; // for kConcat
  int repeat;                     // replication count {N{...}}
};
```

**Statements:**
```cpp
enum class StatementKind { kAssign, kIf, kBlock };
struct Statement {
  StatementKind kind;
  SequentialAssign assign;        // lhs = rhs or lhs <= rhs
  std::unique_ptr<Expr> condition;
  std::vector<Statement> then_branch, else_branch, block;
};

struct SequentialAssign {
  std::string lhs;
  std::unique_ptr<Expr> rhs;
  bool nonblocking;               // true for <=, false for =
};
```

**Always blocks:**
```cpp
enum class EdgeKind { kPosedge, kNegedge };
struct AlwaysBlock {
  EdgeKind edge;
  std::string clock;
  std::vector<Statement> statements;
};
```

**Module structure:**
```cpp
struct Module {
  std::string name;
  std::vector<Port> ports;
  std::vector<Net> nets;
  std::vector<Assign> assigns;          // continuous assignments
  std::vector<Instance> instances;      // submodule instantiations
  std::vector<AlwaysBlock> always_blocks;
  std::vector<Parameter> parameters;
};
```

### `src/frontend/verilog_parser.cc`
Hand-written recursive descent parser.

**Tokenization:**
- Handles whitespace, line/block comments (`//`, `/* */`)
- Token types: Identifier, Number, Symbol, End
- Tracks line/column for error reporting
- `IsIdentStart()`, `IsIdentChar()` for identifier lexing

**Parser class structure:**
```cpp
class Parser {
  std::string path_;
  std::vector<Token> tokens_;
  size_t pos_ = 0;
  Diagnostics* diagnostics_;

  // Lookahead
  const Token& Peek() const;
  const Token& Peek(size_t lookahead) const;

  // Matching
  bool MatchSymbol(const char* symbol);
  bool MatchKeyword(const char* keyword);

  // Parsing methods
  bool ParseProgram(Program* out);
  bool ParseModule(Program* out);
  bool ParseExpression(std::unique_ptr<Expr>* out);
  // ... many more parse methods
};
```

**Operator precedence (lowest to highest):**
1. Ternary `?:`
2. Logical OR `||`
3. Logical AND `&&`
4. Bitwise OR `|`
5. Bitwise XOR `^`
6. Bitwise AND `&`
7. Equality/Inequality `==`, `!=`
8. Relational `<`, `>`, `<=`, `>=`
9. Shifts `<<`, `>>`
10. Add/Sub `+`, `-`
11. Mul/Div/Mod `*`, `/`, `%`
12. Unary `~`, `-`, `+`, `!`
13. Primaries: identifiers, numbers, bit-selects, concat, parentheses

**Binary operator encoding:**
- `'E'` = `==`
- `'N'` = `!=`
- `'L'` = `<=`
- `'G'` = `>=`
- `'l'` = `<<`
- `'r'` = `>>`

**Number parsing:**
- Unsized decimals: `42`
- Sized: `8'd255`, `4'b1010`, `16'hCAFE`, `3'o7`
- Extracts width, base character, and value

**Module instantiation:**
- Named port connections: `.port(signal)`
- Positional connections
- Parameter overrides: `#(.PARAM(value))`

**Always blocks:**
- `always @(posedge clk)` or `@(negedge clk)`
- Parses nested if/else and begin/end blocks
- Distinguishes blocking `=` vs non-blocking `<=`

---

## Core: Elaboration

### `src/core/elaboration.cc`
Flattens the module hierarchy into a single "top" module.

**Key functions:**

`FindTopModule()`
- Identifies modules not instantiated anywhere
- Ensures exactly one top-level module exists
- Allows explicit `--top` override

`CloneExpr()` and `CloneStatement()`
- Deep-copy expressions/statements while renaming signals
- Used when instantiating submodules to create unique copies
- Takes a `rename` function to map signals to flattened names

`AddFlatNet()`
- Adds a net to the flattened module
- Detects name collisions
- Maintains `flat_to_hier` debug map for error reporting

`MakeNumberExpr()`
- Creates constant number expressions (used for unconnected inputs)

`CollectAssignedSignals()`
- Recursively walks statements to find all LHS signals
- Used for multiple-driver detection

`ValidateSingleDrivers()`
- Ensures each signal has at most one driver
- Checks continuous assigns vs always blocks
- Error if multiple sources drive the same signal

**Flattening algorithm:**
1. Find top module
2. For each instance:
   - Resolve parameter overrides (constant folding)
   - Create unique names: `instName_sigName`
   - Clone module contents with renamed signals
   - Connect instance ports to parent signals
   - Recursively flatten sub-instances
3. Collect all nets, assigns, always blocks into flat module
4. Validate single-driver rule

**Unconnected port handling:**
- Unconnected inputs: default to 0 (warning issued)
- Unconnected outputs: ignored (warning issued)

**Hierarchical name map:**
- Maintains `flat_to_hier` mapping for debug output
- Shows original source location of flattened signals

---

## Codegen: MSL

### `src/codegen/msl_codegen.cc`
Emits Metal Shading Language code for GPU execution.

**v0.1 Width Inference Rules:**
- `MaskForWidth64(width)`: Compute bit mask (0 to 64 bits)
- `TypeForWidth(width)`: Return `"uint"` (≤32 bits) or `"ulong"` (>32 bits)
- `ZeroForWidth(width)`: Return `"0u"` or `"0ul"`
- `MaskForWidthExpr(expr, width)`: Emit `(expr & mask)` to truncate
- `ExtendExpr(expr, from_w, to_w)`: Zero-extend or cast as needed

**Expression width computation (v0.1 implementation):**
- **Identifiers**: Port or net width from declaration
- **Numbers**: Sized literal width (`8'd42` → 8 bits) or minimal width for unsized (`42` → 6 bits)
- **Unary ops**: Operand width preserved
- **Binary arithmetic** (`+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`): `max(lhs_width, rhs_width)`
- **Comparison ops** (`==`, `!=`, `<`, `>`, `<=`, `>=`): Always 1 bit (boolean result)
- **Shifts** (`<<`, `>>`): Left-hand side width (shift amount doesn't affect width)
- **Ternary** (`? :`): `max(then_width, else_width)`
- **Bit select** (`a[msb:lsb]`): `(msb - lsb + 1)`
- **Concat** (`{a, b, ...}`): Sum of element widths
- **Replication** (`{N{a, b}}`): Sum of element widths × repeat count

**v0.1 Simplifications:**
- All values are **unsigned** (no sign extension)
- No signedness tracking (`signed` keyword not supported)
- Unsized literals use minimal bit representation (not Verilog-compliant 32-bit default)

**Expression emission:**
- `EmitExpr()`: Recursively emit MSL code for expressions
- `EmitExprSized()`: Emit expression and cast/mask to target width
- `EmitConcatExpr()`: Handle `{a, b}` and `{N{a, b}}`
  - Shifts and ORs elements into position
  - Uses `ulong` for >32 bits

**Binary operators:**
- Encoded chars (`'E'`, `'N'`, `'L'`, `'G'`, `'l'`, `'r'`) → MSL operators
- Width-extends operands to common width before operation
- Comparisons return 1-bit result (0 or 1)

**v0.1 Shift semantics:**
- Implements Verilog rule: `a << s` where `s >= width(a)` yields 0
- Emits: `(s >= width) ? 0 : (a << s)`
- Note: Variable shift amounts may not be fully clamped in all edge cases

**Code structure:**
- `EmitCombinational()`: Emit kernel for continuous assigns
  - Evaluates all `assign` statements
  - Stores results in output buffers
- `EmitSequential()`: Emit kernel for always blocks
  - Reads current state from register buffers
  - Evaluates statements
  - Writes next state (non-blocking semantics)
- No actual kernel invocation yet (stubs)

---

## Codegen: Host

### `src/codegen/host_codegen.hh` / `.mm`
Placeholder headers/stubs for host-side Metal API code.
- Will contain buffer allocation, kernel dispatch, PSO creation
- Not implemented in v0.1

---

## Runtime

### `src/runtime/metal_runtime.hh` / `.mm`
Empty placeholder files for Metal runtime.

---

## Utilities

### `src/utils/diagnostics.cc` / `.hh`
**Severity levels:**
```cpp
enum class Severity { kInfo, kWarning, kError };
```

**SourceLocation:**
```cpp
struct SourceLocation {
  std::string file;
  int line;
  int column;
};
```

**Diagnostics class:**
```cpp
class Diagnostics {
  void Add(Severity, std::string message, SourceLocation = {});
  bool HasErrors() const;
  void Print(std::ostream&) const;
};
```

Used throughout parser and elaboration for error reporting.

---

## CLI

### `src/main.mm`
Command-line interface and driver.

**Flags:**
- `--emit-msl <path>`: Write generated MSL code
- `--emit-host <path>`: Write host code (stub)
- `--dump-flat`: Pretty-print flattened module
- `--top <module>`: Specify top-level module

**Flow:**
1. Parse command-line arguments
2. Read input Verilog file
3. Tokenize and parse → `Program`
4. Elaborate (flatten) → `Module` (flat)
5. Optionally dump flattened module (human-readable)
6. Generate MSL code
7. Write outputs

**Dump format:**
- Shows ports (direction, width, name)
- Shows nets (type, width, name)
- Shows assigns with expression widths
- Shows always blocks with edge/clock
- Pretty-prints expressions with width annotations
- Example:
  ```
  Port: input [7:0] a
  Port: output [7:0] sum
  Net: wire [7:0] temp
  Assign: sum = (a + temp) [width=8]
  Always @(posedge clk):
    temp <= a [width=8]
  ```

**Helper functions:**
- `DirLabel()`: Convert PortDir to string
- `SignalWidth()`: Look up signal width
- `ExprWidth()`: Compute expression width (duplicated from codegen)
- `IsAllOnesExpr()`: Check if expression is all 1's (for ~0 detection)

---

## Test Suite

**Test Organization:**
- All 47 test files in `verilog/pass/` directory
- Tests validate parser, elaboration, and MSL emission
- No runtime execution tests (runtime not implemented)

**Coverage areas:**

**Basic operations:**
- `adder.v`: Simple adder
- `aluadder.v`: ALU with flags
- `shifter.v`: Shift operations
- `parens.v`: Parentheses grouping

**Operators:**
- `unarychains.v`: Chained unary ops like `~~~a`
- `nestedternary.v`: Nested `?:` operators

**Bit manipulation:**
- `selects.v`: Bit and part selects `a[3]`, `a[7:0]`
- `concat_nesting.v`: Concatenations `{a, b, c}`
- `repl_nesting.v`: Replications `{N{a}}`
- `shiftmaskconcat.v`: Combined shift/mask/concat
- `shiftprescedence.v`: Shift operator precedence

**Width handling:**
- `unsized_vs_sized.v`: `42` vs `8'd42`
- `widthmismatch.v`: Mismatched widths
- `zerowidth.v`: Zero-width parameters
- `overshift.v`: Shift by >= width

**Hierarchy:**
- `nestedmodules.v`: Module instantiation
- `ghostinstancechain.v`: Chain of instances
- `paramshadow.v`: Parameter shadowing

**Sequential logic:**
- `sequcomb.v`: Sequential and combinational mixed

**Edge cases:**
- `nowhitespace.v`: No whitespace between tokens
- `lit_matrix.v`: Literal formats
- `everything.v`: Kitchen-sink test
- `multidriver.v`: Multiple driver detection (should error)

---

## Key Algorithms

### Parameter Override Resolution
- Constant-fold parameter expressions
- Substitute into instantiated module
- Rename signals to avoid collisions

### Width Inference
- Bottom-up traversal of expression tree
- Unsized literals get minimal width
- Binary ops: max of operands (except comparisons → 1 bit)
- Concat: sum of elements

### Expression Masking
- Emit `(expr) & mask` to enforce width
- Cast between `uint`/`ulong` when crossing 32-bit boundary

### Flattening
- DFS through instance hierarchy
- Rename: `instance.signal` → `instance_signal`
- Clone module contents for each instance
- Connect ports via assignments or inline substitution

---

## Known Gaps and Limitations

### Frontend (Parse) Missing
- `generate` / `genvar` constructs
- `case` / `casex` / `casez` statements
- Loops: `for`, `while`, `repeat`
- Functions and tasks
- Multi-dimensional arrays
- Tri-state net types (`tri`, `triand`, `trior`, etc.)
- SystemVerilog constructs

### Elaboration Missing
- Array/memory flattening (parsed but not elaborated)
- Multi-dimensional array support
- Validation of blocking vs non-blocking mixing in same always block

### Codegen Missing
- **4-state logic**: All values are 2-state (0/1), no X/Z support
- **Signedness**: All values treated as unsigned, no sign extension
- Memory/array codegen (parsed and elaborated but not emitted to MSL)
- Host code generation (stubbed only)

### Runtime Missing
- **No execution**: Metal runtime not implemented
- No kernel dispatch or buffer management
- No test vector execution

### Semantic Simplifications (v0.1)
- **Unsized literals**: Use minimal width instead of Verilog-compliant 32-bit default
- **Shift clamping**: Variable shift amounts may not be fully clamped in edge cases
- **Width inference**: Simplified rules (see Codegen section for details)

---

## Design Philosophy

1. **Explicit width tracking**: Every expression has a computable width
2. **Zero-extension default**: Smaller values extend to larger contexts
3. **Masking for truncation**: Larger values masked down when assigned to smaller signals
4. **Single-driver rule**: Enforced during elaboration
5. **Verilog shift semantics**: Overshifts yield zero
6. **Error locality**: Diagnostics include file/line/column

---

## Statistics
- **Files changed**: 47 (all new)
- **Lines added**: 3,983
- **Core components**:
  - Parser: Hand-written recursive descent
  - Elaboration: Module flattening with validation
  - Codegen: MSL emission for combinational and sequential logic
- **Test coverage**: 47 Verilog test cases
- **Documentation**: README, architecture docs, keyword reference, roadmap
