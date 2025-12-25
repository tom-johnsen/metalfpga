# Verilog Language Reference Documentation

Compiled: 2025-12-25

## Overview

This document contains reference information about the Verilog Hardware Description Language (HDL), compiled from official IEEE specifications and online resources.

## 4-State Logic System

Verilog employs a **4-value logic system** (unlike typical programming languages with 2-state logic):

- **0**: Logical zero/false
- **1**: Logical one/true
- **X**: Unknown value (uninitialized or conflicting)
- **Z**: High impedance state (tri-state, disconnected)

This 4-state system is fundamental to hardware modeling and represents real circuit behavior including unknown states during simulation and high-impedance tri-state buffers.

## Primary Data Types

### Wire (Net Type)
- Represents physical connections between hardware elements
- "Nets are continuously driven by combinational logic circuits. It means it cannot store any values."
- Default value: `Z` (high impedance)
- Used with continuous assignment (`assign` statement)
- Example: `wire and_gate_output;`

### Reg (Register Type)
- Represents storage elements (NOT necessarily hardware flip-flops)
- Can only be assigned in procedural blocks (`always`, `initial`)
- Retains values between assignments
- Example: `reg [7:0] address_bus;`

**Important**: Despite the name "reg", this does NOT always synthesize to a register/flip-flop. It's just a variable type that can hold values in procedural code.

## Number Representation

Syntax: `<size>'<radix><value>`

### Radix Options
- **Binary**: `b` or `B` (e.g., `8'b10101010`)
- **Octal**: `o` or `O` (e.g., `8'o252`)
- **Decimal**: `d` or `D` (e.g., `8'd170`)
- **Hexadecimal**: `h` or `H` (e.g., `8'hAA`)

### Examples
```verilog
1              // Unsized decimal (defaults to 32 bits)
8'hAA          // 8-bit hex: 10101010
6'b10_0011     // 6-bit binary: 100011 (underscore ignored)
'hF            // Unsized hex: 32-bit 0000...00001111
```

### Size Handling
- When `<size>` < `<value>`: leftmost bits truncated
- When `<size>` > `<value>`: leftmost bits filled based on MSB:
  - `0` or `1` filled with `0`
  - `Z` filled with `Z`
  - `X` filled with `X`

Examples:
```verilog
6'hCA      // 001010 (truncated)
6'hA       // 001010 (zero-extended)
16'bZ      // ZZZZZZZZZZZZZZZZ
8'bx       // xxxxxxxx
```

## Operators

### Equality Operators

#### Case Equality (includes X and Z)
- `===` : Equal including X and Z
- `!==` : Not equal including X and Z
- **Always returns 0 or 1** (never X)

```verilog
4'bx001 === 4'bx001  // Returns 1 (X matches X)
4'bx0x1 === 4'bx001  // Returns 0 (X doesn't match 0)
4'bz0x1 === 4'bz0x1  // Returns 1
```

#### Logical Equality (X and Z propagate)
- `==` : Equal (may return X)
- `!=` : Not equal (may return X)
- Returns X if either operand contains X or Z

```verilog
5 == 5         // Returns 1
5 == 10        // Returns 0
4'b10x1 == 4'b1001  // Returns X (contains X)
```

### Concatenation Operator

Syntax: `{signal1, signal2, ...}`

```verilog
{4'b1001, 4'b10x1}  // Result: 100110x1
```

### Replication Operator

Syntax: `{n{value}}`

Replicates a value n times (n must be constant):

```verilog
{4{4'b1001}}           // 1001100110011001
{4{4'b1001, 1'bz}}     // 1001z1001z1001z1001z
{3{a}}                 // Same as {a, a, a}
{b, {3{c, d}}}         // Same as {b, c, d, c, d, c, d}
```

### Conditional (Ternary) Operator

Syntax: `condition ? true_expr : false_expr`

```verilog
assign out = (enable) ? data : 1'bz;  // Tri-state buffer
```

### Arithmetic Operators
- `*` : Multiply
- `/` : Division
- `+` : Add
- `-` : Subtract
- `%` : Modulus

### Logical Operators
- `!` : Logical negation
- `&&` : Logical AND
- `||` : Logical OR

### Bitwise Operators
- `~` : Bitwise NOT
- `&` : Bitwise AND
- `|` : Bitwise OR
- `^` : Bitwise XOR
- `~^` or `^~` : Bitwise XNOR

### Reduction Operators
Apply operation across all bits of single operand:
- `&` : Reduction AND
- `|` : Reduction OR
- `^` : Reduction XOR
- `~&` : Reduction NAND
- `~|` : Reduction NOR
- `~^` or `^~` : Reduction XNOR

### Shift Operators
- `<<` : Logical left shift
- `>>` : Logical right shift
- `<<<` : Arithmetic left shift
- `>>>` : Arithmetic right shift

### Relational Operators
- `<` : Less than
- `>` : Greater than
- `<=` : Less than or equal
- `>=` : Greater than or equal

## Assignment Types

### Blocking Assignment (`=`)
- Executes sequentially
- Blocks execution of subsequent statements
- Used for combinational logic
- Example: `a = b + c;`

### Non-Blocking Assignment (`<=`)
- Executes in parallel
- RHS evaluated immediately, LHS updated at end of time step
- Used for sequential logic (flip-flops)
- Example: `q <= d;`

**CRITICAL RULE**: Never mix blocking and non-blocking assignments in the same `always` block!

| Aspect | Blocking (=) | Non-Blocking (<=) |
|--------|--------------|-------------------|
| Execution | Sequential | Parallel |
| Timing | Immediate update | End of time step |
| Use Case | Combinational logic | Sequential logic |
| Mix in same block? | **NO** | **NO** |

## Module Structure

### Basic Module Syntax

```verilog
module module_name (
    port_list
);
    // Port declarations
    input wire clk;
    input wire [7:0] data_in;
    output reg [7:0] data_out;

    // Internal logic

endmodule
```

### Verilog-2001 Port Declaration

```verilog
module arbiter (
    input  wire       clock,
    input  wire       reset,
    input  wire       req_0,
    input  wire       req_1,
    output reg        gnt_0,
    output reg        gnt_1
);
    // Module logic
endmodule
```

### Port Directions
- `input` : Input port
- `output` : Output port
- `inout` : Bidirectional port

### Vector Ports

```verilog
input  [7:0] address;      // 8-bit input (little-endian)
output [0:7] data;         // 8-bit output (big-endian)
inout  [15:0] bus;         // 16-bit bidirectional
```

## Procedural Blocks

### Initial Block
- Executes once at time 0
- Used for testbenches and initialization
```verilog
initial begin
    clk = 0;
    reset = 1;
    #10 reset = 0;
end
```

### Always Block
- Executes based on sensitivity list
- Used for combinational and sequential logic

#### Combinational Logic
```verilog
always @(*) begin
    // Use blocking assignments
    out = a & b;
end
```

#### Sequential Logic
```verilog
always @(posedge clk or posedge reset) begin
    if (reset)
        q <= 0;
    else
        q <= d;  // Use non-blocking assignments
end
```

## Escaped Identifiers

Verilog allows any ASCII character in identifiers using escape sequences:
- Begin with backslash `\`
- Terminated by whitespace
- Useful for names starting with numbers or containing special characters

```verilog
module \1dff (
    output q,
    output \q~,      // Special character in name
    input  d,
    input  cl$k,     // $ allowed without escape
    input  \reset*   // * requires escape
);
```

## IEEE Standards

### Verilog Standards Evolution
- **IEEE 1364-1995 (Verilog-95)**: Original standard
- **IEEE 1364-2001 (Verilog-2001)**: Added enhanced features, ANSI-C style ports
- **IEEE 1364-2005**: Minor corrections and clarifications
- **IEEE 1800-2009**: Merged with SystemVerilog

### SystemVerilog Standards
- **IEEE 1800-2005**: First SystemVerilog standard
- **IEEE 1800-2009**: Merged Verilog and SystemVerilog
- **IEEE 1800-2012**: Updates and enhancements
- **IEEE 1800-2017**: Major revision (available free via IEEE GET Program)
- **IEEE 1800-2023**: Most recent standard (December 2023)

## Key Language Features by Version

### Verilog-2001 Enhancements
- ANSI-C style port declarations
- Generate blocks
- Multi-dimensional arrays
- Comma-separated sensitivity lists
- `@*` for implicit sensitivity
- Signed arithmetic
- Power operator `**`
- Localparam keyword
- Indexed part selects

### SystemVerilog Additions
- 2-state data types (`bit`, `byte`, `int`, `longint`)
- Enhanced procedural blocks (`always_comb`, `always_ff`, `always_latch`)
- Structures and unions
- Object-oriented programming (classes)
- Interfaces
- Assertions (SVA)
- Coverage constructs
- Constrained random verification
- Direct Programming Interface (DPI)

## Common Synthesis Guidelines

1. **Use 2-state types for synthesis** (avoid X, Z when possible for actual hardware)
2. **Infer registers with non-blocking assignments** in clocked always blocks
3. **Combinational logic** should use blocking assignments or continuous assign
4. **Avoid latches** unless intentional (complete if-else or case with default)
5. **Reset logic**: Decide async vs sync reset consistently
6. **Avoid delays in synthesizable code** (`#10` is simulation-only)

## Resources

### Official Standards
- IEEE Std 1364-2005: Verilog HDL Standard
- IEEE Std 1800-2017: SystemVerilog Standard (free via GET Program)
- IEEE Std 1800-2023: Latest SystemVerilog

### Online Tutorials
- ASIC-World Verilog Tutorial: https://www.asic-world.com/verilog/veritut.html
- ChipVerify Verilog Tutorial: https://www.chipverify.com/tutorials/verilog
- Verilog in One Day: Quick introduction for beginners

### Tools
- Icarus Verilog: Open-source simulator
- Verilator: Fast open-source simulator
- Yosys: Open-source synthesis tool

## Sources

This reference was compiled from the following sources:

- [IEEE Standard 1364-2005 for Verilog HDL](https://ieeexplore.ieee.org/document/1620780/)
- [IEEE Standard 1800-2017 for SystemVerilog](https://ieeexplore.ieee.org/document/8299595)
- [IEEE Standard 1800-2023 for SystemVerilog](https://ieeexplore.ieee.org/document/10458102/)
- [ChipVerify Verilog Tutorial](https://www.chipverify.com/tutorials/verilog)
- [ASIC-World Verilog Tutorial](https://www.asic-world.com/verilog/veritut.html)
- [GeeksforGeeks Verilog Guide](https://www.geeksforgeeks.org/electronics-engineering/getting-started-with-verilog/)
- [Icarus Verilog Documentation](https://steveicarus.github.io/iverilog/)
- [Verilog Wikipedia Article](https://en.wikipedia.org/wiki/Verilog)
