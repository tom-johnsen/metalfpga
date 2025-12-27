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

## Strobe Management

- **__gpga_strobe_count** - Counter for pending strobe system task executions

## Four-State Logic Helper Functions
(Note: These are defined in `include/gpga_4state.h` but may be referenced by generated code)

### Bitwise Operations (referenced but not typically emitted as identifiers)
- **gpga_and** - Four-state bitwise AND (inferred from fs_and32/fs_and64)
- **gpga_or** - Four-state bitwise OR (inferred from fs_or32/fs_or64)
- **gpga_xor** - Four-state bitwise XOR (inferred from fs_xor32/fs_xor64)
- **gpga_add** - Four-state addition (inferred from fs_add32/fs_add64)

## Special/Unused Keywords
(Found in search but not actively used in current codebase)

- **gpga_lut_lookup** - Reserved for lookup table operations (not currently used)
- **gpga_ram_read** - Reserved for RAM read operations (not currently used)
- **gpga_ram_write** - Reserved for RAM write operations (not currently used)
- **gpga_rc_charge** - RC charging kernel for analog simulation (referenced in docs/ANALOG.md)
- **gpga_runtime** - Reserved for runtime system calls (not currently used)
- **gpga_shift_left** - Reserved for shift operations (not currently used)
- **gpga_std** - Reserved for standard library functions (not currently used)

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