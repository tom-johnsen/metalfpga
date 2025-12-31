# IEEE 1364-2005 Implementation-Defined Behavior Decisions

This document records the metalfpga implementation decisions for all 27 VARY test cases in the IEEE 1364-2005 conformance test suite. These tests represent areas where the standard allows implementation-defined behavior, and this document establishes metalfpga's official behavior for each case.

**Document Status:** Implementation Specification  
**Last Updated:** December 31, 2025  
**Applies To:** metalfpga (current)

---

## Table of Contents

1. [Lexical Conventions](#1-lexical-conventions)
2. [Data Types and Parameters](#2-data-types-and-parameters)
3. [Expressions and Operators](#3-expressions-and-operators)
4. [Procedural Assignments](#4-procedural-assignments)
5. [Gate-Level Modeling](#5-gate-level-modeling)
6. [Hierarchical Structures](#6-hierarchical-structures)
7. [Specify Blocks](#7-specify-blocks)
8. [Timing Checks](#8-timing-checks)
9. [SDF Backannotation](#9-sdf-backannotation)
10. [System Tasks and Functions](#10-system-tasks-and-functions)
11. [Compiler Directives](#11-compiler-directives)

---

## 1. Lexical Conventions

### 1.1 Unsized Integer Constants (test_03_05_01_1)

**IEEE Reference:** Section 3.5.1 - Integer constants

**Test Case:**
```verilog
module test;
  parameter p00 = 659;      // decimal - what width?
  parameter p01 = 'h 837FF; // hexadecimal - what width?
  parameter p02 = 'o07460;  // octal - what width?
endmodule
```

**Issue:** IEEE 1364-2005 requires unsized integer constants to be **at least 32 bits**, but leaves room for implementation differences (for example: whether/when an unsized literal is extended beyond 32 bits to fit its digits/value, and how aggressively tools warn about non-portable usage).

**Decision:**
> **Unsized constants default to 32-bit width; apply IEEE sizing rules afterward.**

**Rationale:** 
- 32-bit default matches most commercial simulators (Icarus Verilog, VCS, ModelSim)
- Provides adequate range for most use cases
- IEEE Std 1364-2005 context-dependent sizing rules apply after the 32-bit default

**Implementation Notes:**
- Size: start at 32 bits (minimum). If the literal cannot be represented in 32 bits, extend the literal width to fit.
- Signedness (per common IEEE 1364-2005 rules): unsized **decimal** numbers behave as signed; unsized **based** numbers (e.g. `'h...`) behave as unsigned unless an explicit `s` is present.
- After literal formation, apply normal IEEE expression sizing/signing rules for the surrounding context.

---

### 1.2 Signed Number Literals (test_03_05_01_3)

**IEEE Reference:** Section 3.5.1 - Integer constants with sign

**Test Case:**
```verilog
module test;
  parameter p01 = -8 'd 6;  // two's complement of 6 in 8 bits
  parameter p02 = 4 'shf;   // signed 4-bit 0xF = -1
  parameter p03 = -4 'sd15; // -(−1) = 1
  parameter p04 = 16'sb?;   // should be same as 16'sbz
endmodule
```

**Issue:** The standard requires specific handling of signed literals and the special case where 'sb?' should be treated as 'sbz'.

**Decision:**
> **Signed literal semantics; treat 16'sb? as 16'sbz.**

**Rationale:**
- Strict compliance with IEEE signed number semantics
- 's' prefix indicates signed interpretation (two's complement)
- '?' character should be treated identically to 'z' in all bases

**Implementation Notes:**
- Support 'sd', 'sh', 'so', 'sb' prefixes for signed literals
- Implement proper two's complement arithmetic
- Map '?' → 'z' during literal parsing

---

### 1.3 Real Number Literal Formats (test_03_05_02_1)

**IEEE Reference:** Section 3.5.2 - Real constants

**Test Case:**
```verilog
module test;
  real x00 = 1.2;           // legal
  real x01 = 0.1;           // legal
  real x08 = 236.123_763_e-12; // legal with underscores
  
  // Potentially implementation-defined:
  real x09 = .12;    // leading decimal point
  real x10 = 9.;     // trailing decimal point
  real x11 = 4.E3;   // trailing decimal + exponent
  real x12 = .2e-7;  // leading decimal + exponent
endmodule
```

**Issue:** Abbreviated real number formats (leading/trailing decimal point) are not explicitly required by the standard.

**Decision:**
> **Accept abbreviated real literals with a warning.**

**Rationale:**
- Many legacy codebases use abbreviated formats
- Better compatibility with existing Verilog code
- Warning alerts users to potentially non-portable syntax
- Can be promoted to error with `--strict-1364` flag

**Implementation Notes:**
- Accept `.123` as `0.123`
- Accept `9.` as `9.0`
- Accept `.2e-7` as `0.2e-7`
- Emit warning: "Abbreviated real literal format is not standard"
- Use `--strict-1364` to reject these forms

---

## 2. Data Types and Parameters

### 2.1 Specparam to Parameter Assignment (test_04_10_03_2)

**IEEE Reference:** Section 4.10.3 - Specify parameters

**Test Case:**
```verilog
module RAM16GEN(DOUT, DIN, ADR, WE, CE);
  output [7:0] DOUT;
  input [7:0] DIN;
  input [5:0] ADR;
  input WE, CE;

  specparam dhold = 1.0;
  specparam ddly = 1.0;
  parameter width = 1;
  
  // Is this legal?
  parameter regsize = dhold + 1.0;
endmodule
```

**Issue:** Using specparam values in regular parameter assignments is questionable because specparams are meant for specify blocks only.

**Decision:**
> **Allow specparam->parameter with warning by default; reject with --strict-1364.**

**Rationale:**
- Some commercial simulators allow this for convenience
- Provides flexibility for timing-related parameters
- Warning mode catches potentially non-portable code
- Strict mode enforces proper separation of concerns

**Implementation Notes:**
- Default mode: Allow with warning "Using specparam in parameter expression is non-standard"
- `--strict-1364`: Reject with error "Cannot use specparam in parameter assignment"
- Type checking: Ensure specparam is numeric (time/real)

---

## 3. Expressions and Operators

### 3.1 X/Z Replication Counts (test_05_01_14_2)

**IEEE Reference:** Section 5.1.14 - Concatenations (Replication)

**Test Case:**
```verilog
module test;
  reg a;
  reg b;
  reg w;
  reg [6:0] result;

  initial begin
    result[3:0] = {4{w}};

`ifdef NEGATIVE_TEST
    result = {1'bz{1'b0}};   // illegal
    result = {1'bx{1'b0}};   // illegal
`endif

    result = {b, {3{a, b}}};
  end
endmodule
```

**Issue:** Replication counts containing X or Z values cannot be evaluated to a specific count.

**Decision:**
> **Reject X/Z replication counts.**

**Rationale:**
- Replication requires a definite integer count
- X and Z are not numeric values
- All major simulators reject this
- Catches obvious errors early

**Implementation Notes:**
- Error during elaboration: "Replication count cannot contain X or Z values"
- Applies to `{count{expr}}` syntax
- Must evaluate count to integer at compile time

---

### 3.2 Zero Replication Alone (test_05_01_14_3)

**IEEE Reference:** Section 5.1.14 - Concatenations

**Test Case:**
```verilog
module test;
  parameter P = 32;
  reg [31:0] a;
  wire [31:0] b;
  wire [31:0] c;

  // Legal - zero replication with other elements
  assign b[31:0] = { {32-P{1'b1}}, a[P-1:0] };

`ifdef NEGATIVE_TEST
  // Illegal for P=32 because the zero replication appears alone within an inner concatenation.
  // (When P=32, 32-P == 0, so {32-P{1'b1}} has width 0; the extra braces make it the only
  // element of a concatenation.)
  assign c[31:0] = { {{32-P{1'b1}}}, a[P-1:0] };

  // Also illegal for P=32 in the suite:
  initial $displayb({32-P{1'b1}}, a[P-1:0]);
`endif
endmodule
```

**Issue:** A **zero replication** can create an **empty (zero-width) concatenation**. IEEE 1364-2005 treats a zero replication that appears *alone* within a concatenation as illegal (this commonly happens via extra braces that turn the replication into its own concatenation element).

**Decision:**
> **Error on zero replication used alone (compile-time if determinable).**

**Rationale:**
- Empty concatenation is semantically meaningless
- Cannot determine bit width
- Static analysis can catch many cases
- Indicates likely logic error

**Implementation Notes:**
- Compile-time error if replication count is constant zero
- Runtime check if count is parameter-dependent
- Error message: "Concatenation cannot contain only zero-width replication"
- Suggest adding a valid element or checking parameter ranges

---

### 3.3 Indexed Part-Select Expressions (test_05_02_01_1)

**IEEE Reference:** Section 5.2.1 - Vector bit-select and part-select addressing

**Test Case:**
```verilog
`define lsb_base_expr (4 - 1)
`define msb_base_expr (2 * 3)
`define width_expr    (1 + 1)

module test;
  reg [15:0] big_vect;
  reg [0:15] little_vect;

  initial begin
    big_vect[`lsb_base_expr +: `width_expr] = 8'b1010_1010;
    little_vect[`msb_base_expr +: `width_expr] = 8'b1010_1010;
    big_vect[`msb_base_expr -: `width_expr] = 8'b1010_1010;
    little_vect[`lsb_base_expr -: `width_expr] = 8'b1010_1010;
  end
endmodule
```

**Issue:** Support for constant expressions in indexed part-select (+: and -:) operators.

**Decision:**
> **Support constant-expression indexed part-selects (+: and -:).**

**Rationale:**
- IEEE 1364-2005 explicitly supports this syntax
- Essential for parameterized code
- Width expression must be constant
- Base expression evaluated at runtime

**Implementation Notes:**
- `vec[base +: width]` - ascending from base
- `vec[base -: width]` - descending from base
- Width must be constant expression (evaluate at elaboration)
- Base can be variable (runtime)
- Validate width > 0 at elaboration time

---

### 3.4 String Padding in Concatenation (test_05_02_03_2_1)

**IEEE Reference:** Section 5.2.3.2 - String value padding

**Test Case:**
```verilog
module test;
  reg [8*10:1] s1, s2;

  initial begin
    s1 = "Hello";      // padded to 80 bits
    s2 = " world!";    // padded to 80 bits
    if ({s1,s2} == "Hello world!")  // Does this match?
      $display ("strings are equal");
  end
endmodule
```

**Issue:** String variables are zero-padded on the left. When concatenated, the padding affects comparison results.

**Decision:**
> **String padding: METALFPGA_STRING_PAD=zero|space (default zero).**

**Rationale:**
- IEEE standard specifies zero-padding (default)
- Some codebases expect space padding for string operations
- Environment variable provides override
- Default matches IEEE standard

**Implementation Notes:**
- Default: Pad strings with `8'h00` on the left
- `METALFPGA_STRING_PAD=space`: Pad with `8'h20` (space character)
- Padding applied when string is shorter than target width
- Affects string comparisons and concatenations
- Document in user manual

---

### 3.5 Expression Bit-Length and Overflow (test_05_04_02_1)

**IEEE Reference:** Section 5.4.2 - Expression bit lengths

**Test Case:**
```verilog
module test;
  reg [15:0] a, b, answer;

  initial begin
    answer = (a + b) >> 1;       // may lose carry bit
    answer = (a + b + 0) >> 1;   // forces proper width
  end
endmodule
```

**Issue:** During expression evaluation, interim results take the size of the largest operand. Adding two 16-bit values produces a 16-bit result (losing the carry).

**Decision:**
> **IEEE expression sizing; no implicit extra carry bit.**

**Rationale:**
- Strict compliance with IEEE 1364-2005 Section 5.4
- Predictable, well-defined behavior
- Matches hardware synthesis semantics
- Users must explicitly widen expressions if overflow matters

**Implementation Notes:**
- Expression width = max(operand widths)
- No automatic width extension for overflow
- `(16-bit + 16-bit)` → 16-bit result
- Add zero or cast to force width: `(a + b + 0)` or `(17'b0 + a + b)`
- Document this behavior prominently

---

## 4. Procedural Assignments

### 4.1 Array Initialization in Declarations (test_06_02_01_2)

**IEEE Reference:** Section 6.2.1 - Variable declaration assignment

**Test Case:**
```verilog
module test;
  // Is this legal?
  reg [3:0] array [3:0] = 0;
endmodule
```

**Issue:** Initializing entire arrays in the declaration is not supported in Verilog-2005 (added in SystemVerilog).

**Decision:**
> **Reject array initialization in declarations.**

**Rationale:**
- Not part of IEEE 1364-2005 (Verilog-2005)
- Feature added in IEEE 1800-2005 (SystemVerilog)
- Enforces standard compliance
- Clear error message guides users to correct syntax

**Implementation Notes:**
- Error: "Array initialization not supported in Verilog-2005"
- Suggest: "Use initial block to initialize array elements"
- Example in error message:
  ```verilog
  initial begin
    for (i = 0; i < 4; i = i + 1)
      array[i] = 0;
  end
  ```

---

## 5. Gate-Level Modeling

### 5.1 Gate Array Instance Ranges (test_07_01_05_1)

**IEEE Reference:** Section 7.1.5 - Range specification

**Test Case:**
```verilog
module test(in1, in2, out1);
  input in1, in2;
  output out1;

  // Illegal - same identifier with different ranges
  nand #2 t_nand[0:3](out1, in1, in2), t_nand[4:7](out1, in1, in2);

  // Legal - single range or different identifiers
  nand #2 t_nand[0:7](out1, in1, in2);
  nand #2 x_nand[0:3](out1, in1, in2), y_nand[4:7](out1, in1, in2);
endmodule
```

**Issue:** An instance identifier can only be associated with one range specification.

**Decision:**
> **Reject duplicate gate array identifiers with different ranges.**

**Rationale:**
- IEEE standard explicitly forbids this
- Each identifier must have exactly one range
- Prevents ambiguous instance naming
- Helps catch copy-paste errors

**Implementation Notes:**
- Error: "Instance identifier 't_nand' declared with multiple ranges"
- Detect during gate instantiation parsing
- Suggest using different identifiers or combining ranges

---

## 6. Hierarchical Structures

### 6.1 Defparam in Generate Blocks (test_12_02_01_1)

**IEEE Reference:** Section 12.2.1 - defparam statement

**Test Case:**
```verilog
module test(in, in1, out1);
  input [7:0] in, in1;
  output [7:0] out1;
  genvar i;

  generate
    for (i = 0; i < 8; i = i + 1) begin : somename
      flop my_flop(in[i], in1[i], out1[i]);
      // Illegal - targeting other generate instance
      defparam somename[i+1].my_flop.xyz = i;
    end
  endgenerate
endmodule
```

**Issue:** Each generate block instance is a separate scope. A defparam cannot target parameters in other instances of the same generate loop.

**Decision:**
> **Reject defparam targeting other generate instances.**

**Rationale:**
- Each generate iteration creates independent hierarchy scope
- Cross-instance defparam violates scope isolation
- IEEE standard explicitly prohibits this
- Prevents complex dependency issues

**Implementation Notes:**
- Error: "defparam cannot target other generate block instances"
- Validate during generate elaboration
- Only allow defparam to target current instance's children
- Suggest using parameter overrides at instantiation instead

---

### 6.2 Mixed Parameter Override Styles (test_12_02_02_2_2)

**IEEE Reference:** Section 12.2.2.2 - Parameter value assignment by name

**Test Case:**
```verilog
module tb3;
  wire [9:0] out_a;
  reg  [9:0] in_a;
  reg        clk;

  // Legal - all positional
  vdff #(10, 15) mod_a(.out(out_a), .in(in_a), .clk(clk));
  
  // Legal - all named
  vdff #(.delay(12)) mod_c(.out(out_c), .in(in_c), .clk(clk));

  // Illegal - mixed positional and named
  vdff #(10, .delay(15)) mod_a(.out(out_a), .in(in_a), .clk(clk));
endmodule
```

**Issue:** Cannot mix positional and named parameter override styles in the same instantiation.

**Decision:**
> **Reject mixed positional + named parameter overrides.**

**Rationale:**
- IEEE standard requires consistent style per instantiation
- Prevents ambiguity in parameter assignment order
- All major simulators enforce this
- Clear error guides users to correct syntax

**Implementation Notes:**
- Error: "Cannot mix positional and named parameter overrides"
- Detect during module instantiation parsing
- Track override style (positional/named) from first parameter
- Suggest converting all to named or all to positional

---

### 6.3 Duplicate Port Declarations (test_12_03_03_1)

**IEEE Reference:** Section 12.3.3 - Port declarations

**Test Case:**
```verilog
module test(aport);
  input aport;  // First declaration - okay
  input aport;  // Error - duplicate declaration
  output aport; // Error - conflicting direction
endmodule
```

**Issue:** Port names must be declared exactly once.

**Decision:**
> **Reject duplicate port declarations.**

**Rationale:**
- IEEE standard forbids multiple declarations
- Prevents ambiguous port direction/type
- Standard error detection
- Should already be implemented in most parsers

**Implementation Notes:**
- Error: "Port 'aport' declared multiple times"
- Track declared ports in symbol table
- Detect during port declaration parsing
- Special case: Different error for conflicting directions vs. pure duplicates

---

### 6.4 Multiple Port Connections (test_12_03_06_3)

**IEEE Reference:** Section 12.3.6 - Port connections by name

**Test Case:**
```verilog
module test;
  // Illegal - connecting same ports multiple times
  a ia (.i (a), .i (b),   // input port 'i' connected twice
        .o (c), .o (d),   // output port 'o' connected twice
        .e (e), .e (f));  // inout port 'e' connected twice
endmodule
```

**Issue:** Each port can only be connected once in a module instantiation.

**Decision:**
> **Reject multiple connections to the same port.**

**Rationale:**
- IEEE standard prohibits multiple connections
- Hardware has single physical connection per port
- Prevents synthesis and simulation ambiguity
- Detect during elaboration

**Implementation Notes:**
- Error: "Port '.i' connected multiple times in instance 'ia'"
- Track connected ports during instantiation parsing
- Use hash set to detect duplicates
- Apply to input, output, and inout ports

---

### 6.5 Generate Loop Variable Scoping (test_12_04_01_1)

**IEEE Reference:** Section 12.4.1 - Loop generate constructs

**Test Case:**
```verilog
module mod_a;
  genvar i;
  
  for (i = 0; i < 5; i = i + 1) begin : a
    // Illegal - reusing 'i' for nested loop
    for (i = 0; i < 5; i = i + 1) begin : b
      ...
    end
  end
endmodule

module mod_b;
  genvar i;
  reg a;
  
  // Illegal - 'a' conflicts with reg 'a'
  for (i = 1; i < 0; i = i + 1) begin : a
  end
endmodule
```

**Issue:** Generate loop variables have specific scoping rules, and generate block names cannot conflict with other identifiers.

**Decision:**
> **Enforce genvar scoping and generate block name conflicts.**

**Rationale:**
- Each genvar has defined scope
- Nested loops must use different genvars
- Generate block names are identifiers in module scope
- Standard compliance requirement

**Implementation Notes:**
- Error 1: "genvar 'i' already in use by enclosing generate loop"
- Error 2: "Generate block name 'a' conflicts with identifier 'a'"
- Track active genvars during generate parsing
- Check generate block names against all module identifiers
- Suggest using different names

---

## 7. Specify Blocks

### 7.1 Conditional Path Destination Consistency (test_14_02_04_3_4)

**IEEE Reference:** Section 14.2.4.3 - Edge-sensitive state-dependent paths

**Test Case:**
```verilog
module test(clk, data, q, reset, cntrl);
  input clk, reset, cntrl;
  output data;
  output [4:0] q;

  specify
    // Illegal - inconsistent destination specifications
    if (reset)
      (posedge clk => (q[3:0]:data)) = (10,5);  // part-select
    if (!reset)
      (posedge clk => (q[0]:data)) = (15,8);    // bit-select
  endspecify
endmodule
```

**Issue:** State-dependent paths to the same destination must use consistent destination specification styles.

**Decision:**
> **Enforce consistent dest specs in conditional specify paths.**

**Rationale:**
- Mixed part-select and bit-select is ambiguous
- All conditions for same destination must match
- IEEE standard requires consistency
- Prevents timing annotation conflicts

**Implementation Notes:**
- Error: "Inconsistent destination specification for 'q' in conditional paths"
- Track destination format (bit-select vs part-select) for each output
- Validate all conditional paths to same destination use same format
- Applies within same module only

---

### 7.2 Multiple Conditional Path Delay Selection (test_14_03_03_2)

**IEEE Reference:** Section 14.3.3 - Delay selection

**Test Case:**
```verilog
module test(A, MODE, Y);
  input A;
  output integer MODE = 0;
  output Y;

  specify
    if (MODE < 5) (A => Y) = (5, 9);  // rise=5, fall=9
    if (MODE < 4) (A => Y) = (4, 8);  // rise=4, fall=8
    if (MODE < 3) (A => Y) = (6, 5);  // rise=6, fall=5
    if (MODE < 2) (A => Y) = (3, 2);  // rise=3, fall=2
    if (MODE < 1) (A => Y) = (7, 7);  // rise=7, fall=7
  endspecify
  // When MODE=2: paths 1,2,3 are active
  // Rise: min(5,4,6) = 4, Fall: min(9,8,5) = 5
endmodule
```

**Issue:** When multiple conditional paths are active, which delay should be selected?

**Decision:**
> **Delay select: METALFPGA_SPECIFY_DELAY_SELECT=fast|slow (default fast).**

**Rationale:**
- IEEE allows implementation choice
- "fast" (minimum) is most common for setup checks
- "slow" (maximum) useful for worst-case hold checks
- Environment variable allows mode selection per simulation

**Implementation Notes:**
- Default: `fast` mode (select minimum delay among active paths)
- `METALFPGA_SPECIFY_DELAY_SELECT=slow`: Select maximum delay
- Apply per transition type (rise vs fall)
- Document in timing analysis guide

---

### 7.3 Showcancelled Declaration Ordering (test_14_06_04_2_2)

**IEEE Reference:** Section 14.6.4.2 - Pulse detection

**Test Case:**
```verilog
module test(input a, b, output out);
  specify
    (a => out) = (2,3);
    
    // Illegal - showcancelled after path using 'out'
    showcancelled out;
    (b => out) = (3,4);
  endspecify
endmodule
```

**Issue:** The showcancelled/noshowcancelled declaration must appear before any module paths using that output.

**Decision:**
> **Enforce showcancelled/noshowcancelled ordering.**

**Rationale:**
- Pulse behavior must be established before paths are defined
- Prevents inconsistent pulse handling
- IEEE standard requirement
- Clear ordering prevents ambiguity

**Implementation Notes:**
- Error: "showcancelled for 'out' must appear before all paths using 'out'"
- Track pulse control declarations during specify block parsing
- Flag any paths declared before pulse control for same signal
- Apply to both showcancelled and noshowcancelled

---

## 8. Timing Checks

### 8.1 Timing Check Parameter Validation (test_15_03_04_2)

**IEEE Reference:** Section 15.3.4 - $width timing check

**Test Case:**
```verilog
module test(clr);
  input clr;
  reg notif;

  specify
    specparam lim = 10, thresh = 0.5;
    
    // Legal:
    $width(negedge clr, lim);
    $width(negedge clr, lim, thresh, notif);
    $width(negedge clr, lim, 0, notif);
    
    // Illegal:
    $width(negedge clr, lim, , notif);      // missing parameter
    $width(negedge clr, lim, notif);        // wrong position
  endspecify
endmodule
```

**Issue:** Timing check optional parameters must be in correct positions; cannot skip parameters with empty positions.

**Decision:**
> **Validate timing check params; no skipped args.**

**Rationale:**
- IEEE timing check syntax is positional
- Empty positions (,,) are not allowed for optional parameters
- Must use explicit 0 or default value
- Matches behavior of all major simulators

**Implementation Notes:**
- Error: "Timing check parameter cannot be omitted with ',,' syntax"
- Validate parameter count and positions during parsing
- Each timing check has defined signature
- Suggest using explicit 0 for default values

---

### 8.2 Negative Setup Times and Delayed Notifiers (test_15_05_01_4)

**IEEE Reference:** Section 15.5.1 - Notifiers for accurate simulation

**Test Case:**
```verilog
module test(CLK, D, dCLK, dD, Q);
  input CLK, D;
  inout dCLK, dD;
  output Q;

  specify
    (CLK => Q) = 6;  // Path delay
    $setuphold(posedge CLK, posedge D, -3,  8, , , , dCLK, dD);
    $setuphold(posedge CLK, negedge D, -7, 13, , , , dCLK, dD);
    // -7 setup creates 7 time unit delay for dCLK notifier
  endspecify
endmodule
```

**Issue:** Negative setup times can create delayed notifiers that affect output timing. How should this be handled?

**Decision:**
> **Negative setup: METALFPGA_NEGATIVE_SETUP_MODE=allow|clamp|error (default allow).**

**Rationale:**
- Negative setup times are legal in IEEE standard (represent hold-like behavior)
- Different simulation modes useful for different validation scenarios
- Allow mode supports advanced timing models
- Error mode catches potentially problematic specifications

**Implementation Modes:**
- `allow` (default): Support negative setup times per IEEE semantics, create delayed notifiers
- `clamp`: Clamp negative values to 0, warn user
- `error`: Reject negative setup times with error

**Implementation Notes:**
- When negative setup time magnitude > path delay, notifier is delayed
- Delayed notifier affects when output changes
- Document interaction between setup time and path delays
- Environment variable selectable per simulation

---

## 9. SDF Backannotation

### 9.1 SDF Conditional Timing Check Matching (test_16_02_02_2)

**IEEE Reference:** Section 16.2.2 - SDF timing check mapping

**Test Case:**
```verilog
module test(clk, mode, data);
  input clk, mode, data;
  reg ntfr;

  specify
    $setuphold(posedge clk &&&  mode, data, 1, 1, ntfr); // Annotated
    $setuphold(negedge clk &&& !mode, data, 1, 1, ntfr); // Not annotated
  endspecify
endmodule
```

**Issue:** SDF COND specifications must match Verilog timing check conditions for annotation to occur.

**Decision:**
> **SDF conditional match rules.**

**Rationale:**
- SDF COND and Verilog &&& must match exactly
- Prevents incorrect timing annotation
- IEEE-1497 (SDF) and IEEE-1364 coordination
- Annotation occurs only for matching conditions

**Implementation Notes:**
- Parse SDF COND expressions
- Compare against Verilog timing check conditions
- Exact match required (edge type + condition expression)
- Warn if SDF contains COND but no matching timing check found
- Log which timing checks were annotated

---

### 9.2 SDF Non-Matching Preserves Original Timing (test_16_02_02_3)

**IEEE Reference:** Section 16.2.2 - SDF timing check mapping

**Test Case:**
```verilog
module test(clk, mode, data);
  input clk, mode, data;
  reg ntfr;

  specify
    // Neither annotated if SDF doesn't match
    $setuphold(posedge clk &&&  mode, data, 1, 1, ntfr);
    $setuphold(negedge clk &&& !mode, data, 1, 1, ntfr);
  endspecify
endmodule
```

**Issue:** When SDF does not match any timing check, what happens to the original values?

**Decision:**
> **SDF no-match keeps original timing.**

**Rationale:**
- Original Verilog timing values are defaults
- SDF only overrides when matches occur
- Allows partial annotation
- Predictable fallback behavior

**Implementation Notes:**
- Unannotated timing checks retain original values
- Log which timing checks were not annotated (if verbose)
- Do not error on non-matching SDF entries
- Support partial SDF files

---

## 10. System Tasks and Functions

### 10.1 $random Signed Behavior (test_17_09_01_1)

**IEEE Reference:** Section 17.9.1 - $random function

**Test Case:**
```verilog
module test;
  reg [23:0] rnd;
  initial assign rnd = $random % 60;
endmodule
```

**Issue:** $random returns a signed 32-bit integer. Modulo with signed values produces signed results.

**Decision:**
> **$random is signed 32-bit; modulo preserves sign.**

**Rationale:**
- IEEE standard specifies signed return value
- Modulo of signed values follows C semantics
- Result range: -(N-1) to +(N-1) for % N
- Matches all major simulator behavior

**Implementation Notes:**
- `$random` is treated as a signed 32-bit value for arithmetic.
- With signed arithmetic, `$random % 60` evaluates to a signed value in the range -59..59.
- The suite assigns this expression into `reg [23:0] rnd;` (an unsigned vector). If the arithmetic result is negative, the stored bits are the 2’s-complement/truncated vector value (i.e., it will *not* remain a negative “integer” once stored in an unsigned vector).

---

### 10.2 $random Unsigned via Concatenation (test_17_09_01_2)

**IEEE Reference:** Section 17.9.1 - $random function

**Test Case:**
```verilog
module test;
  reg [23:0] rnd;
  initial assign rnd = {$random} % 60;
endmodule
```

**Issue:** Using concatenation operator forces unsigned context for $random.

**Decision:**
> **{$random} forces unsigned interpretation.**

**Rationale:**
- Concatenation creates unsigned bit vector
- Standard Verilog type coercion rules
- Provides documented way to get unsigned random
- Result range: 0 to (N-1) for % N

**Implementation Notes:**
- `{$random}` forms an unsigned 32-bit vector from the `$random` result.
- With an unsigned left operand, `% 60` yields a value in the range 0..59.
- As above, the final assignment to `reg [23:0]` stores the truncated vector value.

---

## 11. Compiler Directives

### 11.1 Macro String Splitting (test_19_03_01_2)

**IEEE Reference:** Section 19.3.1 - `define

**Test Case:**
```verilog
module test;
  // Illegal - macro splits string literal
  `define first_half "start of string
  $display (`first_half end of string");
endmodule
```

**Issue:** Macro definitions cannot split string literals across the macro boundary.

**Decision:**
> **Reject macros that split strings.**

**Rationale:**
- String literals must be complete lexical tokens
- Splitting breaks lexical analysis
- All simulators reject this
- Prevents confusing behavior

**Implementation Notes:**
- Error: "Macro definition cannot split a string literal"
- Detect unterminated string in macro definition
- Detect string continuation from macro substitution
- Suggest complete strings in macro or proper concatenation

---

### 11.2 Keyword Version Control (test_19_11_00_2)

**IEEE Reference:** Section 19.11 - `begin_keywords, `end_keywords

**Test Case:**
```verilog
`begin_keywords "1364-2005" // use IEEE Std 1364-2005 Verilog keywords
module m2;

`ifdef NEGATIVE_TEST
  wire [63:0] uwire; // ERROR: "uwire" is a keyword in 1364-2005
`endif
endmodule
`end_keywords
```

**Issue:** Different Verilog/SystemVerilog versions have different keyword sets. How to handle version-specific keywords?

**Decision:**
> **Implement begin_keywords/end_keywords; default 1364-2005.**

**Rationale:**
- IEEE 1364-2005 added this feature for compatibility
- Allows mixing legacy and modern code
- Default to 1364-2005 (Verilog-2005) for metalfpga
- Support version-specific keyword sets

**Supported Versions (initial scope):**
- `"1364-1995"` - Verilog-1995 keywords
- `"1364-2001"` - Verilog-2001 keywords
- `"1364-2005"` - Verilog-2005 keywords (default)

Other version strings should be treated as unknown until metalfpga explicitly adds and tests them.

**Implementation Notes:**
- Maintain keyword tables for each standard version
- Default (no directive): 1364-2005
- `begin_keywords` pushes new keyword set (stack-based)
- `end_keywords` pops keyword set
- Validate version string, error on unknown version
- Can nest different versions
- Each version defines: reserved words, allowed identifiers

**Reserved Keywords by Version:**
- 1364-2005 reserves `uwire` (relative to older 1364 revisions), which is what this suite case is exercising.


---

## Summary Table

| # | Test | Topic | Decision Type | Default Behavior |
|---|------|-------|---------------|------------------|
| 1 | test_03_05_01_1 | Unsized constant width | Fixed | 32-bit |
| 2 | test_03_05_01_3 | Signed literals | Fixed | IEEE compliant |
| 3 | test_03_05_02_1 | Real literal format | Configurable | Accept + warn |
| 4 | test_04_10_03_2 | Specparam→parameter | Configurable | Allow + warn |
| 5 | test_05_01_14_2 | X/Z replication | Fixed | Reject |
| 6 | test_05_01_14_3 | Zero replication | Fixed | Reject if alone |
| 7 | test_05_02_01_1 | Indexed part-select | Fixed | Support |
| 8 | test_05_02_03_2_1 | String padding | Configurable | Zero-pad |
| 9 | test_05_04_02_1 | Expression sizing | Fixed | IEEE sizing |
| 10 | test_06_02_01_2 | Array initialization | Fixed | Reject |
| 11 | test_07_01_05_1 | Gate array ranges | Fixed | Reject duplicates |
| 12 | test_12_02_01_1 | Generate defparam | Fixed | Reject cross-instance |
| 13 | test_12_02_02_2_2 | Mixed param styles | Fixed | Reject |
| 14 | test_12_03_03_1 | Duplicate ports | Fixed | Reject |
| 15 | test_12_03_06_3 | Multiple port connections | Fixed | Reject |
| 16 | test_12_04_01_1 | Genvar scoping | Fixed | Enforce |
| 17 | test_14_02_04_3_4 | Specify path dest | Fixed | Enforce consistency |
| 18 | test_14_03_03_2 | Delay selection | Configurable | Fast (minimum) |
| 19 | test_14_06_04_2_2 | Showcancelled order | Fixed | Enforce |
| 20 | test_15_03_04_2 | Timing check params | Fixed | Validate |
| 21 | test_15_05_01_4 | Negative setup | Configurable | Allow |
| 22 | test_16_02_02_2 | SDF conditional match | Fixed | Exact match |
| 23 | test_16_02_02_3 | SDF no-match | Fixed | Keep original |
| 24 | test_17_09_01_1 | $random signed | Fixed | Signed 32-bit |
| 25 | test_17_09_01_2 | {$random} unsigned | Fixed | Unsigned |
| 26 | test_19_03_01_2 | Macro string split | Fixed | Reject |
| 27 | test_19_11_00_2 | Keyword versioning | Configurable | 1364-2005 |

---

## Environment Variables Reference

| Variable | Values | Default | Purpose |
|----------|--------|---------|---------|
| `METALFPGA_STRING_PAD` | `zero`, `space` | `zero` | String padding character |
| `METALFPGA_SPECIFY_DELAY_SELECT` | `fast`, `slow` | `fast` | Multi-path delay selection |
| `METALFPGA_NEGATIVE_SETUP_MODE` | `allow`, `clamp`, `error` | `allow` | Negative setup time handling |

---

## Command-Line Flags Reference

| Flag | Purpose |
|------|---------|
| `--strict-1364` | Reject non-standard extensions (affects tests 3, 4) |

---

## Implementation Status

All 27 decisions documented. Implementation tracking in separate issue tracker.

**Next Steps:**
1. Review and approve decisions (Technical Review Board)
2. Implement each decision in metalfpga core
3. Add regression tests for each case
4. Update user documentation
5. Verify all 27 VARY tests pass with documented behavior

---

**Document Version:** 1.0  
**Approved By:** [TBD]  
**Effective Date:** [TBD]
