# GPGA Keywords Reference

This document catalogs all `gpga_*` keywords used in the metalfpga codebase with brief descriptions.

## Runtime Helper Functions

### Power Operations
- **gpga_pow_u32** - Unsigned 32-bit power operation (base^exp)
- **gpga_pow_u64** - Unsigned 64-bit power operation (base^exp)
- **gpga_pow_s32** - Signed 32-bit power operation with negative exponent handling
- **gpga_pow_s64** - Signed 64-bit power operation with negative exponent handling

### Real Number Conversion
- **gpga_bits_to_real** - Convert 64-bit unsigned integer bits to IEEE 754 double
- **gpga_real_to_bits** - Convert IEEE 754 double to 64-bit unsigned integer bits

## Scheduler Infrastructure

### Scheduler Index and Metadata
- **gpga_sched_index** - Calculate scheduler array index from grid ID and process ID
- **gpga_proc_parent** - Constant array mapping process IDs to their parent process IDs
- **gpga_proc_join_tag** - Constant array of join tags for process synchronization

## Temporary Variable Prefixes

### Four-State Temporaries
- **__gpga_fs_tmp** - Prefix for temporary variables in four-state logic expressions

### Drive/Resolution Temporaries
- **__gpga_drv_** - Prefix for driver value/xz/drive strength variables in wire resolution
- **__gpga_res_** - Prefix for resolved value/xz/drive variables after wire resolution
- **__gpga_partial_** - Prefix for partial assignment intermediate values

### Control Flow Temporaries
- **__gpga_sw_a** - Temporary variable 'a' for strength-weighted resolution switches
- **__gpga_sw_b** - Temporary variable 'b' for strength-weighted resolution switches
- **__gpga_sw_m** - Temporary mask variable 'm' for strength-weighted resolution switches

### Loop and Repetition
- **__gpga_rep** - Loop counter for repeat statement iterations

### Service Call Management
- **__gpga_svc_index** - Index counter for scheduler service call queue
- **__gpga_svc_offset** - Offset into service call array for current entry

### Monitor Variables
- **__gpga_mon_** - Prefix for monitor block tracking variables

## Edge Detection Temporaries

- **__gpga_edge_base** - Base index into edge detection arrays
- **__gpga_edge_idx** - Index for single edge detection entry
- **__gpga_edge_val** - Current value bits for edge comparison
- **__gpga_edge_xz** - Current X/Z bits for edge comparison
- **__gpga_edge_star_base** - Base index for event control star (any change) detection arrays
- **__gpga_edge_mask** - Bit mask identifying which bits had edge transitions
- **__gpga_prev_val** - Previous value bits for edge detection comparison
- **__gpga_prev_xz** - Previous X/Z bits for edge detection comparison
- **__gpga_prev_zero** - Previous bits that were logic 0
- **__gpga_prev_one** - Previous bits that were logic 1
- **__gpga_prev_unk** - Previous bits that were X or Z (unknown)
- **__gpga_curr_val** - Current value bits for edge detection
- **__gpga_curr_xz** - Current X/Z bits for edge detection
- **__gpga_curr_zero** - Current bits that are logic 0
- **__gpga_curr_one** - Current bits that are logic 1
- **__gpga_curr_unk** - Current bits that are X or Z (unknown)
- **__gpga_any** - Boolean flag indicating any edge was detected
- **__gpga_changed** - Boolean flag indicating signal changed (for event star)

## Timing and Delay

- **__gpga_time** - Current simulation time variable (64-bit unsigned)
- **__gpga_delay** - Temporary variable for delay calculation in timing control
- **__gpga_delay_slot** - Delay slot index for timing control arrays

## Non-Blocking Assignment (NBA) Queue

- **__gpga_dnba_base** - Base index into delayed NBA queue arrays
- **__gpga_dnba_count** - Counter for delayed NBA entries
- **__gpga_dnba_i** - Iterator index for processing NBA queue
- **__gpga_dnba_id** - NBA entry identifier
- **__gpga_dnba_idx** - Index for NBA array access
- **__gpga_dnba_out** - Output variable for NBA value retrieval
- **__gpga_dnba_slot** - NBA queue slot allocation
- **__gpga_dnba_time** - Scheduled execution time for NBA
- **__gpga_dnba_write** - Flag indicating NBA write operation

## Repeat Statement Management

- **__gpga_rep_active** - Flag indicating repeat loop is active
- **__gpga_rep_count** - Total iteration count for repeat statement
- **__gpga_rep_left** - Remaining iterations in repeat loop
- **__gpga_rep_slot** - Repeat statement slot allocation

## Strobe Management

- **__gpga_strobe_count** - Counter for pending strobe system task executions

## Trireg (Charge Storage) Variables

- **__gpga_trireg_decay_** - Prefix for trireg capacitive decay tracking
- **__gpga_trireg_drive_** - Prefix for trireg drive strength tracking

## Drive Resolution Variables

- **__gpga_didx** - Driver index for multi-driver resolution
- **__gpga_didx_val** - Driver value bits at index
- **__gpga_didx_xz** - Driver X/Z bits at index
- **__gpga_dval** - Resolved driver value result
- **__gpga_dxz** - Resolved driver X/Z result
- **__gpga_sw_diff_a** - Switch difference value A (for strength comparison)
- **__gpga_sw_diff_b** - Switch difference value B (for strength comparison)
- **__gpga_sw_drive** - Switch drive strength value
- **__gpga_sw_val** - Switch resolved value bits
- **__gpga_sw_xz** - Switch resolved X/Z bits

## Four-State Logic Helper Functions
(Note: These are defined in `include/gpga_4state.h` but may be referenced by generated code)

### Bitwise Operations (referenced but not typically emitted as identifiers)
- **gpga_and** - Four-state bitwise AND (inferred from fs_and32/fs_and64)
- **gpga_or** - Four-state bitwise OR (inferred from fs_or32/fs_or64)
- **gpga_xor** - Four-state bitwise XOR (inferred from fs_xor32/fs_xor64)
- **gpga_add** - Four-state addition (inferred from fs_add32/fs_add64)

## Runtime Data Structures

### Parameter Structs (Metal Kernel Arguments)
- **GpgaParams** - Basic simulation parameters (instance count)
- **GpgaSchedParams** - Scheduler configuration (max_steps, max_proc_steps, service_capacity)

### Module and Signal Metadata
- **ModuleInfo** - Module metadata (name, four_state flag, signal list)
- **SignalInfo** - Signal descriptor (name, width, array_size, is_real, is_trireg)
- **SchedulerConstants** - Compile-time scheduler configuration (proc_count, event_count, edge_count, etc.)
- **BufferSpec** - Buffer allocation specification (name, length)

### Service Record Infrastructure
- **ServiceKind** - Enum for system task types (kDisplay, kMonitor, kFinish, kDumpfile, kDumpvars, kReadmemh, kReadmemb, kStop, kStrobe, kDumpoff, kDumpon, kDumpflush, kDumpall, kDumplimit, kFwrite)
- **ServiceArgKind** - Enum for service argument types (kValue, kIdent, kString)
- **ServiceStringTable** - String table for $display format strings and identifiers
- **ServiceArgView** - Service call argument view (kind, width, value, xz)
- **ServiceRecordView** - Service record view (kind, pid, format_id, args)
- **ServiceDrainResult** - Result of draining service queue (saw_finish, saw_stop, saw_error)

### Metal Runtime Objects (C++ API)
- **MetalBuffer** - RAII GPU buffer wrapper (handle, contents pointer, length)
- **MetalBufferBinding** - Kernel argument binding (index, buffer, offset)
- **MetalKernel** - Compiled kernel wrapper (pipeline, argument_table, buffer_indices, thread execution width)
- **MetalRuntime** - Metal 4 runtime manager (device, queue, compiler)

## Special/Unused Keywords
(Found in search but not actively used in current codebase)

- **gpga_lut_lookup** - Reserved for lookup table operations (not currently used)
- **gpga_ram_read** - Reserved for RAM read operations (not currently used)
- **gpga_ram_write** - Reserved for RAM write operations (not currently used)
- **gpga_rc_charge** - RC charging kernel for analog simulation (referenced in docs/ANALOG.md)
- **gpga_runtime** - Reserved for runtime system calls (not currently used)
- **gpga_shift_left** - Reserved for shift operations (not currently used)
- **gpga_std** - Reserved for standard library functions (not currently used)
- **gpga_smoke** - Test kernel name for runtime validation (src/tools/metal_smoke.mm)

## Notes

### Naming Conventions
- Prefix `__gpga_` (double underscore): Internal temporary variables and constants
- Prefix `gpga_` (single underscore): Runtime helper functions and constants
- Suffix pattern `_val`, `_xz`, `_drive`: Four-state logic value, X/Z bits, and drive strength components

### Usage Context
Most of these keywords are generated by the MSL (Metal Shading Language) code generator in [src/codegen/msl_codegen.cc](../src/codegen/msl_codegen.cc). They represent internal implementation details of the Verilog-to-Metal compilation process for GPU-accelerated simulation.

### Four-State Logic
The four-state system represents Verilog's 4-valued logic (0, 1, X, Z) using two bit vectors:
- `val`: The value bits (0 or 1)
- `xz`: The X/Z bits (indicates unknown or high-impedance)

Combined, these encode: `0` = (val:0, xz:0), `1` = (val:1, xz:0), `X` = (val:0, xz:1), `Z` = (val:1, xz:1)