# metalfpga

A Verilog-to-Metal (MSL) compiler for GPU-based hardware simulation. Parses and lowers a large, practical subset of Verilog (spanning RTL and common testbench semantics) into Metal Shading Language compute kernels, enabling fast hardware prototyping and validation on Apple GPUs.

**Current Status**: v0.6+ — **Verilog frontend complete**: parsing, elaboration, and MSL codegen for ~95% of Verilog-2005. Includes **IEEE 754 real number arithmetic**, User-Defined Primitives (UDPs), match-3 operators (`===`, `!==`, `==?`, `!=?`), power operator (`**`), multi-dimensional arrays, and procedural part-select assignment. Extensive support for generate blocks, 4-state logic, signed arithmetic, system tasks, timing controls, and switch-level primitives. **365 total test files** validate the compiler pipeline. **Next phase: Host-side runtime and GPU kernel execution (v0.7 → v1.0).**

## What is this?

metalfpga is a "GPGA" (GPU-based FPGA) compiler that:
- Parses Verilog RTL and testbench constructs
- Flattens module hierarchy through elaboration
- Generates Metal compute shaders for GPU execution
- Emits host-side runtime scaffolding

**Current phase:** ✅ **Verilog frontend complete** (parsing → elaboration → MSL codegen). Next: GPU runtime dispatch and validation.

This allows FPGA designers to prototype and validate hardware designs on GPUs before synthesis to actual hardware, leveraging massive parallelism for fast simulation.

**Note**: The compiler successfully parses, elaborates, and generates MSL code for the full Verilog-2005 language. The emitted MSL is structurally sound and implements intended semantics, though runtime bugs may surface during GPU execution. The frontend is feature-complete; ongoing work focuses on host-side runtime and kernel execution validation.

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
  - Net types: `wire`, `wand`, `wor`, `tri`, `triand`, `trior`, `tri0`, `tri1`, `trireg`, `supply0`, `supply1`
  - Charge storage: `trireg` nets with capacitance levels (small/medium/large) and charge retention
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

**365 total test files** across the test suite:
- **1 file** in `verilog/` (smoke test for quick validation, runs in ~2 seconds)
- **364 files** in `verilog/pass/` (comprehensive test suite, requires `--full` flag, runs in ~3 minutes)
- **18 files** in `verilog/systemverilog/` (SystemVerilog features, expected to fail)

The test suite validates parsing, elaboration, MSL codegen output quality, and semantic correctness. Tests cover all major Verilog-2005 features including edge cases and advanced semantics.

**Note**: The default test run (`./test_runner.sh`) now executes only a single smoke test for rapid iteration during development. Use `./test_runner.sh --full` for comprehensive regression testing.

**Test coverage includes** (all tests in `verilog/pass/`):
- **User-Defined Primitives (UDPs)**: Combinational, sequential, edge-sensitive primitives
- **Real number arithmetic**: All IEEE 754 operations, conversions, edge cases (infinity, NaN, denormals)
- **casez/casex pattern matching** (16 tests): Don't-care case statements, wildcard matching, priority encoding, state machines with X/Z tolerance
- **defparam hierarchical override** (9 tests): Parameter override across module boundaries, precedence rules, nested hierarchy, generate block instances
- **Generate blocks** (23 tests): Conditional/loop/case generate, nested generate, genvar scoping, gate instantiation, scoping edge cases, multi-dimensional unrolling
- **Timing semantics** (14 tests): Blocking vs. non-blocking assignment, delta cycles, NBA scheduling, race conditions, intra/inter-assignment delays, multi-always interactions, delayed NBA
- **Switch-level primitives** (23 tests): Tristate buffers (bufif/notif), transmission gates (tran/tranif), MOS switches (nmos/pmos/cmos), charge storage (trireg), drive strength resolution, wired logic, 4-state control values
- **Arithmetic and logic operations**: All operators including power (`**`)
- **Match-3 operators**: Case equality `===`/`!==`, wildcard match `==?`/`!=?`
- **Part-select assignment**: Fixed `[7:0]` and indexed `[idx +: 4]` ranges
- **Reduction operators**: All 6 variants (&, |, ^, ~&, ~|, ~^) in all contexts
- **Signed arithmetic**: Overflow, underflow, division by zero, sign extension
- **4-state logic**: X/Z propagation, full 4-state operator semantics
- **System tasks**: $display, $monitor, $time, $finish, $readmemh/b, $dumpvars, etc.
- **Memory operations**: Single and multi-dimensional arrays, hierarchical indexing, part-select with array access
- **Advanced net types**: tri, trireg, wand, wor, supply0/1
- **System functions**: File I/O ($fopen, $fclose, $fscanf, $fwrite, etc.), random ($random, $urandom), bit manipulation ($bits, $size, $dimensions)

### Running Tests

**Quick smoke test** (1 file, ~2 seconds):
```sh
./test_runner.sh
```

**Full test suite** (364 files, ~3 minutes):
```sh
./test_runner.sh --full
```

**Test modes**:
```sh
./test_runner.sh --4state      # Force 4-state mode for all tests
./test_runner.sh --2state      # Force 2-state mode (tests requiring X/Z marked N/A)
./test_runner.sh --sysverilog  # Run SystemVerilog tests (expected failures)
```

**Single file**:
```sh
./build/metalfpga_cli verilog/pass/test_example.v
./build/metalfpga_cli verilog/pass/test_example.v --4state  # With X/Z support
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

### Core Documentation
- [Project Overview](docs/gpga/README.md) - Architecture and goals
- [Verilog Coverage](docs/gpga/verilog_words.md) - Keyword implementation status
- [IR Invariants](docs/gpga/ir_invariants.md) - Flattened netlist guarantees
- [Roadmap](docs/gpga/roadmap.md) - Development milestones

### Technical References
- [4-State Logic Plan](docs/4STATE.md) - X/Z support design document
- [4-State API Reference](docs/gpga_4state_api.md) - Complete MSL library documentation (100+ functions)
- [Bit Packing Strategy](docs/bit_packing_strategy.md) - GPU memory optimization techniques
- [Verilog Reference](docs/VERILOG_REFERENCE.md) - Language reference
- [Async Debugging](docs/ASYNC_DEBUGGING.md) - Debugging asynchronous circuits

### Revision History
- [docs/diff/](docs/diff/) - REV documents tracking commit-by-commit changes (REV0-REV26)

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
  test_v1_ready_do_not_move.v  # Smoke test (1 file, default run)
  pass/           # Comprehensive test suite (364 files, --full flag)
  systemverilog/  # SystemVerilog tests (18 files, expected to fail)
docs/
  gpga/                 # Core documentation
  diff/                 # REV documents (REV0-REV26 commit changelogs)
  4STATE.md             # 4-state logic implementation
  gpga_4state_api.md    # Complete MSL 4-state library reference
  ANALOG.md             # Analog/mixed-signal support
  GPGA_KEYWORDS.md      # Verilog keyword reference
  VERILOG_REFERENCE.md  # Language reference
  ASYNC_DEBUGGING.md    # Async circuit debugging
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
