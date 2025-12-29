# metalfpga

A Verilog-to-Metal (MSL) compiler for GPU-based hardware simulation. Parses and lowers a large, practical subset of Verilog (spanning RTL and common testbench semantics) into Metal Shading Language compute kernels, enabling fast hardware prototyping and validation on Apple GPUs.

**Current Status**: v0.7+ — **Verilog frontend 100% complete** + **GPU runtime functional** + **VCD waveform generation** + **Wide integer support**: Full Verilog-2005 parsing, elaboration, and MSL codegen, plus Metal runtime with GPU execution and industry-standard VCD waveform output. Includes **software double-precision float** (IEEE 754), **wide integers** (128-bit, 256-bit, arbitrary width), User-Defined Primitives (UDPs), match-3 operators (`===`, `!==`, `==?`, `!=?`), power operator (`**`), multi-dimensional arrays, procedural part-select assignment, and **dynamic repeat loops**. Full **VCD debugging support** with `$dumpfile`, `$dumpvars`, dump control (`$dumpoff`/`$dumpon`), and `$readmemh`/`$readmemb`. **14 file I/O functions** including `$fopen`, `$fseek`, `$fread`, `$ungetc`, `$ferror`. Extensive support for generate blocks, 4-state logic, signed arithmetic, system tasks, timing controls, and switch-level primitives. **393 total test files** validate the compiler. **VCD smoke test passes ✅**. **Next: Full test suite validation on GPU (v1.0).**

## What is this?

metalfpga is a "GPGA" (GPU-based FPGA) compiler that:
- Parses Verilog RTL and testbench constructs
- Flattens module hierarchy through elaboration
- Generates Metal compute shaders for GPU execution
- Emits host-side runtime scaffolding

**Current phase:** ✅ **Verilog frontend complete** (parsing → elaboration → MSL codegen). ✅ **GPU runtime functional** (Metal framework integration, smoke test passes). ✅ **VCD waveform generation** (full debugging support with dump control). Next: Full test suite validation.

This allows FPGA designers to prototype and validate hardware designs on GPUs before synthesis to actual hardware, leveraging massive parallelism for fast simulation.

**Note**: The compiler successfully parses, elaborates, and generates MSL code for the full Verilog-2005 language. The Metal runtime infrastructure is functional with smoke test passing on actual GPU hardware. **VCD waveform generation is fully implemented** with support for `$dumpfile`, `$dumpvars`, `$dumpoff`/`$dumpon`, `$dumpflush`, `$dumpall`, and hierarchical signal naming. The frontend and runtime core are complete; ongoing work focuses on full test suite validation and `$display`/`$monitor` format string implementation.

## Quick Start

### Build
```sh
cmake -S . -B build
cmake --build build

# Run smoke test to verify GPU execution
./build/metalfpga_smoke
# Expected output: Smoke output: 1 2 3 4 5 6 7 8
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

# Run with VCD waveform output (requires --run, generates .vcd files)
./build/metalfpga_cli path/to/design.v --run --vcd-dir ./waves/
```

## Output Artifacts

The compiler produces:

- **Metal shaders** (`--emit-msl`): Emits `.metal` source containing compute kernels for combinational logic, sequential blocks, and scheduler infrastructure
- **Host runtime** (`--emit-host`): Complete executable `.mm` file with Metal runtime integration, buffer management, and service record handling
- **Flattened netlist** (`--dump-flat`): Human-readable elaborated design showing hierarchy flattening, signal widths, drivers, and lowered constructs
- **VCD waveforms** (automatic with `$dumpfile`/`$dumpvars`): Industry-standard `.vcd` files compatible with GTKWave, ModelSim, and other waveform viewers

**Runtime execution**: The Metal runtime (`src/runtime/metal_runtime.{hh,mm}`) provides GPU kernel compilation, dispatch, and buffer management. VCD writer (`src/main.mm`) generates waveform output with full dump control support. Smoke test validates the full compilation → execution → waveform pipeline.

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
- For/while loops (constant bounds, unrolled during elaboration)
- Repeat loops (both constant and dynamic/runtime-evaluated counts)
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
- String literals: `"Hello"` - Converted to packed ASCII bit values (little-endian, up to 8 characters/64 bits)
- **Wide integers**: Arbitrary-width literals and operations (128-bit, 256-bit, etc.) via concatenation-based decomposition
  - Wide literals: `128'hDEADBEEF_CAFEBABE`, `256'd12345`, etc.
  - Wide arithmetic: Multi-word add/sub with carry/borrow propagation
  - Wide shifts: Dynamic shifts across 64-bit boundaries (128-bit << 65)
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
  - Waveform: `$dumpfile`, `$dumpvars`, `$dumpall`, `$dumpon`, `$dumpoff`, `$dumpflush`, `$dumplimit` - Full VCD waveform generation with dump control
  - File I/O: `$fopen`, `$fclose`, `$fgetc`, `$fgets`, `$feof`, `$ftell`, `$rewind`, `$fseek`, `$fread`, `$ungetc`, `$ferror`, `$fflush`, `$fscanf`, `$sscanf` - File operations (14 functions, infrastructure complete, runtime execution pending)
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
- **Real number arithmetic** (IEEE 754 double-precision via software emulation):
  - `real` data type for floating-point variables
  - **Software double-precision**: Implemented using `ulong` (Metal GPUs lack native double hardware)
  - Real literals: `3.14`, `1.5e-10`, `.5`, `5.`
  - Real arrays: `real voltage[0:15];`
  - Real parameters: `parameter real PI = 3.14159;`
  - Arithmetic operators: `+`, `-`, `*`, `/`, `**` (power, partial support)
  - Comparison operators: `<`, `>`, `<=`, `>=`, `==`, `!=`
  - Type conversion: `$itor()` (int→real), `$rtoi()` (real→int with truncation)
  - Bit conversion: `$realtobits()`, `$bitstoreal()` (for real storage in 4-state)
  - Mixed integer/real arithmetic with automatic promotion
  - Real constant expressions in parameters and generate blocks
  - Special values: NaN, Infinity, ±0, denormals fully supported

### 🧪 Implemented with runtime validation in progress

The Metal runtime infrastructure is fully functional (smoke test passes on GPU). **VCD waveform generation is complete and working** with full dump control support. The following features are implemented in the compiler and runtime but require full validation:

- Event scheduling behavior (fork/join, wait, delays) under dispatch loop
- `$display`/`$monitor`/`$strobe` format string parsing and output
- Timing controls (`#delay`) in real-time execution
- Switch-level resolution correctness under GPU scheduling / write ordering
- Non-blocking assignment (`<=`) scheduling semantics

**Status:** Metal runtime successfully compiles, dispatches, and executes GPU kernels. VCD waveform generation fully working. `$readmemh`/`$readmemb` file I/O implemented. Smoke test validates the core pipeline. Full Verilog test suite validation is in progress.

### ❌ Not Yet Implemented
**High Priority**:
- Full test suite validation on GPU (smoke test and VCD tests pass, comprehensive suite pending)
- `$display`/`$monitor`/`$strobe` format string parsing (infrastructure exists, formatting pending)
- Timing delay execution and NBA scheduling validation
- Full sensitivity list support beyond `@*` and `@(posedge/negedge clk)`

**Low Priority**:
- SystemVerilog constructs
- Multi-GPU parallelization

**Non-goals** (for now):
- Full SystemVerilog compliance (classes, interfaces, packages, assertions)
- Full PLI/VPI compatibility
- Cycle-accurate timing matching commercial simulators

## Test Suite

**393 total test files** across the test suite:
- **13 files** in `verilog/` (smoke tests including VCD tests, for quick validation)
- **379 files** in `verilog/pass/` (comprehensive test suite, requires `--full` flag, runs in ~3 minutes)
- **18 files** in `verilog/systemverilog/` (SystemVerilog features, expected to fail)

The test suite validates parsing, elaboration, MSL codegen output quality, semantic correctness, and VCD waveform generation. Tests cover all major Verilog-2005 features including edge cases and advanced semantics.

**Note**: The default test run (`./test_runner.sh`) now executes only a single smoke test for rapid iteration during development. Use `./test_runner.sh --full` for comprehensive regression testing.

**Test coverage includes**:
- **User-Defined Primitives (UDPs)**: Combinational, sequential, edge-sensitive primitives
- **Real number arithmetic**: Software double-precision float, all IEEE 754 operations, conversions, edge cases (infinity, NaN, denormals)
- **Wide integers** (5 tests): 128-bit/256-bit literals, wide arithmetic with carry propagation, wide shifts across 64-bit boundaries, wide casez/X/Z matching
- **File I/O** (14 functions, 9+ tests): $fopen, $fseek, $fread, $ftell, $rewind, $ungetc, $ferror, $fflush, $fscanf/$sscanf with wide values
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
- **VCD waveform generation** (12 tests): `$dumpfile`, `$dumpvars`, dump control (`$dumpoff`/`$dumpon`), hierarchical signals, 4-state encoding, timing/edge detection, FSM visualization
- **Dynamic repeat loops** (1 test): Runtime-evaluated repeat counts with non-constant expressions
- **System tasks**: $display, $monitor, $time, $stime, $finish, $readmemh/b, $writememh/b, $dumpvars, $dumpoff, $dumpon, etc.
- **Memory operations**: Single and multi-dimensional arrays, hierarchical indexing, part-select with array access, wide memory ($readmemh with 128-bit values)
- **Advanced net types**: tri, trireg, wand, wor, supply0/1
- **System functions**: File I/O ($fopen, $fclose, $fseek, $fread, $fscanf, etc.), random ($random, $urandom), bit manipulation ($bits, $size, $dimensions)

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
- [Software Double Implementation](docs/SOFTFLOAT64_IMPLEMENTATION.md) - IEEE 754 double-precision emulation on Metal GPUs
- [GPGA Keywords Reference](docs/GPGA_KEYWORDS.md) - All `gpga_*` and `__gpga_*` keywords used in generated MSL
- [App Bundling Vision](docs/APP_BUNDLING.md) - HDL-to-native macOS applications
- [Bit Packing Strategy](docs/bit_packing_strategy.md) - GPU memory optimization techniques
- [Verilog Reference](docs/VERILOG_REFERENCE.md) - Language reference
- [Async Debugging](docs/ASYNC_DEBUGGING.md) - Debugging asynchronous circuits

### Revision History
- [docs/diff/](docs/diff/) - REV documents tracking commit-by-commit changes (REV0-REV32)
  - [REV32](docs/diff/REV32.md) - Wide integer support & complete file I/O (v0.7+)
  - [REV31](docs/diff/REV31.md) - Software double-precision float & extended file I/O (v0.7)
  - [REV30](docs/diff/REV30.md) - File I/O & string literals (v0.7)
  - [REV29](docs/diff/REV29.md) - Enhanced VCD & dynamic repeat (v0.7+)
  - [REV28](docs/diff/REV28.md) - VCD writer & service record integration (v0.7)
  - [REV27](docs/diff/REV27.md) - GPU runtime & smoke test success (v0.666)
  - [REV26](docs/diff/REV26.md) - Verilog frontend completion (v0.6+)
  - [REV25](docs/diff/REV25.md) - Edge case coverage & drive tracking (v0.6)

## Runtime Tools

### Smoke Test
```sh
./build/metalfpga_smoke [count]
```
Validates Metal runtime by executing a trivial increment kernel on GPU. Default count is 16 elements.

**Example output**:
```
Smoke output: 1 2 3 4 5 6 7 8
```

This confirms the entire pipeline works: Metal compilation → GPU dispatch → buffer readback.

### VCD Waveform Generation

Generate waveforms from Verilog testbenches:

```verilog
module test;
  reg clk;
  reg [7:0] counter;

  initial begin
    $dumpfile("waves.vcd");        // Set VCD output file
    $dumpvars(0, test);            // Dump all signals in module 'test'

    clk = 0;
    counter = 0;

    repeat (10) begin
      #1 clk = ~clk;
      counter = counter + 1;
    end

    $finish;
  end
endmodule
```

**Run with VCD output**:
```sh
./build/metalfpga_cli test.v --run --vcd-dir ./output/
# Generates: ./output/waves.vcd
```

**View waveforms**:
```sh
gtkwave ./output/waves.vcd
```

**Supported VCD features**:
- `$dumpfile(filename)` - Set VCD output file
- `$dumpvars(depth, module)` - Start waveform capture
- `$dumpoff` / `$dumpon` - Suspend/resume dumping
- `$dumpflush` - Flush VCD buffer to disk
- `$dumpall` - Force dump all signal values
- `$dumplimit(size)` - Set maximum dump count
- Hierarchical signal names with scope filtering
- 4-state logic (X/Z) encoding in VCD format
- Multi-dimensional arrays expanded to individual signals

## Project Structure

```
src/
  frontend/       # Verilog parser and AST
  core/           # Elaboration and flattening
  ir/             # Intermediate representation
  codegen/        # MSL and host code generation
  msl/            # Metal Shading Language backend
  runtime/        # Metal runtime wrapper (GPU execution)
  tools/          # Utility programs (smoke test)
  utils/          # Diagnostics and utilities
verilog/
  test_v1_ready_do_not_move.v  # Smoke test (1 file, default run)
  test_vcd_*.v                 # VCD waveform tests (12 files)
  test_repeat_dynamic.v        # Dynamic repeat test
  pass/           # Comprehensive test suite (365 files, --full flag)
  systemverilog/  # SystemVerilog tests (18 files, expected to fail)
docs/
  gpga/                 # Core documentation
  diff/                 # REV documents (REV0-REV29 commit changelogs)
  4STATE.md             # 4-state logic implementation
  gpga_4state_api.md    # Complete MSL 4-state library reference
  ANALOG.md             # Analog/mixed-signal support
  GPGA_KEYWORDS.md      # Verilog keyword reference
  VERILOG_REFERENCE.md  # Language reference
  ASYNC_DEBUGGING.md    # Async circuit debugging
  bit_packing_strategy.md  # GPU memory optimization
build/
  metalfpga_cli         # Main compiler executable
  metalfpga_smoke       # GPU runtime smoke test
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
