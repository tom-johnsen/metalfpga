# IEEE 1364-2005 Implementation-Defined Behaviors

This document catalogs all the VARY test cases from the IEEE 1364-2005 conformance test suite. These tests compiled successfully but represent implementation-defined behavior according to the Verilog-2005 standard. Each case requires a decision on how metalfpga should handle it.

**Summary:** 27 implementation-defined behavior tests  
**Status:** Decisions recorded (implementation pending)

---

## 1. Lexical Conventions - Number Constants

### Test: test_03_05_01_1
**Section:** IEEE Std 1364-2005, Section 3.5.1 - Integer constants  
**Topic:** Unsized constant numbers

**Description:**
IEEE 1364-2005 requires unsized integer constants to be at least 32 bits; tools differ in edge cases and diagnostics.

**Test Code:**
```verilog
module test;
  parameter p00 = 659;      // is a decimal number
  parameter p01 = 'h 837FF; // is a hexadecimal number
  parameter p02 = 'o07460;  // is an octal number
endmodule
```

**Decision:**
- Default unsized constant width is 32 bits (metalfpga default, matches IEEE minimum and most tools).
- If an unsized literal cannot be represented in 32 bits, extend the literal width to fit.
- Normal IEEE expression sizing/signing rules still apply for context (extend/truncate as needed).

---

### Test: test_03_05_01_3
**Section:** IEEE Std 1364-2005, Section 3.5.1 - Integer constants  
**Topic:** Using sign with constant numbers

**Description:**
The exact interpretation of signed number literals with specific bit widths.

**Test Code:**
```verilog
module test;
  parameter p01 = -8 'd 6;  // two's complement of 6 in 8 bits = -(8'd 6)
  parameter p02 = 4 'shf;   // 4-bit number '1111' = -1 in 2's complement
  parameter p03 = -4 'sd15; // equivalent to -(-4'd 1), or '0001'
  parameter p04 = 16'sb?;   // the same as 16'sbz
endmodule
```

**Decision:**
- Signed literal semantics; treat `16'sb?` as `16'sbz`.

---

### Test: test_03_05_02_1
**Section:** IEEE Std 1364-2005, Section 3.5.2 - Real constants  
**Topic:** Real number format restrictions

**Description:**
Some real number formats (e.g., `.12`, `9.`, `4.E3`, `.2e-7`) may not be legal in all implementations.

**Test Code:**
```verilog
module test;
  real x00 = 1.2;
  real x01 = 0.1;
  real x02 = 2394.26331;
  real x03 = 1.2E12;
  real x04 = 1.30e-2;
  real x05 = 0.1e-0;
  real x06 = 23E10;
  real x07 = 29E-2;
  real x08 = 236.123_763_e-12;
  
  // Potentially illegal:
  // real x09 = .12;    // leading decimal point
  // real x10 = 9.;     // trailing decimal point
  // real x11 = 4.E3;   // trailing decimal point with exponent
  // real x12 = .2e-7;  // leading decimal point with exponent
endmodule
```

**Decision:**
- Accept abbreviated real literals with a warning.

---

## 2. Data Types - Parameters

### Test: test_04_10_03_2
**Section:** IEEE Std 1364-2005, Section 4.10.3 - Specify parameters  
**Topic:** Cannot assign specparams to regular parameters

**Description:**
Using a specparam in a parameter assignment is implementation-defined or illegal.

**Test Code:**
```verilog
module RAM16GEN(DOUT, DIN, ADR, WE, CE);
  output [7:0] DOUT;
  input [7:0] DIN;
  input [5:0] ADR;
  input WE, CE;

  specparam dhold = 1.0;
  specparam ddly = 1.0;
  parameter width = 1;
  
  // Illegal - cannot assign specparams to parameters:
  // parameter regsize = dhold + 1.0;
endmodule
```

**Decision:**
- Default: allow `parameter = specparam_expression` (non-standard), emit a warning.
- Strict mode: reject when `--strict-1364` is set (IEEE compliant).

---

## 3. Expressions - Operators

### Test: test_05_01_14_2
**Section:** IEEE Std 1364-2005, Section 5.1.14 - Concatenations  
**Topic:** Illegal replications with X/Z values

**Description:**
Replication counts containing X or Z values are illegal.

**Test Code:**
```verilog
module test;
  reg a, b, w;
  reg [6:0] result;

  initial begin
    result[3:0] = {4{w}};    // Legal replication
    
    // Illegal replications:
    // result = {1'bz{1'b0}};   // illegal - Z as replication count
    // result = {1'bx{1'b0}};   // illegal - X as replication count
    
    result = {b, {3{a, b}}}; // Legal nested concatenation
  end
endmodule
```

**Decision:**
- Reject X/Z replication counts.

---

### Test: test_05_01_14_3
**Section:** IEEE Std 1364-2005, Section 5.1.14 - Concatenations  
**Topic:** Zero replication in concatenation

**Description:**
A zero replication appearing alone within a concatenation is illegal.

**Test Code:**
```verilog
module test;
  parameter P = 32;
  reg [31:0] a;
  wire [31:0] b, c;

  // Legal for all P from 1 to 32
  assign b[31:0] = { {32-P{1'b1}}, a[P-1:0] };

  // Illegal for P=32 (zero replication appears alone within an inner concatenation):
  // assign c[31:0] = { {{32-P{1'b1}}}, a[P-1:0] };
  // initial $displayb({32-P{1'b1}}, a[P-1:0]);
endmodule
```

**Decision:**
- Error on zero replication used alone (compile-time if determinable).

---

## 4. Expressions - Operands

### Test: test_05_02_01_1
**Section:** IEEE Std 1364-2005, Section 5.2.1 - Part-select addressing  
**Topic:** Indexed part-select with expressions

**Description:**
Using expressions in indexed part-select ('+:' and '-:') operators.

**Test Code:**
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

**Decision:**
- Support constant-expression indexed part-selects (+: and -:).

---

### Test: test_05_02_03_2_1
**Section:** IEEE Std 1364-2005, Section 5.2.3.2 - String value padding  
**Topic:** String concatenation with padding

**Description:**
String comparison after concatenation may be affected by zero-padding, and implementations may differ.

**Test Code:**
```verilog
module test;
  reg [8*10:1] s1, s2;

  initial begin
    s1 = "Hello";
    s2 = " world!";
    if ({s1,s2} == "Hello world!")
      $display ("strings are equal");
  end
endmodule
```

**Decision:**
- Provide two padding behaviors for short strings assigned to wider packed regs:
  - `zero` padding (IEEE-style, most simulators): NULs on the MSB side.
  - `space` padding (compatibility mode): ASCII space on the MSB side.
- Select via `METALFPGA_STRING_PAD=zero|space`.
- Default: `zero` (matches IEEE/most tools).

---

### Test: test_05_04_02_1
**Section:** IEEE Std 1364-2005, Section 5.4.2 - Expression bit-length problem  
**Topic:** Loss of significant bits in expressions

**Description:**
During expression evaluation, interim results take the size of the largest operand. Adding zero forces proper width.

**Test Code:**
```verilog
module test;
  reg [15:0] a, b, answer;

  initial begin
    // Potentially problematic (may lose carry bit):
    // answer = (a + b) >> 1;
    
    // Correct (forces 17-bit intermediate):
    answer = (a + b + 0) >> 1;
  end
endmodule
```

**Decision:**
- Follow IEEE 1364-2005 expression sizing rules.
- No implicit promotion beyond the standard (e.g., no automatic extra carry bit).
- No warning by default; expression width is deterministic per IEEE.

---

## 5. Procedural Assignments

### Test: test_06_02_01_2
**Section:** IEEE Std 1364-2005, Section 6.2.1 - Variable declaration assignment  
**Topic:** Array initialization in declaration

**Description:**
Initializing entire arrays in declaration is not legal.

**Test Code:**
```verilog
module test;
  // Illegal:
  // reg [3:0] array [3:0] = 0;
endmodule
```

**Decision:**
- Reject array initialization in declarations.

---

## 6. Gate and Switch Level Modeling

### Test: test_07_01_05_1
**Section:** IEEE Std 1364-2005, Section 7.1.5 - Range specification  
**Topic:** Multiple range specifications for same identifier

**Description:**
Cannot use the same identifier with different ranges in multiple instance arrays.

**Test Code:**
```verilog
module test(in1, in2, out1);
  input in1, in2;
  output out1;

  // Illegal - using "t_nand" twice with different ranges:
  // nand #2 t_nand[0:3](out1, in1, in2), t_nand[4:7](out1, in1, in2);

  // Legal alternatives:
  nand #2 t_nand[0:7](out1, in1, in2);
  nand #2 x_nand[0:3](out1, in1, in2), y_nand[4:7](out1, in1, in2);
endmodule
```

**Decision:**
- Reject duplicate gate array identifiers with different ranges.

---

## 7. Hierarchical Structures

### Test: test_12_02_01_1
**Section:** IEEE Std 1364-2005, Section 12.2.1 - defparam statement  
**Topic:** defparam in generate blocks

**Description:**
A defparam in a generate block cannot target a parameter in another instance of the same generate block.

**Test Code:**
```verilog
module flop(input in, in1, output out1);
endmodule

module test(in, in1, out1);
  input [7:0] in, in1;
  output [7:0] out1;
  genvar i;

  generate
    for (i = 0; i < 8; i = i + 1) begin : somename
      flop my_flop(in[i], in1[i], out1[i]);
      // Illegal:
      // defparam somename[i+1].my_flop.xyz = i;
    end
  endgenerate
endmodule
```

**Decision:**
- Reject defparam targeting other generate instances.

---

### Test: test_12_02_02_2_2
**Section:** IEEE Std 1364-2005, Section 12.2.2.2 - Parameter value assignment  
**Topic:** Mixing positional and named parameters

**Description:**
Cannot mix positional and named parameter assignments in the same module instantiation.

**Test Code:**
```verilog
module tb3;
  wire [9:0] out_a, out_d;
  wire [4:0] out_b, out_c;
  reg  [9:0] in_a, in_d;
  reg  [4:0] in_b, in_c;
  reg        clk;

  // Legal:
  vdff #(10, 15)     mod_a(.out(out_a), .in(in_a), .clk(clk));
  vdff               mod_b(.out(out_b), .in(in_b), .clk(clk));
  vdff #(.delay(12)) mod_c(.out(out_c), .in(in_c), .clk(clk));

  // Illegal - mixing positional and named:
  // vdff #(10, .delay(15)) mod_a(.out(out_a), .in(in_a), .clk(clk));
endmodule
```

**Decision:**
- Reject mixed positional + named parameter overrides.

---

### Test: test_12_03_03_1
**Section:** IEEE Std 1364-2005, Section 12.3.3 - Port declarations  
**Topic:** Multiple port declarations

**Description:**
Once a port is declared, it cannot be declared again.

**Test Code:**
```verilog
module test(aport);
  input aport; // First declaration - okay.
  
  // Illegal:
  // input aport;  // Error - multiple declaration
  // output aport; // Error - conflicting direction
endmodule
```

**Decision:**
- Reject duplicate port declarations.

---

### Test: test_12_03_06_3
**Section:** IEEE Std 1364-2005, Section 12.3.6 - Port connections by name  
**Topic:** Multiple port connections

**Description:**
Cannot connect the same port multiple times.

**Test Code:**
```verilog
module test;
  // Illegal - connecting ports multiple times:
  // a ia (.i (a), .i (b),  // illegal connection of input port twice
  //       .o (c), .o (d),  // illegal connection of output port twice
  //       .e (e), .e (f)); // illegal connection of inout port twice
endmodule
```

**Decision:**
- Reject multiple connections to the same port.

---

### Test: test_12_04_01_1
**Section:** IEEE Std 1364-2005, Section 12.4.1 - Loop generate constructs  
**Topic:** Generate loop variable scope

**Description:**
Cannot use the same genvar for nested generate loops, and generate block names cannot conflict with other identifiers.

**Test Code:**
```verilog
module mod_a;
  genvar i;
  
  for (i = 0; i < 5; i = i + 1) begin : a
    // Illegal - reusing "i" for nested loop:
    // for (i = 0; i < 5; i = i + 1) begin : b
    //   ...
    // end
  end
endmodule

module mod_b;
  genvar i;
  reg a;
  
  // Illegal - "a" conflicts with reg "a":
  // for (i = 1; i < 0; i = i + 1) begin : a
  // end
endmodule
```

**Decision:**
- Enforce genvar scoping and generate block name conflicts.

---

## 8. Specify Blocks

### Test: test_14_02_04_3_4
**Section:** IEEE Std 1364-2005, Section 14.2.4.3 - Edge-sensitive state-dependent paths  
**Topic:** Inconsistent destination specifications

**Description:**
State-dependent paths with different conditions must specify destinations consistently (both part-select or both bit-select).

**Test Code:**
```verilog
module test(clk, data, q, reset, cntrl);
  input clk, reset, cntrl;
  output data;
  output [4:0] q;

  specify
    // Illegal - mixing part-select and bit-select:
    // if (reset)
    //   (posedge clk => (q[3:0]:data)) = (10,5);
    // if (!reset)
    //   (posedge clk => (q[0]:data)) = (15,8);
  endspecify
endmodule
```

**Decision:**
- Enforce consistent dest specs in conditional specify paths.

---

### Test: test_14_03_03_2
**Section:** IEEE Std 1364-2005, Section 14.3.3 - Delay selection  
**Topic:** Multiple conditional paths with same condition type

**Description:**
When multiple specify paths are active, delay selection depends on transition type.

**Test Code:**
```verilog
module test(A, MODE, Y);
  input A;
  output integer MODE = 0;
  output Y;

  specify
    if (MODE < 5) (A => Y) = (5, 9);
    if (MODE < 4) (A => Y) = (4, 8);
    if (MODE < 3) (A => Y) = (6, 5);
    if (MODE < 2) (A => Y) = (3, 2);
    if (MODE < 1) (A => Y) = (7, 7);
  endspecify
endmodule
```

**Decision:**
- Delay select: `METALFPGA_SPECIFY_DELAY_SELECT=fast|slow` (default `fast`).

---

### Test: test_14_06_04_2_2
**Section:** IEEE Std 1364-2005, Section 14.6.4.2 - Pulse detection  
**Topic:** showcancelled declaration ordering

**Description:**
showcancelled declaration must precede all module path declarations for that output.

**Test Code:**
```verilog
module test(input a, b, output out);
  specify
    (a => out) = (2,3);
    // Illegal - showcancelled after path declaration:
    // showcancelled out;
    // (b => out) = (3,4);
  endspecify
endmodule
```

**Decision:**
- Enforce showcancelled/noshowcancelled ordering.

---

## 9. Timing Checks

### Test: test_15_03_04_2
**Section:** IEEE Std 1364-2005, Section 15.3.4 - $width timing check  
**Topic:** Optional parameters in timing checks

**Description:**
Timing check parameters must be specified correctly; missing parameters need explicit placeholder.

**Test Code:**
```verilog
module test(clr);
  input clr;
  reg notif;

  specify
    specparam lim = 10, thresh = 0.5;
    
    // Legal Calls:
    $width(negedge clr, lim);
    $width(negedge clr, lim, thresh, notif);
    $width(negedge clr, lim, 0, notif);
    
    // Illegal Calls:
    // $width(negedge clr, lim, , notif);     // missing parameter
    // $width(negedge clr, lim, notif);       // wrong position
  endspecify
endmodule
```

**Decision:**
- Validate timing check params; no skipped args.

---

### Test: test_15_05_01_4
**Section:** IEEE Std 1364-2005, Section 15.5.1 - Notifiers  
**Topic:** Delayed notifiers affecting path delays

**Description:**
Negative setup times can create delayed notifiers that affect when outputs change.

**Test Code:**
```verilog
module test(CLK, D, dCLK, dD, Q);
  input CLK, D;
  inout dCLK, dD;
  output Q;

  specify
    (CLK => Q) = 6;  // Path delay is 6
    $setuphold(posedge CLK, posedge D, -3,  8, , , , dCLK, dD);
    $setuphold(posedge CLK, negedge D, -7, 13, , , , dCLK, dD);
    // Note: -7 setup creates 7 time unit delay for dCLK
  endspecify
endmodule
```

**Decision:**
- Default: follow IEEE; negative setup times are allowed and can delay notifiers.
- Allow override via `METALFPGA_NEGATIVE_SETUP_MODE=allow|clamp|error`:
  - `allow` = IEEE behavior (default).
  - `clamp` = treat negative setup as 0.
  - `error` = reject negative setup values.

---

## 10. SDF Backannotation

### Test: test_16_02_02_2
**Section:** IEEE Std 1364-2005, Section 16.2.2 - SDF timing check mapping  
**Topic:** Conditional timing check annotation

**Description:**
SDF can only annotate timing checks that match its conditions.

**Test Code:**
```verilog
module test(clk, mode, data);
  input clk, mode, data;
  reg ntfr;

  specify
    $setuphold(posedge clk &&&  mode, data, 1, 1, ntfr); // Annotated
    // Not annotated if condition differs:
    // $setuphold(negedge clk &&& !mode, data, 1, 1, ntfr);
  endspecify
endmodule
```

**Decision:**
- SDF conditional match rules.

---

### Test: test_16_02_02_3
**Section:** IEEE Std 1364-2005, Section 16.2.2 - SDF timing check mapping  
**Topic:** Multiple timing checks, no SDF annotation

**Description:**
When no SDF annotations match, all timing checks remain with original values.

**Test Code:**
```verilog
module test(clk, mode, data);
  input clk, mode, data;
  reg ntfr;

  specify
    // Both not annotated if SDF doesn't match:
    // $setuphold(posedge clk &&&  mode, data, 1, 1, ntfr);
    // $setuphold(negedge clk &&& !mode, data, 1, 1, ntfr);
  endspecify
endmodule
```

**Decision:**
- SDF no-match keeps original timing.

---

## 11. System Tasks and Functions

### Test: test_17_09_01_1
**Section:** IEEE Std 1364-2005, Section 17.9.1 - $random function  
**Topic:** $random with modulo operation

**Description:**
Using $random % 60 produces values from -59 to 59 (signed result).

**Test Code:**
```verilog
module test;
  reg [23:0] rnd;
  initial assign rnd = $random % 60;
endmodule
```

**Decision:**
- $random is signed 32-bit; modulo preserves sign.

**Note:** The suite stores the result into `reg [23:0] rnd;` (an unsigned packed vector). If the arithmetic result is negative, the stored bits are the 2's-complement/truncated vector value.

---

### Test: test_17_09_01_2
**Section:** IEEE Std 1364-2005, Section 17.9.1 - $random function  
**Topic:** $random with concatenation

**Description:**
Using {$random} forces unsigned interpretation, giving 0 to 59.

**Test Code:**
```verilog
module test;
  reg [23:0] rnd;
  initial assign rnd = {$random} % 60;
endmodule
```

**Decision:**
- {$random} forces unsigned.

---

## 12. Compiler Directives

### Test: test_19_03_01_2
**Section:** IEEE Std 1364-2005, Section 19.3.1 - `define  
**Topic:** Macro definitions split across strings

**Description:**
Cannot split macro definitions across string literals.

**Test Code:**
```verilog
module test;
  // Illegal - macro split across string:
  // `define first_half "start of string
  // $display (`first_half end of string");
endmodule
```

**Decision:**
- Reject macros that split strings.

---

### Test: test_19_11_00_2
**Section:** IEEE Std 1364-2005, Section 19.11 - `begin_keywords, `end_keywords  
**Topic:** Keyword version control

**Description:**
When using `begin_keywords "1364-2005"`, reserved keywords like "uwire" cannot be used as identifiers.

**Test Code:**
```verilog
`begin_keywords "1364-2005"
module m2;
  // ERROR: "uwire" is a keyword in 1364-2005:
  // wire [63:0] uwire;
endmodule
`end_keywords
```

**Decision:**
- Implement begin_keywords/end_keywords; default 1364-2005.

---

## Summary and Recommendations

### High Priority (Core Language Features)
1. **Unsized constant width** (test_03_05_01_1) - 32-bit default
2. **Expression width handling** (test_05_04_02_1) - IEEE sizing rules (no implicit promotion)
3. **Generate block scoping** (test_12_04_01_1) - Must enforce
4. **Parameter assignment styles** (test_12_02_02_2_2) - Must reject mixing

### Medium Priority (Error Detection)
5. **Zero replication** (test_05_01_14_3) - Detect and reject
6. **X/Z replication counts** (test_05_01_14_2) - Should already work
7. **Duplicate port declarations** (test_12_03_03_1) - Should already work
8. **Multiple port connections** (test_12_03_06_3) - Detect in elaboration

### Low Priority (Advanced Features)
9. **SDF annotation matching** (test_16_02_02_2, test_16_02_02_3) - If SDF supported
10. **Specify block details** (test_14_02_04_3_4, test_14_03_03_2, test_14_06_04_2_2)
11. **Timing check validation** (test_15_03_04_2, test_15_05_01_4)
12. **Keyword versioning** (test_19_11_00_2)

### Documentation Needed
13. **String handling** (test_05_02_03_2_1) - `METALFPGA_STRING_PAD=zero|space` (default `zero`)
14. **$random behavior** (test_17_09_01_1, test_17_09_01_2) - Document sign handling
15. **Real number formats** (test_03_05_02_1) - Document accepted formats
16. **Strict mode** (test_04_10_03_2) - `--strict-1364`
17. **Specify delay selection** (test_14_03_03_2) - `METALFPGA_SPECIFY_DELAY_SELECT=fast|slow`
18. **Negative setup handling** (test_15_05_01_4) - `METALFPGA_NEGATIVE_SETUP_MODE=allow|clamp|error`

### Next Steps
1. Review each case and make implementation decisions (Done)
2. Update metalfpga to handle cases consistently (Pending)
3. Add tests to verify chosen behavior (Derive from VARY tests in golden suite)
4. Document implementation-defined choices in user documentation (Pending)
