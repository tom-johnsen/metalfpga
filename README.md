# metalfpga

A Verilog-to-Metal (MSL) compiler for GPU-based hardware simulation. Compiles a subset of Verilog HDL into Metal Shading Language compute kernels, enabling fast hardware prototyping and validation on Apple GPUs.

**Current Status**: v0.1+ - Core pipeline working with 35+ passing test cases

## What is this?

metalfpga is a "GPGA" (GPU-based FPGA) compiler that:
- Parses synthesizable Verilog RTL
- Flattens module hierarchy through elaboration
- Generates Metal compute shaders for GPU execution
- Provides host-side runtime for simulation

This allows FPGA designers to prototype and validate hardware designs on GPUs before synthesis to actual hardware, leveraging massive parallelism for fast simulation.

## Quick Start

### Build
```sh
cmake -S . -B build
cmake --build build
```

### Run
```sh
# Check syntax and elaborate design
./build/metalfpga_cli path/to/design.v

# Dump flattened netlist
./build/metalfpga_cli path/to/design.v --dump-flat

# Generate Metal shader code
./build/metalfpga_cli path/to/design.v --emit-msl output.metal

# Generate host runtime code
./build/metalfpga_cli path/to/design.v --emit-host output.mm

# Specify top module (if multiple exist)
./build/metalfpga_cli path/to/design.v --top module_name
```

## Supported Verilog Features

### ✅ Working
- Module declarations with hierarchy and parameters
- Port connections (named and positional)
- Wire/reg declarations with bit-widths
- Continuous assignments (`assign`)
- Always blocks:
  - Combinational (`always @(*)`)
  - Sequential (`always @(posedge clk)`, `always @(negedge clk)`)
- Blocking (`=`) and non-blocking (`<=`) assignments
- If/else statements
- Operators:
  - Arithmetic: `+`, `-`, `*`, `/`, `%`
  - Bitwise: `&`, `|`, `^`, `~`
  - Logical: `&&`, `||`, `!`
  - Shifts: `<<`, `>>`, `<<<`, `>>>`
  - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
  - Ternary: `? :`
- Bit/part selects: `signal[3]`, `signal[7:0]`
- Concatenation: `{a, b, c}`
- Replication: `{4{1'b0}}`
- Memory arrays: `reg [7:0] mem [0:255]`
- Parameters and localparams with expressions
- Width mismatches (automatic extension/truncation)

### 🚧 In Development
- `case`/`casex`/`casez` statements (in progress)
- 4-state logic (0/1/X/Z) support (planned - see 4STATE.md)

### ❌ Not Yet Implemented
**High Priority**:
- Reduction operators (`&data`, `|data`, `^data`)
- Signed arithmetic (`signed` keyword)

**Medium Priority**:
- `inout` ports and high-Z literals (`8'bz`)
- Multi-dimensional arrays
- `for`/`while`/`repeat` loops
- `function`/`task` declarations

**Low Priority**:
- `initial` blocks
- `generate` blocks with `genvar`
- SystemVerilog constructs

## Test Suite

**35+ passing test cases** in `verilog/pass/`:
- Arithmetic and logic operations
- Module instantiation and hierarchy
- Sequential and combinational logic
- Memory operations
- Edge case handling

Run all tests:
```sh
for f in verilog/pass/*.v; do
  ./build/metalfpga_cli "$f"
done
```

## Error Detection

The compiler detects and reports:
- Combinational loops
- Multiple drivers on signals
- Recursive module instantiation
- Undeclared signals
- Width mismatches (with warnings)

## Documentation

- [Project Overview](docs/gpga/README.md) - Architecture and goals
- [Verilog Coverage](docs/gpga/verilog_words.md) - Keyword implementation status
- [IR Invariants](docs/gpga/ir_invariants.md) - Flattened netlist guarantees
- [Roadmap](docs/gpga/roadmap.md) - Development milestones
- [4-State Logic Plan](4STATE.md) - X/Z support design document

## Project Structure

```
src/
  frontend/       # Verilog parser and AST
  core/           # Elaboration and flattening
  codegen/        # MSL and host code generation
  runtime/        # Metal runtime wrapper
  utils/          # Diagnostics and utilities
verilog/
  pass/           # Passing test cases
  test_*.v        # Additional test coverage
docs/gpga/        # Documentation
```

## Contributing

This is an early-stage research prototype. The architecture is modular and designed for incremental feature additions.

## License

See repository for license details.
