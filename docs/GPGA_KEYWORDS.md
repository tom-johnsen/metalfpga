# GPGA Keywords Reference

This document catalogs all `gpga_*` keywords used in the metalfpga codebase with brief descriptions.

## Runtime Helper Functions

### Power Operations
- **gpga_pow_u32** - Unsigned 32-bit power operation (base^exp)
- **gpga_pow_u64** - Unsigned 64-bit power operation (base^exp)
- **gpga_pow_s32** - Signed 32-bit power operation with negative exponent handling
- **gpga_pow_s64** - Signed 64-bit power operation with negative exponent handling

### Real Number Core Operations
Basic conversion and arithmetic functions for software double-precision floating point:

- **gpga_bits_to_real** - Convert 64-bit unsigned integer bits to IEEE 754 double
- **gpga_real_to_bits** - Convert IEEE 754 double to 64-bit unsigned integer bits
- **gpga_double** - Typedef for ulong representing IEEE 754 double precision float
- **gpga_double_from_u32** - Convert unsigned 32-bit integer to double
- **gpga_double_from_u64** - Convert unsigned 64-bit integer to double
- **gpga_double_from_s32** - Convert signed 32-bit integer to double
- **gpga_double_from_s64** - Convert signed 64-bit integer to double
- **gpga_double_to_s32** - Convert double to signed 32-bit integer (with saturation)
- **gpga_double_to_s64** - Convert double to signed 64-bit integer (with saturation)
- **gpga_double_neg** - Negate a double value
- **gpga_double_add** - Add two double values
- **gpga_double_sub** - Subtract two double values
- **gpga_double_mul** - Multiply two double values
- **gpga_double_div** - Divide two double values
- **gpga_double_pow** - Power operation for double values
- **gpga_double_eq** - Equality comparison for double values
- **gpga_double_lt** - Less-than comparison for double values
- **gpga_double_gt** - Greater-than comparison for double values
- **gpga_double_le** - Less-than-or-equal comparison for double values
- **gpga_double_ge** - Greater-than-or-equal comparison for double values
- **gpga_double_is_zero** - Check if double value is zero
- **gpga_double_is_nan** - Check if double value is NaN
- **gpga_double_is_inf** - Check if double value is infinity

### Real Number Math Library Functions
Implemented in [src/msl/gpga_real_lib.metal](../src/msl/gpga_real_lib.metal) and included dynamically as needed:

#### Elementary Functions
- **gpga_double_sqrt** - Square root
- **gpga_double_floor** - Floor (round down to integer)
- **gpga_double_ceil** - Ceiling (round up to integer)
- **gpga_double_abs** - Absolute value

#### Exponential and Logarithmic Functions
- **gpga_double_exp_real** - Natural exponential (e^x)
- **gpga_double_expm1** - e^x - 1 (for small x)
- **gpga_double_ln** - Natural logarithm
- **gpga_double_log10** - Base-10 logarithm
- **gpga_double_log2** - Base-2 logarithm

#### Trigonometric Functions
- **gpga_double_sin** - Sine
- **gpga_double_cos** - Cosine
- **gpga_double_tan** - Tangent
- **gpga_double_asin** - Arcsine
- **gpga_double_acos** - Arccosine
- **gpga_double_atan** - Arctangent

#### Hyperbolic Functions
- **gpga_double_sinh** - Hyperbolic sine
- **gpga_double_cosh** - Hyperbolic cosine

#### Advanced Power and Utility Functions
- **gpga_double_pow_int** - Integer power operation
- **gpga_double_frexp** - Break into mantissa and exponent
- **gpga_double_ldexp** - Multiply by power of 2

### Wide Integer Operations (>64-bit)
Generated functions for arbitrary-width integers using uint2, uint3, uint4, etc. vectors.
The `_N` suffix represents the bit width (e.g., `gpga_wide_add_96` for 96-bit addition).

#### Wide Construction and Conversion
- **gpga_wide_zero_N** - Create zero value for N-bit wide integer
- **gpga_wide_mask_const_N** - Create all-ones mask for N-bit wide integer
- **gpga_wide_from_u64_N** - Convert 64-bit value to N-bit wide integer
- **gpga_wide_from_words_N** - Construct N-bit wide integer from component words
- **gpga_wide_to_u64_N** - Convert N-bit wide integer to 64-bit value (truncate)
- **gpga_wide_mask_N** - Apply bit mask to N-bit wide integer
- **gpga_wide_resize_N_from_M** - Zero-extend or truncate M-bit to N-bit wide integer
- **gpga_wide_sext_N_from_M** - Sign-extend M-bit to N-bit wide integer
- **gpga_wide_sext_from_u64_N** - Sign-extend from 64-bit to N-bit with dynamic source width

#### Wide Bitwise Operations
- **gpga_wide_not_N** - Bitwise NOT for N-bit wide integer
- **gpga_wide_and_N** - Bitwise AND for N-bit wide integers
- **gpga_wide_or_N** - Bitwise OR for N-bit wide integers
- **gpga_wide_xor_N** - Bitwise XOR for N-bit wide integers

#### Wide Arithmetic Operations
- **gpga_wide_add_N** - Addition for N-bit wide integers with carry propagation
- **gpga_wide_sub_N** - Subtraction for N-bit wide integers with borrow propagation
- **gpga_wide_mul_N** - Multiplication for N-bit wide integers (shift-and-add)
- **gpga_wide_div_N** - Unsigned division for N-bit wide integers (long division)
- **gpga_wide_mod_N** - Unsigned modulo for N-bit wide integers

#### Wide Shift Operations
- **gpga_wide_shl_N** - Logical left shift for N-bit wide integers
- **gpga_wide_shr_N** - Logical right shift for N-bit wide integers
- **gpga_wide_sar_N** - Arithmetic right shift for N-bit wide integers

#### Wide Comparison Operations
- **gpga_wide_eq_N** - Equality comparison for N-bit wide integers
- **gpga_wide_ne_N** - Inequality comparison for N-bit wide integers
- **gpga_wide_lt_u_N** - Unsigned less-than for N-bit wide integers
- **gpga_wide_gt_u_N** - Unsigned greater-than for N-bit wide integers
- **gpga_wide_le_u_N** - Unsigned less-or-equal for N-bit wide integers
- **gpga_wide_ge_u_N** - Unsigned greater-or-equal for N-bit wide integers
- **gpga_wide_lt_s_N** - Signed less-than for N-bit wide integers
- **gpga_wide_gt_s_N** - Signed greater-than for N-bit wide integers
- **gpga_wide_le_s_N** - Signed less-or-equal for N-bit wide integers
- **gpga_wide_ge_s_N** - Signed greater-or-equal for N-bit wide integers

#### Wide Utility Operations
- **gpga_wide_any_N** - Check if any bit is set in N-bit wide integer
- **gpga_wide_get_bit_N** - Extract single bit from N-bit wide integer
- **gpga_wide_set_bit_N** - Set single bit in N-bit wide integer
- **gpga_wide_signbit_N** - Extract sign bit from N-bit wide integer
- **gpga_wide_select_N** - Conditional select between two N-bit wide integers

#### Wide Reduction Operations
- **gpga_wide_red_and_N** - Reduction AND for N-bit wide integer
- **gpga_wide_red_or_N** - Reduction OR for N-bit wide integer
- **gpga_wide_red_xor_N** - Reduction XOR for N-bit wide integer

#### Wide Power Operations
- **gpga_wide_pow_u_N** - Unsigned power operation for N-bit wide integers
- **gpga_wide_pow_s_N** - Signed power operation for N-bit wide integers

### Double Precision Internals
Internal helper functions for software double-precision floating point implementation.
Most of these are used internally by the gpga_real library.

#### IEEE 754 Field Manipulation
- **gpga_double_sign** - Extract sign bit from double
- **gpga_double_exp** - Extract exponent from double
- **gpga_double_exponent** - Extract biased exponent
- **gpga_double_mantissa** - Extract mantissa from double
- **gpga_double_pack** - Pack sign, exponent, mantissa into double
- **gpga_double_zero** - Create zero with specified sign
- **gpga_double_inf** - Create infinity with specified sign
- **gpga_double_nan** - Create NaN value
- **gpga_double_round_pack** - Round and pack normalized significand
- **gpga_double_round_to_s64** - Round double to signed 64-bit integer

#### Low-Level Arithmetic Helpers
- **gpga_clz64** - Count leading zeros in 64-bit value
- **gpga_shift_right_sticky** - Shift right with sticky bit for rounding
- **gpga_shift_right_sticky_128** - Shift right 128-bit value with sticky bit
- **gpga_mul_64** - 64-bit multiplication producing 128-bit result
- **gpga_double_fma** - Fused multiply-add
- **gpga_double_negate** - Flip sign bit
- **gpga_double_next_up** - Next representable value upward
- **gpga_double_next_down** - Next representable value downward
- **gpga_double_raw_inc** - Increment raw bit pattern
- **gpga_double_raw_dec** - Decrement raw bit pattern

#### Double Precision Math Constants
- **gpga_double_const_one** - Constant 1.0
- **gpga_double_const_two** - Constant 2.0
- **gpga_double_const_minus_one** - Constant -1.0
- **gpga_double_const_pi** - Constant π
- **gpga_double_const_half_pi** - Constant π/2
- **gpga_double_const_quarter_pi** - Constant π/4
- **gpga_double_const_two_pi** - Constant 2π
- **gpga_double_const_inv_two_pi** - Constant 1/(2π)
- **gpga_double_const_ln2** - Constant ln(2)
- **gpga_double_const_ln10** - Constant ln(10)
- **gpga_double_const_log10e** - Constant log₁₀(e)
- **gpga_double_const_inv_ln2** - Constant 1/ln(2)
- **gpga_double_const_invN** - Constants 1/N for various factorial-related N values

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
- **__gpga_drive_** - Prefix for drive strength tracking variables

### Control Flow Temporaries
- **__gpga_sw_a** - Temporary variable 'a' for strength-weighted resolution switches
- **__gpga_sw_b** - Temporary variable 'b' for strength-weighted resolution switches
- **__gpga_sw_val** - Temporary resolved value for switch
- **__gpga_sw_xz** - Temporary resolved X/Z bits for switch
- **__gpga_sw_drive** - Temporary resolved drive strength for switch
- **__gpga_sw_diff_a** - Switch difference value A (for strength comparison)
- **__gpga_sw_diff_b** - Switch difference value B (for strength comparison)

### Loop and Repetition
- **__gpga_rep** - Loop counter for repeat statement iterations
- **__gpga_rep_active** - Flag indicating repeat loop is active
- **__gpga_rep_count** - Total iteration count for repeat statement
- **__gpga_rep_left** - Remaining iterations in repeat loop
- **__gpga_rep_slot** - Repeat statement slot allocation

### Service Call Management
- **__gpga_svc_index** - Index counter for scheduler service call queue
- **__gpga_svc_offset** - Offset into service call array for current entry

### Monitor Variables
- **__gpga_mon_** - Prefix for monitor block tracking variables
- **__gpga_wide_word** - Iterator for wide value word processing in monitors
- **__gpga_wide_val** - Wide value temporary in monitor processing
- **__gpga_wide_xz** - Wide X/Z bits temporary in monitor processing
- **__gpga_wide_mask** - Wide value mask temporary
- **__gpga_wide_slot** - Wide value slot index
- **__gpga_wide_base** - Wide value base index

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
- **__gpga_cond** - Conditional expression temporary for edge detection

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
- **__gpga_dnba_out** - Output index for NBA value compaction
- **__gpga_dnba_slot** - NBA queue slot allocation
- **__gpga_dnba_time** - Scheduled execution time for NBA
- **__gpga_dnba_write** - Write index counter for NBA queue compaction
- **__gpga_dval** - Delayed assignment value bits
- **__gpga_dxz** - Delayed assignment X/Z bits
- **__gpga_didx_val** - Delayed assignment index value bits
- **__gpga_didx_xz** - Delayed assignment index X/Z bits

## Strobe Management

- **__gpga_strobe_count** - Counter for pending strobe system task executions

## Trireg (Charge Storage) Variables

- **__gpga_trireg_decay_** - Prefix for trireg capacitive decay tracking
- **__gpga_trireg_drive_** - Prefix for trireg drive strength tracking

## Drive Resolution Variables

- **__gpga_didx** - Driver index for multi-driver resolution

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

## Special/Test Keywords

- **gpga_smoke** - Test kernel name for runtime validation ([src/tools/metal_smoke.mm](../src/tools/metal_smoke.mm))

## Notes

### Naming Conventions
- Prefix `__gpga_` (double underscore): Internal temporary variables and constants
- Prefix `gpga_` (single underscore): Runtime helper functions and constants
- Suffix pattern `_val`, `_xz`, `_drive`: Four-state logic value, X/Z bits, and drive strength components
- Suffix `_N` in wide functions: N represents the bit width (e.g., `gpga_wide_add_128` for 128-bit addition)
- Suffix `_u` or `_s`: Unsigned or signed variants (e.g., `gpga_wide_lt_u_N` vs `gpga_wide_lt_s_N`)
- Suffix `_rd`, `_rn`, `_ru`, `_rz`: Rounding modes (round down, round nearest, round up, round toward zero) for advanced math functions

### Usage Context
Most of these keywords are generated by the MSL (Metal Shading Language) code generator in [src/codegen/msl_codegen.cc](../src/codegen/msl_codegen.cc). They represent internal implementation details of the Verilog-to-Metal compilation process for GPU-accelerated simulation.

### Four-State Logic
The four-state system represents Verilog's 4-valued logic (0, 1, X, Z) using two bit vectors:
- `val`: The value bits (0 or 1)
- `xz`: The X/Z bits (indicates unknown or high-impedance)

Combined, these encode: `0` = (val:0, xz:0), `1` = (val:1, xz:0), `X` = (val:0, xz:1), `Z` = (val:1, xz:1)

### Wide Integer Implementation
For Verilog signals wider than 64 bits, the compiler generates specialized functions using Metal's vector types (uint2, uint3, uint4, etc.). These functions implement multi-word arithmetic, shifts, and comparisons. The functions are generated on-demand for each required width and are emitted into the generated Metal shader source code.

### Software Floating Point
Metal does not support native double-precision floating point on all hardware. The compiler implements IEEE 754 double precision using software emulation with 64-bit integer operations (`gpga_double` typedef). This enables Verilog `real` type support across all Metal-capable GPUs.

The comprehensive math library implementation is in [include/gpga_real.h](../include/gpga_real.h) (665 KB, 17,113 lines) and is wrapped by [src/msl/gpga_real_lib.metal](../src/msl/gpga_real_lib.metal) for dynamic loading. It implements correctly-rounded elementary functions based on the CRlibm (Correctly Rounded mathematical library) algorithms.

### Dynamic Library Loading
The real number library (`gpga_real_lib.metal`) is compiled to a dynamic Metal library (`gpga_real.metallib`, ~8.4 MB) and linked at runtime when needed. This keeps kernel sizes small when real arithmetic is not used - the library is only loaded when Verilog code actually uses `real` types or math system functions like `$sin`, `$cos`, `$sqrt`, etc.
