# metalfpga

A Verilog-to-Metal (MSL) compiler for GPU-based hardware simulation. Parses and lowers a large, practical subset of Verilog (spanning RTL and common testbench semantics) into Metal Shading Language compute kernels, enabling fast hardware prototyping and validation on Apple GPUs.

**Current Status**: v0.5+ — Compiler/codegen coverage includes **IEEE 754 real number arithmetic**, User-Defined Primitives (UDPs), match-3 operators (`===`, `!==`, `==?`, `!=?`), power operator (`**`), and procedural part-select assignment. Extensive support for generate/loops, 4-state logic, signed arithmetic, system tasks, timing controls, and switch-level primitives. **281 total test files** with **93% pass rate** on the default test suite. **GPU runtime execution and validation are the next milestone.**

## What is this?

metalfpga is a "GPGA" (GPU-based FPGA) compiler that:
- Parses Verilog RTL and testbench constructs
- Flattens module hierarchy through elaboration
- Generates Metal compute shaders for GPU execution
- Emits host-side runtime scaffolding

**Current phase:** MSL codegen is in place; GPU runtime dispatch/validation is in development.

This allows FPGA designers to prototype and validate hardware designs on GPUs before synthesis to actual hardware, leveraging massive parallelism for fast simulation.

**Note**: The emitted Metal Shading Language (MSL) code is verbose and generally correct, but has not been thoroughly validated through kernel execution. The codegen produces structurally sound MSL that implements the intended semantics, though bugs may surface during actual GPU dispatch. Runtime validation is the next development phase.

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

## Output Artifacts

The compiler produces:

- **Metal shaders** (`--emit-msl`): Emits `.metal` source containing compute kernels for combinational logic, sequential blocks, and scheduler infrastructure (layout depends on design/top and codegen mode)
- **Host runtime stub** (`--emit-host`): `.mm` file with buffer layout, service records, and Metal dispatch scaffolding (requires manual integration)
- **Flattened netlist** (`--dump-flat`): Human-readable elaborated design showing hierarchy flattening, signal widths, drivers, and lowered constructs

## Supported Verilog Features

### ✅ Implemented (frontend/elaboration/codegen)
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
  - Arithmetic: `+`, `-`, `*`, `/`, `%`, `**` (power/exponentiation)
  - Bitwise: `&`, `|`, `^`, `~`
  - Reduction: `&`, `|`, `^`, `~&`, `~|`, `~^`
  - Logical: `&&`, `||`, `!`
  - Shifts: `<<`, `>>`, `<<<`, `>>>`
  - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
  - Case equality: `===`, `!==` (exact bit-for-bit match including X/Z, requires `--4state`)
  - Wildcard match: `==?`, `!=?` (pattern matching with X/Z as don't-care, requires `--4state`)
  - Ternary: `? :`
- Signed arithmetic with `signed` keyword
- Arithmetic right shift (`>>>`) with sign extension
- Type casting: `$signed(...)`, `$unsigned(...)`
- Bit/part selects: `signal[3]`, `signal[7:0]`, `signal[i +: 4]`, `signal[i -: 4]`
- Part-select assignment (procedural): `data[7:0] = value`, `data[idx +: 4] = value`, `data[idx -: 4] = value`
- Concatenation: `{a, b, c}`
- Replication: `{4{1'b0}}`
- Memory arrays: `reg [7:0] mem [0:255]` (multi-dimensional supported)
- Parameters and localparams with expressions (including port widths)
- Width mismatches (automatic extension/truncation)
- 4-state logic (0/1/X/Z) with `--4state` flag:
  - X/Z literals (`8'bz`, `4'b10zx`, etc.)
  - X/Z propagation in operations
  - See [4STATE.md](docs/4STATE.md) for details
- System tasks:
  - Output: `$display`, `$write`, `$strobe` - Console output with format specifiers (%h, %d, %b, %o, %t, %s)
  - Monitoring: `$monitor` - Continuous value watching
  - Control: `$finish`, `$stop` - Simulation control
  - Time: `$time`, `$stime`, `$realtime` - Time retrieval
  - Formatting: `$timeformat`, `$printtimescale` - Time formatting
  - Memory I/O: `$readmemh`, `$readmemb`, `$writememh`, `$writememb` - Memory initialization and dump
  - Waveform: `$dumpfile`, `$dumpvars`, `$dumpall`, `$dumpon`, `$dumpoff` - VCD waveform dumping (infrastructure)
  - String: `$sformat` - String formatting
  - Math/Type: `$signed`, `$unsigned`, `$clog2`, `$bits` - Type casting and utility functions
- Tasks (procedural `task` blocks with inputs/outputs)
- `time` data type and time system functions
- Named events (`event` keyword and `->` trigger)
- Switch-level modeling:
  - Transmission gates: `tran`, `tranif0`, `tranif1`, `rtran`, `rtranif0`, `rtranif1`
  - MOS switches: `nmos`, `pmos`, `rnmos`, `rpmos`, `cmos`, `rcmos`
  - Drive strengths: `supply0`, `supply1`, `strong0`, `strong1`, `pull0`, `pull1`, `weak0`, `weak1`, `highz0`, `highz1`
  - Net types: `wire`, `wand`, `wor`, `tri`, `triand`, `trior`, `tri0`, `tri1`, `supply0`, `supply1`
- Timing delays (`#` delay syntax)
- `timescale` directive
- User-Defined Primitives (UDPs):
  - Combinational UDPs (truth tables)
  - Sequential UDPs (state machines with current/next state)
  - Edge-sensitive UDPs (posedge/negedge detection)
  - Level-sensitive UDPs (transparent latches)
- Real number arithmetic (IEEE 754 double-precision):
  - `real` data type for floating-point variables
  - Real literals: `3.14`, `1.5e-10`, `.5`, `5.`
  - Real arrays: `real voltage[0:15];`
  - Real parameters: `parameter real PI = 3.14159;`
  - Arithmetic operators: `+`, `-`, `*`, `/`, `**` (power)
  - Comparison operators: `<`, `>`, `<=`, `>=`, `==`, `!=`
  - Type conversion: `$itor()` (int→real), `$rtoi()` (real→int with truncation)
  - Bit conversion: `$realtobits()`, `$bitstoreal()` (for real storage in 4-state)
  - Mixed integer/real arithmetic with automatic promotion
  - Real constant expressions in parameters and generate blocks

### 🧪 Implemented but awaiting GPU runtime verification

These features are fully implemented in the compiler pipeline + MSL emission, but have not yet been validated by executing GPU kernels:

- Event scheduling behavior (fork/join, wait, delays) under dispatch loop
- System tasks requiring host services: `$readmemh`/`$readmemb` file I/O, `$display` formatting, VCD waveform dumping (`$dumpvars`)
- Timing controls (`#delay`) in real-time execution
- Switch-level resolution correctness under GPU scheduling / write ordering
- Non-blocking assignment (`<=`) scheduling semantics

**Status:** MSL emission is structurally sound and follows intended semantics. Bugs may surface during actual Metal dispatch and are expected to be addressed during runtime validation phase.

### ❌ Not Yet Implemented
**High Priority**:
- Runtime kernel execution and validation (MSL code generation complete, GPU dispatch pending)
- Event scheduling validation (infrastructure complete, needs runtime testing)
- Full sensitivity list support beyond `@*` and `@(posedge/negedge clk)`

**Low Priority**:
- SystemVerilog constructs

**Non-goals** (for now):
- Full SystemVerilog compliance (classes, interfaces, packages, assertions)
- Full PLI/VPI compatibility
- Cycle-accurate timing matching commercial simulators

## Test Suite

**281 total test files** across the test suite:
- **54 files** in `verilog/` (default test suite, runs in ~30 seconds)
- **227 files** in `verilog/pass/` (extended test suite, requires `--full` flag)
- **18 files** in `verilog/systemverilog/` (SystemVerilog features, expected to fail)

**Pass rate: 93%** (54/58 passing in default suite)

The 4 expected failures are SystemVerilog-specific features (not Verilog-2005):
- `test_streaming_operator.v`, `test_struct.v`, `test_enum.v`, `test_interface.v`
- Plus 10 more SystemVerilog features in `verilog/systemverilog/`

These tests validate parsing, elaboration, codegen output quality, and semantic lowering decisions. Full GPU runtime validation is in progress.

**Test coverage includes**:
- **User-Defined Primitives (UDPs)**: Combinational, sequential, edge-sensitive primitives
- **Real number arithmetic**: All IEEE 754 operations, conversions, edge cases (infinity, NaN, denormals)
- **Arithmetic and logic operations**: All operators including power (`**`)
- **Match-3 operators**: Case equality `===`/`!==`, wildcard match `==?`/`!=?`
- **Part-select assignment**: Fixed `[7:0]` and indexed `[idx +: 4]` ranges
- **Reduction operators**: All 6 variants (&, |, ^, ~&, ~|, ~^) in all contexts
- **Signed arithmetic**: Overflow, underflow, division by zero, sign extension
- **Generate blocks**: Nested for/if-generate, genvar arithmetic
- **4-state logic**: X/Z propagation, casex/casez, match operators
- **System tasks**: $display, $monitor, $time, $finish, $readmemh/b, $dumpvars, etc.
- **Switch-level modeling**: Transmission gates, MOS switches, drive strengths
- **Memory operations**: Multi-dimensional arrays, read/write
- **Timing controls**: Delays, events, fork/join

### Running Tests

**Default test suite** (54 files, ~30 seconds):
```sh
./test_runner.sh
```

**Full test suite** (281 files, ~3 minutes):
```sh
./test_runner.sh --full
```

**With 4-state mode**:
```sh
./test_runner.sh --4state
```

**Single file**:
```sh
./build/metalfpga_cli verilog/test_example.v
./build/metalfpga_cli verilog/test_example.v --4state  # With X/Z support
```

**Artifacts**: Test results are saved to `artifacts/<RUN_ID>/` with:
- Generated MSL files in `msl/`
- Test logs in `test_results/`
- Flattened netlists for each test

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
- [4-State Logic Plan](docs/4STATE.md) - X/Z support design document
- [Bit Packing Strategy](docs/bit_packing_strategy.md) - GPU memory optimization techniques
- [Verilog Reference](docs/VERILOG_REFERENCE.md) - Language reference
- [Async Debugging](docs/ASYNC_DEBUGGING.md) - Debugging asynchronous circuits

## Project Structure

```
src/
  frontend/       # Verilog parser and AST
  core/           # Elaboration and flattening
  ir/             # Intermediate representation
  codegen/        # MSL and host code generation
  msl/            # Metal Shading Language backend
  runtime/        # Metal runtime wrapper
  utils/          # Diagnostics and utilities
verilog/
  test_*.v        # Main test suite (54 files, default run)
  pass/           # Extended test suite (227 files, --full flag)
  systemverilog/  # SystemVerilog tests (18 files, expected to fail)
docs/
  gpga/           # Core documentation
  diff/           # REV documents (commit changelogs)
  4STATE.md       # 4-state logic implementation
  ANALOG.md       # Analog/mixed-signal support
  GPGA_KEYWORDS.md  # Verilog keyword reference
  VERILOG_REFERENCE.md     # Language reference
  ASYNC_DEBUGGING.md       # Async circuit debugging
  bit_packing_strategy.md  # GPU memory optimization
artifacts/        # Test run outputs (generated, gitignored)
  <RUN_ID>/
    msl/          # Generated Metal shaders
    test_results/ # Test logs and status
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

For commercial licensing inquiries, see [LICENSE.COMMERCIAL](LICENSE.COMMERCIAL) for contact information and terms.
