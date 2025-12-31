# Verilog Test Suite Overview

This document provides a comprehensive overview of all test files in the `verilog/` directory. The test suite validates metalfpga's Verilog compilation and Metal shader generation capabilities.

## Test Organization

Tests are organized into three main categories:

### 1. Root-Level Tests (`verilog/`)
Integration and feature tests focusing on VCD generation, real number math, and complete simulations.

### 2. Pass Tests (`verilog/pass/`)
Unit tests covering individual Verilog language features. These tests are expected to compile and run successfully.

### 3. SystemVerilog Tests (`verilog/systemverilog/`)
SystemVerilog-specific constructs. Most are expected to fail as metalfpga primarily targets Verilog-2001/2005.

---

## Root-Level Tests (22 files)

| Test | Description | Category |
|------|-------------|----------|
| `test_clock_big_vcd.v` | Large clock generation with VCD output | VCD / Timing |
| `test_clock_gen.v` | Basic clock generator | Timing |
| `test_parametric_arithmetic.v` | Parameterized arithmetic operations | Arithmetic |
| `test_real_math.v` | Real number math functions ($ln, $exp, $sqrt, $pow, $sin, $cos, etc.) | Real Math |
| `test_repeat_dynamic.v` | Dynamic repeat statement | Control Flow |
| `test_resistor_power.v` | Analog resistor power calculation | Analog / Real |
| `test_shift_left_arithmetic.v` | Arithmetic left shift operations | Arithmetic |
| `test_v1_ready_do_not_move.v` | Legacy test (do not modify) | Legacy |
| `test_vcd_4state_xz.v` | VCD with X/Z states | VCD / 4-State |
| `test_vcd_array_memory.v` | VCD with array/memory signals | VCD |
| `test_vcd_conditional_dump.v` | Conditional VCD dumping | VCD |
| `test_vcd_counter_basic.v` | Basic counter with VCD | VCD |
| `test_vcd_edge_detection.v` | Edge detection with VCD | VCD / Timing |
| `test_vcd_fsm.v` | Finite state machine with VCD | VCD / FSM |
| `test_vcd_hierarchy_deep.v` | Deep hierarchical design with VCD | VCD / Hierarchy |
| `test_vcd_multi_signal.v` | Multiple signals in VCD | VCD |
| `test_vcd_nba_ordering.v` | Non-blocking assignment ordering with VCD | VCD / NBA |
| `test_vcd_smoke.v` | Minimal VCD smoke test | VCD |
| `test_vcd_timing_delays.v` | Timing delays with VCD | VCD / Timing |
| `test_vcd_wide_signals.v` | Wide bus signals in VCD | VCD |

---

## Pass Tests (`verilog/pass/`) - 363 files

Comprehensive unit tests covering all major Verilog language features. Key categories:

### Operators & Expressions (40+ tests)
- **Arithmetic**: `test_alu_with_flags.v`, `test_wide_arithmetic_carry.v`, `test_integer_division.v`, `test_divide_by_zero.v`, `test_power_operator.v`
- **Signed operations**: `test_signed_*.v` (13 tests) - division, shifts, comparisons, ternary, width extension
- **Reduction operators**: `test_reduction_*.v` (8 tests) - comprehensive, nested, in always blocks, ternary, wide buses
- **Shifts**: `test_shift_*.v` - ops, precedence, mask concat, dynamic wide shifts
- **Comparison**: `test_comparison_all.v`, `test_match3_ops.v`, `test_match3_ops64.v`
- **Unary**: `test_unary_chains.v`, `test_real_unary.v`, `test_signed_unary.v`
- **Ternary**: `test_ternary_nested_complex.v`, `test_nested_ternary.v`, `test_reduction_ternary.v`

### Data Types & Arrays (30+ tests)
- **Arrays**: `test_array_*.v` - 2D/3D/5D arrays, matmul, convolution, bounds checking
- **Memory**: `test_memory_*.v` - read, write, arrays
- **Multi-dimensional**: `test_multi_dim_array.v`
- **Real numbers**: `test_real_*.v` (17 tests) - arithmetic, arrays, comparisons, edge values, generate, parameters
- **Bit manipulation**: `test_bit_selects.v`, `test_part_select*.v`, `test_concatenation_edge.v`, `test_concat_nesting.v`
- **Literals**: `test_literal_formats.v`, `test_time_literal.v`, `test_real_literal.v`

### Control Flow (35+ tests)
- **Loops**: `test_for_*.v` (7 tests), `test_while_*.v` (8 tests), `test_repeat_*.v`, `test_forever*.v`
- **Case statements**: `test_case*.v` (7 tests), `test_casex*.v` (12 tests), `test_casez*.v` (12 tests)
- **Conditional**: `test_conditional_*.v` - assign, generate
- **Event control**: `test_event*.v`, `test_wait*.v`, `test_disable.v`

### Generate Blocks (40+ tests)
- **Conditional**: `test_generate_if_*.v`, `test_generate_conditional*.v`
- **For loops**: `test_generate_for_*.v`, `test_generate_loops.v`
- **Case**: `test_generate_case*.v`
- **Complex**: `test_generate_nested*.v`, `test_generate_mixed_*.v`, `test_generate_complex.v`
- **Gates**: `test_generate_gate_*.v` - conditional gates, matrix, outputs
- **Parameters**: `test_generate_param*.v`, `test_generate_localparam.v`
- **Scoping**: `test_generate_*_scoping.v`, `test_genvar_*.v`

### Timing & Scheduling (20+ tests)
- **Delays**: `test_delay_*.v`, `test_assign_delay.v`, `test_inter_assignment_delay.v`, `test_intra_assignment_delay.v`
- **Non-blocking assignments**: `test_nba_*.v` (6 tests) - ordering, races, delays
- **Blocking vs NBA**: `test_blocking_*.v`
- **Edge sensitivity**: `test_edge_*.v`, `test_negedge.v`, `test_mixed_edge.v`
- **Delta cycles**: `test_delta_cycle_ordering.v`, `test_event_wait_delta.v`
- **Timing control**: `test_charge_decay_timing.v`, `test_trireg_transition_timing.v`

### Gates & Primitives (25+ tests)
- **Basic gates**: `test_gate_*.v` - buf, nand, nor, arrays
- **Tristate**: `test_tristate_*.v` (10 tests) - bufif, notif, bidirectional, bus mux
- **Switches**: `test_cmos_switches.v`, `test_mos_switches.v`, `test_tran_gates.v`, `test_tranif_bidirectional.v`
- **UDP**: `test_udp*.v` (4 tests) - basic, edge, sequential
- **Net types**: `test_net_*.v` (11 tests) - supply0/1, tri0/1, triand, trior, trireg, wand, wor

### Signal Strength (15+ tests)
- **Strength resolution**: `test_strength_*.v` (15 tests) - conflict, propagation, pull, supply, weak, strong, gate interactions
- **Trireg**: `test_trireg_*.v` (8 tests) - charge hold, decay, capacitance, drive strength

### Functions & Tasks (8 tests)
- **Functions**: `test_function*.v` - basic, automatic, recursive, const
- **Tasks**: `test_task_*.v` - basic, output, void

### System Tasks (60+ tests)
- **Display**: `test_system_display.v`, `test_system_write.v`, `test_system_monitor.v`, `test_system_strobe.v`
- **File I/O**: `test_system_f*.v` (20+ tests) - fopen, fclose, fscanf, fprintf, fread, fwrite, fseek, ftell, fflush, feof, ferror, fgetc, fgets, ungetc
- **Memory files**: `test_system_readmem*.v`, `test_system_writemem*.v` - hex, binary, wide
- **String formatting**: `test_system_sformat*.v`, `test_system_sscanf*.v`
- **VCD control**: `test_system_dump*.v` - dumpfile, dumpvars, dump control
- **Time**: `test_system_time*.v` - time, stime, realtime, timeformat, printtimescale
- **Math**: `test_system_signed.v`, `test_system_unsigned.v`, `test_system_rtoi.v`, `test_system_realtobits.v`
- **Utility**: `test_system_random.v`, `test_system_urandom.v`, `test_system_bits.v`, `test_system_clog2.v`, `test_system_size.v`, `test_system_dimensions.v`
- **Plusargs**: `test_system_test_plusargs.v`, `test_system_value_plusargs.v`
- **Control**: `test_system_finish.v`, `test_system_stop.v`

### Hierarchy & Modules (15+ tests)
- **Module instantiation**: `test_instance_chain.v`, `test_nested_modules.v`, `test_recursive_module.v`
- **Ports**: `test_inout_*.v`, `test_empty_port_connection.v`, `test_port_expression.v`, `test_unconnected_*.v`
- **Hierarchical names**: `test_hierarchical_name.v`
- **Parameters**: `test_parameter_*.v`, `test_defparam*.v` (10 tests) - basic, hierarchical, priority, scope resolution

### Preprocessor (7 tests)
- **Directives**: `test_define.v`, `test_ifdef.v`, `test_include.v`, `test_resetall.v`
- **Attributes**: `test_attribute.v`, `test_celldefine.v`, `test_protect.v`

### Special Features (20+ tests)
- **Parallel blocks**: `test_parallel_block.v`, `test_fork_join.v`
- **Force/release**: `test_force_*.v`
- **Named blocks**: `test_named_block.v`
- **Sensitivity**: `test_sensitivity_*.v`, `test_always_star.v`
- **Default nettype**: `test_default_nettype.v`, `test_implicit_net.v`
- **Timescale**: `test_timescale.v`, `test_time.v`
- **Specify blocks**: `test_specify*.v` (3 tests)
- **Width handling**: `test_width_mismatch.v`, `test_large_width.v`, `test_zero_width_param.v`, `test_unsized_vs_sized.v`
- **Multi-file**: `test_multifile_*.v`
- **Edge cases**: `test_overshift.v`, `test_no_whitespace.v`, `test_comb_loop.v`, `test_combinational_race.v`

---

## SystemVerilog Tests (`verilog/systemverilog/`) - 19 files

Tests for SystemVerilog-specific features. **Most expected to fail** as metalfpga targets Verilog-2001/2005.

| Test | Feature | Expected |
|------|---------|----------|
| `test_always_comb.v` | always_comb block | FAIL |
| `test_always_ff.v` | always_ff block | FAIL |
| `test_assertion.v` | SVA assertions | FAIL |
| `test_associative_array.v` | Associative arrays | FAIL |
| `test_dynamic_array.v` | Dynamic arrays | FAIL |
| `test_enum.v` | Enumerated types | FAIL |
| `test_foreach.v` | foreach loop | FAIL |
| `test_interface.v` | Interfaces | FAIL |
| `test_logic_type.v` | logic type | FAIL |
| `test_package.v` | Packages | FAIL |
| `test_priority_if.v` | priority if | FAIL |
| `test_queue.v` | Queues | FAIL |
| `test_real_array_bounds.v` | Real array bounds checking | TBD |
| `test_streaming_operator.v` | Streaming operators | FAIL |
| `test_struct.v` | Struct types | FAIL |
| `test_unique_case.v` | unique case | FAIL |
| `test_wildcard_equality.v` | Wildcard equality (===?) | FAIL |
| `test_x_propagation.v` | X-propagation semantics | TBD |
| `test_z_propagation.v` | Z-propagation semantics | TBD |

---

## Test Annotations

Many tests include `// EXPECT=PASS` or `// EXPECT=FAIL` comments to indicate expected outcomes. These annotations are used by test harnesses to validate compiler behavior.

---

## Running Tests

Tests can be run using:
```bash
# Run individual test
./build/metalfpga_cli verilog/pass/test_generate.v

# Run test suite
./test_runner.sh

# Run golden tests (IEEE compliance)
cd goldentests
./run_ieee_1364_2005_tests.sh   # Verilog-2005
./run_ieee_1800_2012_tests.sh   # SystemVerilog
```

---

## Test Coverage Summary

| Category | Count | Notes |
|----------|-------|-------|
| **Root tests** | 22 | Integration, VCD, real math |
| **Pass tests** | 363 | Comprehensive Verilog-2001/2005 coverage |
| **SystemVerilog tests** | 19 | Negative tests (expected failures) |
| **Total** | **404** | Full language validation |

### Feature Coverage Highlights
- ✅ **Real number support**: 20+ tests covering IEEE-754 math, CRlibm integration
- ✅ **4-state logic**: X/Z handling in case, tristate, strength resolution
- ✅ **Generate blocks**: 40+ tests including complex nesting and scoping
- ✅ **System tasks**: 60+ tests covering I/O, VCD, time, math functions
- ✅ **Timing & NBA**: Complete coverage of blocking/non-blocking semantics
- ✅ **Gates & primitives**: Comprehensive switch-level and gate-level support
- ✅ **Signal strength**: Full strength resolution and charge storage

---

## Notes

1. **Real number testing**: See `CRLIBM_ULP_COMPARE.md` for detailed ULP accuracy validation (99,999/100,000 bit-exact).
2. **VCD generation**: Multiple tests validate VCD waveform generation for Metal GPU debugging.
3. **4-state logic**: Tests cover X/Z propagation through gates, switches, and signal strength resolution.
4. **File I/O**: Extensive coverage of all Verilog file I/O system tasks including recent additions ($ftell, $rewind, $writemem*).
