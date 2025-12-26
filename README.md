# metalfpga

A Verilog-to-Metal (MSL) compiler for GPU-based hardware simulation. Compiles a subset of Verilog HDL into Metal Shading Language compute kernels, enabling fast hardware prototyping and validation on Apple GPUs.

**Current Status**: v0.3 - Full generate/loop coverage, signed arithmetic, and 4-state logic support. 114 passing test cases in `verilog/pass/`.

## What is this?

metalfpga is a "GPGA" (GPU-based FPGA) compiler that:
- Parses synthesizable Verilog RTL
- Flattens module hierarchy through elaboration
- Generates Metal compute shaders for GPU execution
- Emits host-side stubs (runtime wiring is still TODO)

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

# Generate host runtime stub
./build/metalfpga_cli path/to/design.v --emit-host output.mm

# Specify top module (if multiple exist)
./build/metalfpga_cli path/to/design.v --top module_name

# Enable 4-state logic (X/Z support)
./build/metalfpga_cli path/to/design.v --4state

# Multiple files (module references across files)
./build/metalfpga_cli path/to/file_a.v path/to/file_b.v --top top

# Auto-discover modules under the input file's directory tree
./build/metalfpga_cli path/to/top.v --auto --top top
```

## Supported Verilog Features

### ✅ Working
- Module declarations with hierarchy and parameters
- Port declarations: `input`, `output`, `inout`
- Port connections (named and positional)
- Multi-file input (pass multiple `.v` files on the CLI)
- Auto-discover module files under the input directory tree (`--auto`)
- Wire/reg declarations with bit-widths (including unpacked arrays)
- Continuous assignments (`assign`)
- Always blocks:
  - Combinational (`always @(*)`)
  - Sequential (`always @(posedge clk)`, `always @(negedge clk)`)
- Blocking (`=`) and non-blocking (`<=`) assignments
- If/else statements
- Case statements:
  - `case` - Standard case matching
  - `casex` - Case with X don't-care matching (requires `--4state`)
  - `casez` - Case with Z/X don't-care matching (requires `--4state`)
- Generate blocks with `genvar` and for/if-generate
- For/while/repeat loops (constant bounds, unrolled during elaboration)
- Initial blocks
- Functions (inputs + single return assignment, inlined during elaboration)
- Operators:
  - Arithmetic: `+`, `-`, `*`, `/`, `%`
  - Bitwise: `&`, `|`, `^`, `~`
  - Reduction: `&`, `|`, `^`, `~&`, `~|`, `~^`
  - Logical: `&&`, `||`, `!`
  - Shifts: `<<`, `>>`, `<<<`, `>>>`
  - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
  - Ternary: `? :`
- Signed arithmetic with `signed` keyword
- Arithmetic right shift (`>>>`) with sign extension
- Type casting: `$signed(...)`, `$unsigned(...)`
- Bit/part selects: `signal[3]`, `signal[7:0]`, `signal[i +: 4]`, `signal[i -: 4]`
- Concatenation: `{a, b, c}`
- Replication: `{4{1'b0}}`
- Memory arrays: `reg [7:0] mem [0:255]` (multi-dimensional supported)
- Parameters and localparams with expressions (including port widths)
- Width mismatches (automatic extension/truncation)
- 4-state logic (0/1/X/Z) with `--4state` flag:
  - X/Z literals (`8'bz`, `4'b10zx`, etc.)
  - X/Z propagation in operations
  - See [4STATE.md](4STATE.md) for details

### ❌ Not Yet Implemented
**High Priority**:
- System tasks (`$display`, `$monitor`, `$finish`, etc.)
- Tasks (procedural `task` blocks)

**Medium Priority**:
- General timing controls (`#` delays)
- Sensitivity lists beyond `@*` and `@(posedge/negedge clk)`

**Low Priority**:
- SystemVerilog constructs

## Test Suite

**114 passing test cases** in `verilog/pass/`:
- Arithmetic and logic operations
- Reduction operators (all 6 variants: &, |, ^, ~&, ~|, ~^)
  - Comprehensive coverage: nested, slices, wide buses (64/128/256-bit)
  - In combinational and sequential contexts
  - With ternary and case statements
- Signed arithmetic and comparisons
  - Edge cases: overflow, underflow, division by zero
  - Arithmetic right shift with sign extension
  - Mixed signed/unsigned operations
  - Unary operators on signed values
- Module instantiation and hierarchy
- Multi-file module references
- Generate blocks (nested for/if-generate, genvar arithmetic)
- Sequential and combinational logic
- Memory read/write operations
- Case statements (case, casex, casez)
- 4-state logic with X/Z values
- Parentheses and expression grouping
- Width extension and truncation

**In-development test coverage** in `verilog/`:
- Additional tests and edge cases beyond the passing suite

Run all passing tests:
```sh
for f in verilog/pass/*.v; do
  ./build/metalfpga_cli "$f"
done
```

Run all tests (including in-development):
```sh
for f in verilog/test_*.v; do
  ./build/metalfpga_cli "$f"
done
```

## Error Detection

The compiler detects and reports:
- Combinational loops
- Multiple drivers on regs or mixed always/assign conflicts (wire multi-drive is resolved in 4-state)
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

MetalFPGA is dual-licensed:

### Open Source (AGPL-3.0)

Free for open-source projects, academic research, and personal use under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE) for details.

**Key requirements under AGPL:**
- You must share your source code if you distribute modified versions
- If you run MetalFPGA as a network service (SaaS), you must make your modifications available
- Any derivative works must also be licensed under AGPL-3.0

### Commercial License

For proprietary/closed-source integration, commercial licenses are available. Commercial licensing allows you to:
- Use MetalFPGA in closed-source products
- Deploy as SaaS without releasing modifications
- Integrate into proprietary toolchains
- Redistribute without copyleft restrictions

**For commercial licensing inquiries:** support@tomsdata.no

See [LICENSE.COMMERCIAL](LICENSE.COMMERCIAL) for terms.
