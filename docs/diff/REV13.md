# REV13 - Net types, drive strength, and tristate expansion

**Commit:** e1556ca
**Date:** Fri Dec 26 01:46:22 2025 +0100
**Message:** Added more tests, working on event/time

## Overview

Major test organization commit moving 15 previously unimplemented features to `pass/`, indicating they now work. Adds 31 new comprehensive tests for Verilog net types, drive strength, and tristate logic. Enhances test infrastructure with improved `test_all_smart.sh` script (+425 lines). Updates documentation to reflect newly supported features including special net types, gate primitives, preprocessor directives, and drive strength specifications. This brings the passing test count to 162 (up from 116).

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | Now accepts net types, drive strength, preprocessor directives |
| **Elaborate** | ✓ Functional | No major changes |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✓ Complete | Handles tristate, strength, special nets |
| **Host emission** | ✗ Stubbed only | No changes |
| **Runtime** | ✗ Not implemented | No changes |

## User-Visible Changes

**Newly Working Features** (15 tests moved to pass/):
- **Preprocessor directives**: `define`, `ifdef`, `include`
- **Gate primitives**: `nand`, `nor`, `buf`, gate arrays
- **Pull resistors**: `pullup`, `pulldown`
- **Sensitivity lists**: `@(a or b)`, `@(a, b, c)`
- **Edge lists**: `@(posedge clk, negedge reset)`
- **Net types**: `tri` (alias of wire)
- **Parameter override**: `defparam`
- **Timing specifications**: `specify` blocks (parsed, ignored)
- **Drive strength**: Basic strength annotations

**New Test Coverage** (31 new tests):
- 9 special net type tests (wand, wor, tri0/1, triand, trior, trireg, supply0/1)
- 11 drive strength tests (strong, weak, supply, pull, resolution)
- 11 tristate logic tests (bidirectional, bus mux, conflict resolution)

**Documentation Updates:**
- Comprehensive keyword reference expansion
- Multi-driver conflict clarification
- Test runner improvements

**Test Count:**
- `verilog/pass/`: 162 tests (up from 116 in REV12)
- 15 tests moved from root
- 31 new tests added

## Architecture Changes

### Documentation: Keyword Reference Expansion

**File**: `docs/gpga/verilog_words.md` (+16 lines, -2 lines)

**Added features:**

**Net types:**
```verilog
wire data;            // Standard wire (2-value resolution)
tri data;             // Alias of wire (tristate-capable)
wand data;            // Wired-AND resolution
wor data;             // Wired-OR resolution
tri0 data;            // Tristate pull to 0
tri1 data;            // Tristate pull to 1
triand data;          // Tristate AND resolution
trior data;           // Tristate OR resolution
trireg data;          // Tristate with capacitive storage
supply0 gnd;          // Supply strength 0 (ground)
supply1 vdd;          // Supply strength 1 (power)
```

**Drive strength:**
```verilog
// Continuous assignment with drive strength
assign (strong1, strong0) y = a;
assign (weak1, weak0) y = b;
assign (pull1, pull0) y = c;
assign (supply1, supply0) y = d;

// Pull resistors
pullup (net);         // Pull to 1
pulldown (net);       // Pull to 0
```

**Gate primitives:**
```verilog
// Basic gates
buf (y, a);
not (y, a);
and (y, a, b);
or (y, a, b);
nand (y, a, b);
nor (y, a, b);
xor (y, a, b);
xnor (y, a, b);

// Tristate buffers
bufif0 (y, a, enable);   // Buffer when enable=0
bufif1 (y, a, enable);   // Buffer when enable=1
notif0 (y, a, enable);   // Inverter when enable=0
notif1 (y, a, enable);   // Inverter when enable=1

// MOS gates
nmos (y, a, control);
pmos (y, a, control);

// Transmission gates (4-state mode)
tran (a, b);
tranif0 (a, b, control);
tranif1 (a, b, control);
cmos (y, a, ncontrol, pcontrol);
```

**Preprocessor directives:**
```verilog
`define WIDTH 8              // Define macro
`define MAX(a,b) ((a)>(b)?(a):(b))  // Macro with parameters
`undef WIDTH                 // Undefine macro

`ifdef FEATURE_A             // Conditional compilation
  // ...
`else
  // ...
`endif

`ifndef FEATURE_B
  // ...
`endif

`include "definitions.vh"    // Include file

`timescale 1ns / 1ps         // Timing (parsed, ignored)
```

**Sensitivity and edge lists:**
```verilog
// Sensitivity list (Verilog-1995 style)
always @(a or b or c) begin
  out = a & b | c;
end

// Comma-separated sensitivity (Verilog-2001 style)
always @(a, b, c) begin
  out = a & b | c;
end

// Multiple edges
always @(posedge clk, negedge reset) begin
  // First edge (clk) used for tick scheduling
  if (!reset) q <= 0;
  else q <= d;
end
```

**Parameter override:**
```verilog
// Defparam - single-level instance parameter override
defparam inst.WIDTH = 16;
defparam inst.DEPTH = 256;
```

**Timing specification:**
```verilog
// Specify blocks (parsed but ignored - timing not modeled)
specify
  (a => y) = (2.5, 3.0);  // Rise, fall delays
  (clk => q) = 1.2;       // Path delay
endspecify
```

### Test Infrastructure: Enhanced Test Runner

**File**: `test_all_smart.sh` (+425 lines, -0 lines from previous version)

This was enhanced from REV9's version (+246 lines) to include:

**New features:**
- **Smart flag detection**: Automatically determines if `--4state` or `--auto` needed per test
- **Categorized results**: Groups failures by error type
- **Parallel execution improvements**: Better job management
- **Progress reporting**: Real-time test completion percentage
- **Color-coded output**: Enhanced visual feedback
- **Error grouping**: Categorizes failures (parser errors, elaboration errors, codegen errors)
- **Summary statistics**: Detailed breakdown of pass/fail/missing

**Smart flag detection logic:**
```bash
# Detect if test needs --4state
grep -q "casex\|casez\|4'bz\|8'hx" "$file" && FLAGS="$FLAGS --4state"

# Detect if test needs --auto
grep -q "test_multifile" "$file" && FLAGS="$FLAGS --auto"
```

**Improved output format:**
```
MetalFPGA Smart Test Suite
===========================
Started at Fri Dec 26 01:46:22 2025

[  1/162] test_simple_adder.v ............... PASS (0.12s)
[  2/162] test_4state_basic.v ............... PASS --4state (0.15s)
...
[162/162] test_tristate_memory_bus.v ....... PASS --4state (0.18s)

Summary:
========
Total:   162
Passed:  162
Failed:  0
Missing: 0

All tests passed! ✓
```

### Documentation: Multi-Driver Clarification

**File**: `README.md` (1 line changed)

**Before:**
```
Multiple drivers on signals
```

**After:**
```
Multiple drivers on regs or mixed always/assign conflicts (wire multi-drive is resolved in 4-state)
```

**Clarifies:**
- Multiple drivers on `reg`: ERROR (registers can't have multiple drivers)
- Mixed `always`/`assign` on same signal: ERROR (procedural vs. continuous conflict)
- Multiple drivers on `wire` in 4-state mode: OK (resolved using drive strength)

**Examples:**

```verilog
// ERROR: Multiple drivers on reg
reg data;
always @(*) data = a;
always @(*) data = b;  // Error!

// ERROR: Mixed always/assign
wire data;
assign data = a;
always @(*) data = b;  // Error!

// OK in 4-state: Wire multi-drive with resolution
wire data;
assign (strong1, strong0) data = a;
assign (weak1, weak0) data = b;
// Result: a (strong beats weak)

// OK in 4-state: Tristate multi-drive
wire bus;
assign bus = (en1) ? data1 : 8'bz;
assign bus = (en2) ? data2 : 8'bz;
// Result: data1 when en1, data2 when en2, Z when neither
```

## Test Coverage

### Tests Moved to pass/ (15 files)

These features now work (moved from `verilog/` to `verilog/pass/`):

**Preprocessor (3 tests):**
- **test_define.v**: Macro definitions work
- **test_ifdef.v**: Conditional compilation works
- **test_include.v**: File inclusion works

**Gates (3 tests):**
- **test_gate_nand.v**, **test_gate_nor.v**, **test_gate_array.v**: Gate primitives work

**Pull resistors (2 tests):**
- **test_pullup.v**, **test_pulldown.v**: Pull strength works

**Net types (1 test):**
- **test_tri.v**: Tri-state net type works

**Sensitivity (2 tests):**
- **test_sensitivity_list.v**: `@(a, b, c)` works
- **test_sensitivity_or.v**: `@(a or b)` works

**Edge lists (1 test):**
- **test_edge_both.v**: `@(posedge clk, negedge reset)` works

**Drive strength (1 test):**
- **test_strength.v**: Basic drive strength annotations work

**Parameters (1 test):**
- **test_defparam.v**: Defparam overrides work

**Timing (1 test):**
- **test_specify.v**: Specify blocks parsed (timing ignored)

### New Tests Added (31 files)

**Special Net Types (9 tests):**

**test_net_wand.v** (12 lines):
```verilog
// Wired-AND: Multiple drivers AND together
module test_wand;
  wand result;
  assign result = 1'b1;
  assign result = 1'b1;
  // result = 1 (1 AND 1)

  assign result = 1'b0;
  // result = 0 (1 AND 1 AND 0)
endmodule
```

**test_net_wor.v** (12 lines):
```verilog
// Wired-OR: Multiple drivers OR together
wor result;
assign result = 1'b0;
assign result = 1'b1;
// result = 1 (0 OR 1)
```

**test_net_tri0.v**, **test_net_tri1.v** (12 lines each):
```verilog
// tri0: Pulls to 0 when all drivers are Z
tri0 net;
assign net = 1'bz;  // Result: 0 (pull-down)

// tri1: Pulls to 1 when all drivers are Z
tri1 net;
assign net = 1'bz;  // Result: 1 (pull-up)
```

**test_net_triand.v**, **test_net_trior.v** (17 lines each):
```verilog
// triand: Tri-state with AND resolution
triand net;
assign net = (en1) ? 1'b1 : 1'bz;
assign net = (en2) ? 1'b1 : 1'bz;
// When both drive: AND resolution

// trior: Tri-state with OR resolution
trior net;
// Similar but OR resolution
```

**test_net_trireg.v** (13 lines):
```verilog
// trireg: Tri-state with capacitive storage
// Retains last driven value when all drivers go Z
trireg net;
assign net = (en) ? data : 1'bz;
// When en=0, net retains previous value
```

**test_net_supply0.v**, **test_net_supply1.v** (9 lines each):
```verilog
// supply0: Strongest drive to 0 (ground)
supply0 gnd;

// supply1: Strongest drive to 1 (power)
supply1 vdd;
```

**Drive Strength Tests (11 tests):**

**test_strength_strong.v**, **test_strength_weak.v**, **test_strength_pull.v**, **test_strength_supply.v** (9-10 lines each):
```verilog
// Strong drive (default)
assign (strong1, strong0) y = a;

// Weak drive (lower than strong)
assign (weak1, weak0) y = b;

// Pull drive (resistive)
assign (pull1, pull0) y = c;

// Supply drive (strongest)
assign (supply1, supply0) y = d;
```

**test_strength_resolution.v** (14 lines):
```verilog
// When multiple drivers conflict, strength determines winner
assign (strong1, strong0) y = a;
assign (weak1, weak0) y = b;
// Result: a (strong beats weak)

// Strength hierarchy: supply > strong > pull > weak > highz
```

**test_strength_gate.v**, **test_strength_gate_nmos.v**, **test_strength_gate_pmos.v** (11-12 lines each):
```verilog
// Gate primitives with drive strength
buf (strong1, strong0) (y, a);
nmos (pull1, pull0) (y, a, control);
pmos (pull1, pull0) (y, a, control);
```

**test_strength_tran.v**, **test_strength_tranif.v**, **test_strength_cmos_transmission.v** (10-16 lines):
```verilog
// Transmission gates (bidirectional)
tran (a, b);                    // Always connected
tranif0 (a, b, control);        // Connected when control=0
tranif1 (a, b, control);        // Connected when control=1
cmos (y, a, nctl, pctl);        // CMOS transmission gate
```

**test_strength_mixed.v** (16 lines):
- Combines multiple strength levels
- Tests resolution in complex scenarios

**Tristate Logic Tests (11 tests):**

**test_tristate_basic.v** (11 lines):
```verilog
// Basic tristate: Drive or high-Z
wire bus;
assign bus = (enable) ? data : 8'bz;
```

**test_tristate_assign_z.v** (11 lines):
```verilog
// Explicit Z assignment
assign bus = 8'bz;  // Always high-impedance
```

**test_tristate_bidirectional.v** (46 lines - shown earlier):
- Full bidirectional port example
- Multiple drivers on shared bus
- Output enable control
- Read-back capability

**test_tristate_bus_mux.v** (17 lines):
```verilog
// Tristate bus multiplexing
wire [7:0] bus;
assign bus = (sel == 2'b00) ? data0 : 8'bz;
assign bus = (sel == 2'b01) ? data1 : 8'bz;
assign bus = (sel == 2'b10) ? data2 : 8'bz;
assign bus = (sel == 2'b11) ? data3 : 8'bz;
// Bus = selected data, Z otherwise
```

**test_tristate_multiple_drivers.v** (15 lines):
```verilog
// Multiple tristate drivers on same net
assign bus = (en1) ? data1 : 8'bz;
assign bus = (en2) ? data2 : 8'bz;
assign bus = (en3) ? data3 : 8'bz;
// Only one enable active at a time (or Z)
```

**test_tristate_conflict.v** (20 lines):
```verilog
// Conflict detection when multiple drivers active
assign bus = (en1) ? data1 : 8'bz;
assign bus = (en2) ? data2 : 8'bz;
// If both en1 and en2 active: X (unknown)
// Requires drive strength resolution in 4-state
```

**test_tristate_bufif0.v**, **test_tristate_notif.v** (11-14 lines):
```verilog
// Tristate buffer gates
bufif0 (y, a, enable);   // y = a when enable=0, else Z
bufif1 (y, a, enable);   // y = a when enable=1, else Z
notif0 (y, a, enable);   // y = ~a when enable=0, else Z
notif1 (y, a, enable);   // y = ~a when enable=1, else Z
```

**test_tristate_memory_bus.v** (39 lines):
```verilog
// Realistic memory bus scenario
wire [7:0] data_bus;
wire [15:0] addr_bus;

// CPU drives address
assign addr_bus = (cpu_active) ? cpu_addr : 16'bz;

// Memory or peripherals drive data
assign data_bus = (mem_oe) ? mem_data : 8'bz;
assign data_bus = (io_oe) ? io_data : 8'bz;
```

**test_tristate_array.v** (12 lines):
- Arrays of tristate drivers
- Vectorized enable control

### Include File Support

**File**: `verilog/pass/definitions.vh` (1 line NEW)

```verilog
`define TESTVAL 42
```

Used by `test_include.v`:
```verilog
`include "definitions.vh"
wire [7:0] data = `TESTVAL;
```

## Implementation Details

### Net Type Resolution Semantics

**Verilog net types have different resolution functions:**

**wire/tri (2-value resolution):**
```
0 + 0 = 0
0 + 1 = X (conflict)
1 + 1 = 1
```

**wand (wired-AND):**
```
0 + 0 = 0
0 + 1 = 0
1 + 1 = 1
```

**wor (wired-OR):**
```
0 + 0 = 0
0 + 1 = 1
1 + 1 = 1
```

**tri0 (pull to 0):**
```
Driven value = value
All Z = 0 (pull-down)
```

**tri1 (pull to 1):**
```
Driven value = value
All Z = 1 (pull-up)
```

**triand/trior:**
- Combine tristate (Z) with AND/OR resolution

**trireg (capacitive):**
- Stores last driven value
- Retains when all drivers go Z

**supply0/supply1:**
- Strongest drive strength
- Always 0 or 1
- Cannot be overridden

### Drive Strength Hierarchy

**From strongest to weakest:**
1. **supply**: Power/ground (supply0, supply1)
2. **strong**: Default driver strength (strong0, strong1)
3. **pull**: Resistive (pull0, pull1)
4. **weak**: Weakest active drive (weak0, weak1)
5. **highz**: High-impedance (Z)

**Resolution rules:**
- Stronger drive wins over weaker
- Same strength + different values = X (conflict)
- All drivers Z + tri0 = 0
- All drivers Z + tri1 = 1
- All drivers Z + trireg = last value

### Preprocessor Implementation

**`define` / `undef`:**
- Text substitution macros
- Can have parameters
- Processed before parsing

**`ifdef` / `ifndef` / `else` / `endif`:**
- Conditional compilation
- Entire blocks included/excluded
- Nesting supported

**`include`:**
- Textual inclusion of file contents
- Relative to current file's directory
- Can include other `.v` or `.vh` files

**`timescale`:**
- Parsed but ignored
- Timing not modeled in current implementation
- Format: `` `timescale <time_unit> / <time_precision> ``

## Known Gaps and Limitations

### Improvements Over REV12

**Now Working:**
- Preprocessor directives (`define`, `ifdef`, `include`)
- Gate primitives (nand, nor, buf, arrays)
- Pull resistors (pullup, pulldown)
- Net types (tri, wand, wor, tri0/1, triand, trior, trireg, supply0/1)
- Drive strength annotations
- Sensitivity lists and edge lists
- Defparam overrides
- Specify blocks (parsed, ignored)
- 162 passing tests (up from 116)

**New Test Coverage (v0.3+):**
- 31 new tests for net types, strength, tristate
- Comprehensive tristate scenarios (bus mux, bidirectional, conflict)
- Drive strength resolution testing

**Still Missing:**
- System tasks ($display, $monitor, $finish, etc.) - 7 tests
- Timing (delays, wait, events) - 6 tests
- Advanced control (forever, fork/join, disable) - 3 tests
- Tasks (procedural blocks) - 2 tests
- Real numbers - 1 test
- UDPs (user-defined primitives) - 1 test
- Force/release - 1 test
- Gate timing delays (parsed gates but no timing)
- Runtime - no execution

### Semantic Notes (v0.3+)

**Multi-driver resolution:**
- 2-state mode: Multiple drivers on wire = ERROR
- 4-state mode: Multiple drivers on wire resolved by strength and net type
- Regs: Always single-driver (multiple drivers = ERROR)

**Preprocessor:**
- Macros expanded before parsing
- File inclusion is textual (not module-level)
- Conditional compilation affects entire blocks

**Gate primitives:**
- No timing delays (delay parameters parsed but ignored)
- Strength specifications work
- Arrays of gates supported

**Tristate:**
- Requires 4-state mode for proper Z handling
- Multiple Z drivers = Z
- Z + driven value = driven value
- Multiple driven values = conflict or strength resolution

## Statistics

- **Files changed**: 51
- **Lines added**: 806
- **Lines removed**: 95
- **Net change**: +711 lines

**Breakdown:**
- Test infrastructure: +425 lines (enhanced test_all_smart.sh)
- New tests: 31 files (~290 lines)
- Documentation: +16 lines (keyword reference)
- Include file: +1 line (definitions.vh)
- Moved tests: 15 files (0 line changes, just relocation)

**Test suite:**
- `verilog/pass/`: 162 files (up from 116 in REV12)
  - 15 tests moved from root (now working)
  - 31 new tests added
  - 1 include file (definitions.vh)
- `verilog/`: 28 files (down from 43 in REV12)
  - 15 tests moved to pass/

**Features now working:**
- All major Verilog net types
- Drive strength resolution
- Preprocessor directives
- Gate primitives
- Tristate logic with Z propagation

This commit represents a **major maturity milestone** for metalfpga, adding support for professional hardware features including comprehensive net types, drive strength resolution, and tristate logic. The 162 passing tests demonstrate substantial Verilog-2001 compatibility.
