# REV12 - Test expansion and tooling improvements

**Commit:** 070ef24
**Date:** Thu Dec 25 22:35:31 2025 +0100
**Message:** Added more tests

## Overview

Test-focused commit adding 41 new test cases for unimplemented Verilog features, serving as a comprehensive specification for future work. Also adds professional tooling: Unix man page (`metalfpga.1`) and enhanced CLI with multi-file support and auto-discovery. No implementation changes - this is purely testing/documentation/tooling infrastructure. Tests document expected behavior for advanced features including system tasks, timing controls, preprocessor directives, gate primitives, and tasks.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No implementation changes |
| **Elaborate** | ✓ Functional | No implementation changes |
| **Codegen (2-state)** | ✓ MSL emission | No implementation changes |
| **Codegen (4-state)** | ✓ Complete | No implementation changes |
| **Host emission** | ✗ Stubbed only | No implementation changes |
| **Runtime** | ✗ Not implemented | No implementation changes |

## User-Visible Changes

**CLI Enhancements:**
- **Multi-file input**: Pass multiple `.v` files on command line
- **Auto-discovery**: `--auto` flag to find modules in directory tree
- **Better usage message**: Shows multi-file syntax

**Documentation:**
- **Unix man page**: `metalfpga.1` (296 lines) - professional tool documentation
- **README updates**: Multi-file examples and auto-discovery usage

**Test Organization:**
- 2 tests moved to `pass/` (multi-file tests now working)
- 41 new tests for unimplemented features (specification for future work)

**No Feature Implementations:**
- All new tests are for **unimplemented features**
- Tests serve as executable specification
- Each test includes comment header explaining expected behavior

## Architecture Changes

### CLI: Multi-File Support and Auto-Discovery

**File**: `src/main.mm` (+108 lines, -14 lines)

**Multi-file input:**

```cpp
// Old CLI: Single file only
./metalfpga_cli design.v

// New CLI: Multiple files
./metalfpga_cli top.v utils.v memory.v --top top

// Command-line parsing:
std::vector<std::string> input_paths;  // Changed from single string
// Collect all positional args as input files
```

**Auto-discovery feature:**

```cpp
// New flag: --auto
./metalfpga_cli path/to/top.v --auto --top top

// Behavior:
// 1. Start with explicitly provided file(s)
// 2. Recursively scan directory tree for *.v files
// 3. Parse all discovered modules
// 4. Resolve module references across files
```

**Implementation details:**

```cpp
#include <filesystem>  // NEW: For directory traversal

struct ParseItem {
  std::string path;
  bool explicit_input = false;  // User-provided vs auto-discovered
};

std::vector<ParseItem> parse_queue;
std::unordered_map<std::string, size_t> seen_paths;  // Prevent duplicates

// Deduplication using normalized paths:
std::filesystem::path normalized =
    std::filesystem::weakly_canonical(fs_path, ec);
std::string key = ec ? fs_path.lexically_normal().string()
                     : normalized.string();
```

**Multi-file parsing strategy:**
- Parse all files into single `Program` structure
- `Program.modules` accumulates modules from all files
- Module resolution happens during elaboration (find references)
- Allows modules to reference each other across files

**Test files moved to pass/:**
- `test_multifile_a.v` and `test_multifile_b.v` - multi-file modules now work

### Documentation: Unix Man Page

**File**: `metalfpga.1` (296 lines NEW)

Full Unix manual page in `troff` format. Sections:

**NAME**:
```
metalfpga - Verilog-to-Metal (MSL) compiler for GPU-based
           hardware simulation
```

**SYNOPSIS**:
```
metalfpga input.v [more.v ...] [OPTIONS]
```

**DESCRIPTION**:
- Explains GPGA concept (GPU-based FPGA)
- Describes compilation pipeline
- Notes Apple GPU requirement

**OPTIONS**:
```
--emit-msl PATH       Generate Metal compute kernel
--emit-host PATH      Generate host runtime stub
--dump-flat           Dump flattened netlist
--top MODULE          Specify top-level module
--4state              Enable 4-state logic (0/1/X/Z)
--auto                Auto-discover modules in directory tree
```

**EXAMPLES**:
```bash
# Basic compilation
metalfpga design.v --emit-msl output.metal

# Multi-file project
metalfpga top.v utils.v --top processor --emit-msl proc.metal

# Auto-discovery
metalfpga src/top.v --auto --top system

# 4-state logic
metalfpga design.v --4state --emit-msl output.metal
```

**SUPPORTED FEATURES**:
- Comprehensive list of working Verilog features
- Organized by category (modules, operators, control flow, etc.)

**LIMITATIONS**:
- Lists unimplemented features
- Notes runtime not yet available

**AUTHORS**:
- Project attribution

**SEE ALSO**:
- References to Verilog standards
- Links to related tools

**Installation:**
```bash
# Install man page (on Unix systems):
sudo cp metalfpga.1 /usr/local/share/man/man1/
sudo mandb  # Update man database

# View man page:
man metalfpga
```

## Test Coverage

### Tests for Unimplemented Features (41 new files)

All tests are in `verilog/` (not `pass/`) because features are not implemented. Each test:
- Has descriptive comment header
- Explains expected behavior
- Notes "Expected: Should fail - <feature> not yet implemented"
- Serves as specification for future implementation

**System Tasks (7 tests):**

**test_display.v** (16 lines):
```verilog
// Test: $display system task
// Expected: Should fail - system tasks not implemented

module test_display;
  initial begin
    $display("Hello from metalfpga!");
    $display("Value: %d", 8'd42);
  end
endmodule
```

**test_monitor.v** (20 lines):
```verilog
// $monitor - continuous monitoring of signal changes
initial begin
  $monitor("Time=%0t a=%d b=%d", $time, a, b);
end
```

**test_dumpfile.v** (19 lines):
```verilog
// $dumpfile, $dumpvars - waveform dumping
initial begin
  $dumpfile("waves.vcd");
  $dumpvars(0, top);
end
```

**test_finish.v** (17 lines):
```verilog
// $finish - terminate simulation
initial begin
  #100 $finish;
end
```

**test_random.v** (14 lines):
```verilog
// $random - random number generation
integer rand_val;
initial begin
  rand_val = $random;
end
```

**test_readmemb.v** and **test_readmemh.v** (11 lines each):
```verilog
// $readmemb / $readmemh - initialize memory from file
reg [7:0] mem [0:255];
initial begin
  $readmemh("data.hex", mem);
end
```

**Timing Controls (6 tests):**

**test_delay_assign.v** (13 lines):
```verilog
// Delayed assignments with # delay
initial begin
  #10 data = 8'hFF;  // Wait 10 time units
  #5 data = 8'h00;   // Wait 5 more
end
```

**test_time.v** (14 lines):
```verilog
// $time system function
initial begin
  current_time = $time;
end
```

**test_time_literal.v** (16 lines):
```verilog
// Time literals: 10ns, 5us, etc.
#10ns data = 1;
#5us data = 0;
```

**test_timescale.v** (15 lines):
```verilog
// `timescale directive
`timescale 1ns / 1ps  // time unit / time precision
```

**test_wait.v** (19 lines):
```verilog
// wait statement - wait for condition
initial begin
  wait (ready == 1);
  data = 8'hFF;
end
```

**test_event.v** (17 lines):
```verilog
// Named events and triggers
event data_ready;
initial begin
  @(data_ready);  // Wait for event
  data = 8'hFF;
end
initial begin
  #10 -> data_ready;  // Trigger event
end
```

**Advanced Control Flow (3 tests):**

**test_forever.v** (12 lines):
```verilog
// forever loop - infinite loop
initial begin
  forever begin
    #10 clk = ~clk;  // Clock generation
  end
end
```

**test_fork_join.v** (15 lines):
```verilog
// fork/join - parallel execution blocks
initial begin
  fork
    #10 a = 1;
    #20 b = 1;
    #30 c = 1;
  join
end
```

**test_disable.v** (17 lines):
```verilog
// disable statement - terminate named block
initial begin : main_block
  fork
    #100 disable main_block;  // Exit early
  join
end
```

**Gate-Level Primitives (4 tests):**

**test_gate_nand.v**, **test_gate_nor.v**, **test_gate_buf.v**, **test_gate_array.v** (7 lines each):
```verilog
// Built-in gate primitives
module test_gate_nand(input a, input b, output y);
  nand g1(y, a, b);
endmodule

module test_gate_array(input [3:0] a, output [3:0] y);
  buf g[3:0](y, a);  // Array of buffers
endmodule
```

**Drive Strength (3 tests):**

**test_pullup.v**, **test_pulldown.v**, **test_strength.v** (7 lines each):
```verilog
// Pull-up resistor
pullup (net);

// Pull-down resistor
pulldown (net);

// Drive strength
assign (strong1, weak0) out = in;
```

**Net Types (1 test):**

**test_tri.v** (19 lines):
```verilog
// Tri-state net type
tri [7:0] bus;
assign bus = (drive) ? data : 8'bz;
```

**Preprocessor Directives (3 tests):**

**test_define.v** (16 lines):
```verilog
// `define macros
`define WIDTH 8
`define MAX_VAL 255

wire [`WIDTH-1:0] data;
```

**test_ifdef.v** (19 lines):
```verilog
// Conditional compilation
`ifdef FEATURE_A
  assign out = in << 1;
`else
  assign out = in;
`endif
```

**test_include.v** (14 lines):
```verilog
// Include other files
`include "definitions.vh"
```

**Tasks (2 tests):**

**test_task_basic.v** (24 lines):
```verilog
// Task declaration and call
task add_numbers;
  input [7:0] x;
  input [7:0] y;
  output [7:0] sum;
  begin
    sum = x + y;
  end
endtask

initial begin
  add_numbers(a, b, result);
end
```

Note: Tasks differ from functions (implemented in REV11):
- Functions: Return value, no side effects, used in expressions
- Tasks: No return, can have side effects, statement only

**test_task_void.v** (20 lines):
- Task with no outputs (side effects only)

**Other Advanced Features (12 tests):**

**test_defparam.v** (18 lines):
```verilog
// Defparam - override module parameters
defparam inst.WIDTH = 16;
```

**test_edge_both.v** (21 lines):
```verilog
// Both edge sensitivity
always @(posedge clk or negedge reset) begin
  // ...
end
```

**test_force_release.v** (19 lines):
```verilog
// Force/release - debugging construct
initial begin
  force sig = 1;
  #10 release sig;
end
```

**test_real.v** (15 lines):
```verilog
// Real number type
real pi = 3.14159;
real voltage;
```

**test_sensitivity_list.v** (19 lines):
```verilog
// Explicit sensitivity list
always @(a or b or c) begin
  out = a & b | c;
end
```

**test_sensitivity_or.v** (18 lines):
```verilog
// Sensitivity with 'or' keyword (Verilog-1995 style)
always @(a or b) begin
  sum = a + b;
end
```

**test_specify.v** (11 lines):
```verilog
// Specify block - timing specifications
specify
  (a => y) = (2.5, 3.0);  // Rise/fall delays
endspecify
```

**test_udp.v** (17 lines):
```verilog
// User-defined primitive
primitive my_and (output y, input a, input b);
  table
    0 0 : 0;
    0 1 : 0;
    1 0 : 0;
    1 1 : 1;
  endtable
endprimitive
```

## Documentation Updates

### README.md (+9 lines)

**New CLI examples:**
```bash
# Multiple files (module references across files)
./build/metalfpga_cli path/to/file_a.v path/to/file_b.v --top top

# Auto-discover modules under the input file's directory tree
./build/metalfpga_cli path/to/top.v --auto --top top
```

**Features section updated:**
- Multi-file input mentioned
- Auto-discover module files documented
- Multi-file module references noted

## Implementation Details

### Multi-File Compilation Strategy

**File resolution:**
1. Parse all input files into single `Program`
2. Each file adds modules to `Program.modules`
3. Elaboration resolves cross-file module references
4. Module names must be unique across all files

**Example multi-file project:**

```verilog
// file_a.v
module utils;
  // Utility module
endmodule

// file_b.v
module top;
  utils u();  // Reference module from file_a.v
endmodule

// Compile:
metalfpga file_a.v file_b.v --top top
```

**Auto-discovery use case:**
```
project/
  src/
    top.v         # Has: module top
    alu.v         # Has: module alu
    memory.v      # Has: module ram
    utils/
      helpers.v   # Has: module helpers

# Compile with auto-discovery:
cd project
metalfpga src/top.v --auto --top top

# Discovers: top.v, alu.v, memory.v, helpers.v
# Parses all, resolves references
```

### Test Categorization Strategy

**Tests in `verilog/pass/` (implemented):**
- Features working in v0.3
- Parse, elaborate, codegen all succeed
- 114 tests as of REV11

**Tests in `verilog/` (not implemented):**
- Features planned but not yet working
- Parser may accept or reject
- Serve as specification for future work
- 41 NEW tests in REV12 (plus 2 from earlier)

**Test naming convention:**
- All tests: `test_<feature>.v`
- Consistent across working and unimplemented

## Known Gaps and Limitations

### Improvements Over REV11

**Now Working:**
- Multi-file input on CLI
- Auto-discovery of modules in directory tree
- 2 more tests passing (multifile tests moved to pass/)

**No New Features Implemented:**
- This is a testing/tooling commit
- No parser, elaboration, or codegen changes
- 41 new tests document unimplemented features

**Still Missing (now with tests):**
- System tasks: $display, $monitor, $finish, $random, $readmem, $dumpvars (7 tests)
- Timing: delays (#), wait, events, timescale (6 tests)
- Control: forever, fork/join, disable (3 tests)
- Gates: primitives, arrays (4 tests)
- Drive: pullup, pulldown, strength (3 tests)
- Preprocessor: `define, `ifdef, `include (3 tests)
- Tasks: basic, void (2 tests) - NOTE: functions implemented in REV11
- Advanced: defparam, real, specify, UDPs (12 tests total)
- Runtime - no execution

### Semantic Notes (v0.3)

**Multi-file compilation:**
- Module names must be globally unique
- Files parsed in order specified on CLI
- Auto-discovery traverses depth-first
- Duplicate paths detected via filesystem normalization

**Test organization philosophy:**
- `pass/`: Features implemented and tested
- `verilog/`: Features specified but not implemented
- All tests executable (even if they fail)
- Comment headers explain expected behavior

## Statistics

- **Files changed**: 43
- **Lines added**: 945
- **Lines removed**: 14
- **Net change**: +931 lines

**Breakdown:**
- Man page: +296 lines (new file `metalfpga.1`)
- CLI enhancements: +108 lines (multi-file, auto-discovery)
- README: +9 lines (usage examples)
- New tests: 41 files (approximately 580 lines total)
- Parser cleanup: -1 line

**Test suite:**
- `verilog/pass/`: 116 files (up from 114 in REV11)
  - 2 tests moved from root (multifile tests)
- `verilog/`: 43 files (up from 3 in REV11)
  - 41 new tests for unimplemented features
  - 2 from before (test_function.v, test_for_loop.v removed - now in pass/)

**Documentation:**
- Professional Unix man page (troff format)
- Installable in system man directories
- Comprehensive option documentation

This commit establishes metalfpga as a **professional tool** with proper documentation (man page) and flexible input handling (multi-file, auto-discovery). The 41 new tests create a **comprehensive specification** for future development, documenting expected behavior for advanced Verilog features that will be implemented in subsequent commits.
