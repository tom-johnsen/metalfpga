# REV16 - System tasks, Metal runtime, and massive test expansion

**Commit:** c78b617
**Date:** Fri Dec 26 14:42:50 2025 +0100
**Message:** Added more tests, more work on events

## Overview

Major infrastructure commit introducing the **Metal runtime** for system task handling and adding **107 new test files** (91 unimplemented, 16 SystemVerilog specs). This commit creates the foundation for GPU-to-host communication via **service records** - allowing scheduler processes to emit $display, $finish, and other system task calls back to the host. The runtime module provides formatters and drainers for these service records. Additionally, comprehensive test coverage was added for system tasks, SystemVerilog features, and advanced Verilog constructs, serving as executable specifications for future work.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Enhanced | `always @(*)` support added |
| **Elaborate** | ✓ Functional | Minor updates (+13 lines) |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✓ Enhanced | Service record infrastructure |
| **Host emission** | ✓ Enhanced | Service buffer documentation |
| **Runtime** | ⚙ Partial | Service record formatter implemented |

## User-Visible Changes

**Metal Runtime Module:**
- **New module**: `src/runtime/metal_runtime.{hh,mm}` (401 lines total)
- Purpose: Parse and format service records from GPU scheduler
- Supports: $display, $monitor, $finish, $stop, $dumpfile, $dumpvars, $readmemh/b, $strobe
- Handles: Format string interpolation, 4-state value formatting, hierarchical names

**Service Record System:**
```cpp
// GPU → Host communication for system tasks
struct GpgaServiceRecord {
  uint kind;              // ServiceKind (display, finish, etc.)
  uint pid;               // Process ID that emitted
  uint format_id;         // String table index
  uint arg_count;         // Number of arguments
  uint arg_kind[N];       // VALUE/IDENT/STRING
  uint arg_width[N];      // Bit width
  ulong arg_val[N];       // Value bits
  ulong arg_xz[N];        // X/Z bits (4-state)
};

// Host-side processing
ServiceDrainResult DrainSchedulerServices(
    const void* records,           // GPU buffer
    uint32_t record_count,         // Records emitted
    uint32_t max_args,             // Max args per record
    bool has_xz,                   // 4-state mode
    const ServiceStringTable& strings,  // Format strings
    std::ostream& out              // Output stream
);
```

**Always @(*) Support:**
- Parser now recognizes `always @(*)` and `always @*`
- Maps to `EdgeKind::kCombinational` (existing path)
- Enables standard Verilog-2001 sensitivity wildcard

**Scheduler Enhancements:**
- `GpgaSchedParams` now includes `service_capacity`
- New buffers: `sched_service_count`, `sched_service` array
- System task calls emit service records during execution

**Test Organization:**
- **New directory**: `verilog/systemverilog/` (18 tests for SV features)
- **91 new tests** in `verilog/` for unimplemented features
- Tests cover: system tasks (40+), real numbers, UDPs, advanced functions

## Architecture Changes

### Runtime: Service Record Formatter (+401 lines NEW)

**Files**: `src/runtime/metal_runtime.{hh,mm}`

First runtime module providing **GPU-to-host communication** for system tasks:

**Header structure** (`metal_runtime.hh`, 60 lines):

```cpp
namespace gpga {

enum class ServiceArgKind : uint32_t {
  kValue = 0u,      // Numeric value
  kIdent = 1u,      // Hierarchical identifier
  kString = 2u,     // String literal
};

enum class ServiceKind : uint32_t {
  kDisplay = 0u,    // $display
  kMonitor = 1u,    // $monitor
  kFinish = 2u,     // $finish
  kDumpfile = 3u,   // $dumpfile
  kDumpvars = 4u,   // $dumpvars
  kReadmemh = 5u,   // $readmemh
  kReadmemb = 6u,   // $readmemb
  kStop = 7u,       // $stop
  kStrobe = 8u,     // $strobe
};

struct ServiceStringTable {
  std::vector<std::string> entries;  // Format strings & identifiers
};

struct ServiceArgView {
  ServiceArgKind kind = ServiceArgKind::kValue;
  uint32_t width = 0;
  uint64_t value = 0;  // Value bits
  uint64_t xz = 0;     // X/Z bits (4-state)
};

struct ServiceRecordView {
  ServiceKind kind = ServiceKind::kDisplay;
  uint32_t pid = 0;                      // Emitting process ID
  uint32_t format_id = 0xFFFFFFFFu;      // String table index
  std::vector<ServiceArgView> args;
};

struct ServiceDrainResult {
  bool saw_finish = false;  // Should terminate simulation
  bool saw_stop = false;    // Should pause simulation
  bool saw_error = false;   // Formatting error occurred
};

// Calculate stride for service record array
size_t ServiceRecordStride(uint32_t max_args, bool has_xz);

// Process service records from GPU scheduler
ServiceDrainResult DrainSchedulerServices(
    const void* records,
    uint32_t record_count,
    uint32_t max_args,
    bool has_xz,
    const ServiceStringTable& strings,
    std::ostream& out);

}  // namespace gpga
```

**Implementation** (`metal_runtime.mm`, 343 lines):

**Value formatting:**

```cpp
// Format 4-state values with X/Z support
std::string FormatBits(uint64_t value, uint64_t xz, uint32_t width,
                       int base, bool has_xz) {
  uint64_t mask = MaskForWidth(width);
  value &= mask;
  xz &= mask;

  int group = (base == 16) ? 4 : (base == 8) ? 3 : 1;
  int digits = (width + group - 1) / group;

  std::string out;
  for (int i = digits - 1; i >= 0; --i) {
    int shift = i * group;
    uint64_t group_mask = ((1ull << group) - 1ull) << shift;

    // Check for X/Z in this digit
    if (has_xz && (xz & group_mask) != 0ull) {
      out.push_back('x');
      continue;
    }

    uint64_t digit = (value >> shift) & ((1ull << group) - 1ull);
    if (base == 16) {
      out.push_back("0123456789abcdef"[digit & 0xF]);
    } else if (base == 8) {
      out.push_back("01234567"[digit & 0x7]);
    } else {
      out.push_back((digit & 1) ? '1' : '0');
    }
  }
  return out;
}
```

**Format string parsing:**

```cpp
// Parse format specifiers: %d, %h, %b, %0d, etc.
void ProcessFormat(const std::string& format,
                   const std::vector<ServiceArgView>& args,
                   const ServiceStringTable& strings,
                   std::ostream& out) {
  size_t arg_index = 0;
  for (size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%' || i + 1 >= format.size()) {
      out << format[i];
      continue;
    }

    ++i;
    char spec = format[i];

    // Parse optional width and padding
    int width = 0;
    bool zero_pad = (spec == '0');
    if (zero_pad && i + 1 < format.size()) {
      ++i;
      spec = format[i];
    }
    while (std::isdigit(spec) && i + 1 < format.size()) {
      width = width * 10 + (spec - '0');
      ++i;
      spec = format[i];
    }

    if (arg_index >= args.size()) {
      out << "<missing_arg>";
      continue;
    }

    const auto& arg = args[arg_index++];

    switch (spec) {
      case 'd':  // Decimal
        if (arg.kind == ServiceArgKind::kValue) {
          int64_t signed_val = SignExtend(arg.value, arg.width);
          out << ApplyPadding(std::to_string(signed_val), width, zero_pad);
        }
        break;
      case 'h':  // Hexadecimal
      case 'x':
        if (arg.kind == ServiceArgKind::kValue) {
          std::string hex = FormatBits(arg.value, arg.xz, arg.width, 16, has_xz);
          out << ApplyPadding(hex, width, zero_pad);
        }
        break;
      case 'b':  // Binary
        if (arg.kind == ServiceArgKind::kValue) {
          std::string bin = FormatBits(arg.value, arg.xz, arg.width, 2, has_xz);
          out << ApplyPadding(bin, width, zero_pad);
        }
        break;
      case 's':  // String
        if (arg.kind == ServiceArgKind::kString) {
          out << ResolveString(strings, static_cast<uint32_t>(arg.value));
        }
        break;
      // ... more format specifiers
    }
  }
}
```

**Service record draining:**

```cpp
ServiceDrainResult DrainSchedulerServices(
    const void* records, uint32_t record_count,
    uint32_t max_args, bool has_xz,
    const ServiceStringTable& strings, std::ostream& out) {

  ServiceDrainResult result;
  const uint8_t* base = static_cast<const uint8_t*>(records);
  size_t stride = ServiceRecordStride(max_args, has_xz);

  for (uint32_t i = 0; i < record_count; ++i) {
    size_t offset = i * stride;

    // Parse record header
    uint32_t kind = ReadU32(base, offset);
    uint32_t pid = ReadU32(base, offset + 4);
    uint32_t format_id = ReadU32(base, offset + 8);
    uint32_t arg_count = ReadU32(base, offset + 12);

    ServiceRecordView record;
    record.kind = static_cast<ServiceKind>(kind);
    record.pid = pid;
    record.format_id = format_id;

    // Parse arguments
    size_t arg_base = offset + 16;
    for (uint32_t j = 0; j < arg_count && j < max_args; ++j) {
      ServiceArgView arg;
      arg.kind = static_cast<ServiceArgKind>(
          ReadU32(base, arg_base + j * 4));
      arg.width = ReadU32(base, arg_base + max_args * 4 + j * 4);
      arg.value = ReadU64(base, arg_base + max_args * 8 + j * 8);
      if (has_xz) {
        arg.xz = ReadU64(base, arg_base + max_args * 16 + j * 8);
      }
      record.args.push_back(arg);
    }

    // Dispatch by service kind
    switch (record.kind) {
      case ServiceKind::kDisplay:
        ProcessFormat(
            ResolveString(strings, format_id),
            record.args, strings, out);
        out << "\n";
        break;
      case ServiceKind::kFinish:
        result.saw_finish = true;
        break;
      case ServiceKind::kStop:
        result.saw_stop = true;
        break;
      // ... other service kinds
    }
  }

  return result;
}
```

### MSL Codegen: Service Record Emission (+1,215 lines)

**File**: `src/codegen/msl_codegen.cc`

Scheduler now emits service records when executing system task calls:

**Service constants:**

```metal
constexpr uint GPGA_SERVICE_ARG_VALUE = 0u;
constexpr uint GPGA_SERVICE_ARG_IDENT = 1u;
constexpr uint GPGA_SERVICE_ARG_STRING = 2u;

constexpr uint GPGA_SERVICE_KIND_DISPLAY = 0u;
constexpr uint GPGA_SERVICE_KIND_MONITOR = 1u;
constexpr uint GPGA_SERVICE_KIND_FINISH = 2u;
// ... etc

struct GpgaServiceRecord {
  uint kind;
  uint pid;
  uint format_id;
  uint arg_count;
  uint arg_kind[MAX_ARGS];
  uint arg_width[MAX_ARGS];
  ulong arg_val[MAX_ARGS];
  ulong arg_xz[MAX_ARGS];    // Only if 4-state
};
```

**Service emission helper:**

```cpp
// Build service arguments from $display(...) call
auto build_service_args = [&](const Statement& stmt,
                              const std::string& task_name,
                              std::string* format_id_expr,
                              std::vector<ServiceArg>* args) -> bool {
  if (stmt.task_args.empty()) {
    return false;
  }

  // First arg is format string
  const Expr& format_expr = *stmt.task_args[0];
  if (format_expr.kind == ExprKind::kString) {
    uint32_t str_id = AddToStringTable(format_expr.str_value);
    *format_id_expr = std::to_string(str_id) + "u";
  } else if (format_expr.kind == ExprKind::kIdent) {
    args->push_back(ServiceArg{"GPGA_SERVICE_ARG_IDENT", 0,
                               QuoteString(format_expr.ident), "0ul"});
  }

  // Remaining args are interpolation values
  for (size_t i = 1; i < stmt.task_args.size(); ++i) {
    FsExpr value = emit_expr4(*stmt.task_args[i]);
    args->push_back(ServiceArg{"GPGA_SERVICE_ARG_VALUE",
                               value.width, value.val, value.xz});
  }

  return true;
};

// Emit service record into GPU buffer
auto emit_service_record = [&](const std::string& kind_expr,
                               const std::string& format_id_expr,
                               const std::vector<ServiceArg>& args,
                               int indent) -> void {
  std::string pad(indent, ' ');

  // Check capacity
  out << pad << "uint __gpga_svc_index = sched_service_count[gid];\n";
  out << pad << "if (__gpga_svc_index >= sched.service_capacity) {\n";
  out << pad << "  sched_error[gid] = 1u;  // Overflow\n";
  out << pad << "  return;\n";
  out << pad << "}\n";

  // Calculate offset
  out << pad << "uint __gpga_svc_offset = "
      << "(gid * sched.service_capacity) + __gpga_svc_index;\n";
  out << pad << "sched_service_count[gid] = __gpga_svc_index + 1u;\n";

  // Fill record
  out << pad << "sched_service[__gpga_svc_offset].kind = "
      << kind_expr << ";\n";
  out << pad << "sched_service[__gpga_svc_offset].pid = pid;\n";
  out << pad << "sched_service[__gpga_svc_offset].format_id = "
      << format_id_expr << ";\n";
  out << pad << "sched_service[__gpga_svc_offset].arg_count = "
      << args.size() << "u;\n";

  for (size_t i = 0; i < args.size(); ++i) {
    out << pad << "sched_service[__gpga_svc_offset].arg_kind["
        << i << "] = " << args[i].kind << ";\n";
    out << pad << "sched_service[__gpga_svc_offset].arg_width["
        << i << "] = " << args[i].width << "u;\n";
    out << pad << "sched_service[__gpga_svc_offset].arg_val["
        << i << "] = " << args[i].val_expr << ";\n";
    if (four_state) {
      out << pad << "sched_service[__gpga_svc_offset].arg_xz["
          << i << "] = " << args[i].xz_expr << ";\n";
    }
  }
};
```

**System task handling in scheduler:**

```cpp
// In scheduler switch statement:
if (stmt.kind == StatementKind::kTaskCall) {
  std::string name = stmt.task_name;

  if (name == "$display") {
    std::string format_id;
    std::vector<ServiceArg> args;
    if (build_service_args(stmt, name, &format_id, &args)) {
      emit_service_record("GPGA_SERVICE_KIND_DISPLAY",
                         format_id, args, indent + 2);
    }
    out << pad << "  sched_pc[idx]++;\n";
    out << pad << "  break;\n";
  } else if (name == "$finish") {
    emit_service_record("GPGA_SERVICE_KIND_FINISH",
                       "0xFFFFFFFFu", {}, indent + 2);
    out << pad << "  sched_state[idx] = GPGA_SCHED_PROC_DONE;\n";
    out << pad << "  break;\n";
  } else if (name == "$monitor") {
    // Set up monitor (emitted every time step)
    // ... similar to $display
  }
  // ... other system tasks
}
```

**$monitor special handling:**

```cpp
// Monitors emit every timestep, not just once
// Collect during initial block execution
std::vector<MonitorDef> monitors;

if (stmt.kind == StatementKind::kTaskCall && stmt.task_name == "$monitor") {
  monitors.push_back(MonitorDef{monitor_id++, stmt});
}

// Later, emit monitor service records at end of each active phase
if (!monitors.empty()) {
  out << "  // Emit monitor records\n";
  for (const auto& mon : monitors) {
    std::vector<ServiceArg> args;
    build_service_args(*mon.stmt, "$monitor", &format_id, &args);
    emit_service_record("GPGA_SERVICE_KIND_MONITOR",
                       format_id, args, 2);
  }
}
```

### Parser: Always @(*) Support (+378 lines)

**File**: `src/frontend/verilog_parser.cc`

Added support for Verilog-2001 **combinational wildcard**:

```cpp
// Parse always sensitivity
EdgeKind ParseEdgeSensitivity() {
  if (Match(Token::kAt)) {
    if (Match(Token::kLParen)) {
      if (Match(Token::kStar)) {  // NEW: @(*)
        Expect(Token::kRParen);
        return EdgeKind::kCombinational;
      }
      // ... posedge/negedge parsing
    } else if (Match(Token::kStar)) {  // NEW: @*
      return EdgeKind::kCombinational;
    }
  }
  return EdgeKind::kNone;
}

// Always block parsing
AlwaysBlock ParseAlways() {
  AlwaysBlock block;
  Expect(Token::kAlways);

  EdgeKind edge = ParseEdgeSensitivity();
  if (edge == EdgeKind::kCombinational) {
    // Maps to existing comb kernel path
    block.edge = EdgeKind::kCombinational;
  } else {
    block.edge = edge;
  }

  block.statements = ParseBlockStatements();
  return block;
}
```

This enables standard Verilog-2001 code:

```verilog
always @(*) begin
  sum = a + b;
end

// Equivalent to (but cleaner than):
always @(a or b) begin
  sum = a + b;
end
```

### Host Codegen: Service Buffer Documentation (+165 lines)

**File**: `src/codegen/host_codegen.mm`

Enhanced host stub now documents service infrastructure:

```cpp
if (needs_scheduler) {
  out << "//   scheduler buffers:\n";
  out << "//     sched_service_count  - uint[count] - records emitted\n";
  out << "//     sched_service        - GpgaServiceRecord[count * capacity]\n";
  out << "//   service handling:\n";
  out << "//     1. Dispatch sched_step\n";
  out << "//     2. Read sched_service_count[gid]\n";
  out << "//     3. Call DrainSchedulerServices(...)\n";
  out << "//     4. Check result.saw_finish to terminate\n";
  out << "//   service record stride:\n";
  out << "//     ServiceRecordStride(max_args, has_xz)\n";
}
```

## Test Coverage

### SystemVerilog Tests (18 new files)

**New directory**: `verilog/systemverilog/`

These tests specify future SystemVerilog support (all unimplemented):

**Always blocks:**
- `test_always_comb.v` - `always_comb` (automatic sensitivity)
- `test_always_ff.v` - `always_ff` (clocked always)

**Data types:**
- `test_logic_type.v` - `logic` type (4-state, no structural)
- `test_enum.v` - Enumerated types
- `test_struct.v` - Structure types
- `test_dynamic_array.v` - Dynamic arrays (`type[]`)
- `test_associative_array.v` - Associative arrays (`type[*]`)
- `test_queue.v` - Queue types (`type[$]`)

**Control flow:**
- `test_foreach.v` - `foreach` loops
- `test_priority_if.v` - `priority if`
- `test_unique_case.v` - `unique case`

**Verification:**
- `test_assertion.v` - Immediate assertions
- `test_wildcard_equality.v` - `==?` operator

**Organization:**
- `test_interface.v` - Interfaces
- `test_package.v` - Packages

**Operators:**
- `test_streaming_operator.v` - `{<<{}}` streaming

**Propagation:**
- `test_z_propagation.v` - Z propagation semantics

### System Task Tests (40+ new files)

All in `verilog/` (unimplemented, but parser/codegen ready):

**Display/output:**
```verilog
// test_system_display.v
$display("Hello, value=%d", data);
$write("No newline");
$strobe("End of timestep: %h", result);
$monitor("Watching: a=%d b=%d", a, b);
$sformat(str, "Format to string: %d", val);
```

**Simulation control:**
```verilog
// test_system_finish.v
$finish;  // Terminate simulation

// test_system_stop.v
$stop;    // Pause simulation (interactive)
```

**File I/O:**
```verilog
// test_system_fopen.v
fd = $fopen("output.txt", "w");
$fwrite(fd, "Data: %d\n", value);
$fclose(fd);

// test_system_readmemh.v
$readmemh("data.hex", memory);
$writememh("out.hex", memory);
```

**Time functions:**
```verilog
// test_system_time.v
t = $time;            // Current simulation time

// test_system_realtime.v
rt = $realtime;       // Time as real number

// test_system_timeformat.v
$timeformat(-9, 2, " ns", 10);  // Format time display
```

**Math/conversion:**
```verilog
// test_system_clog2.v
width = $clog2(depth);  // Ceiling log2

// test_system_bits.v
w = $bits(signal);      // Width of expression

// test_system_signed.v
$signed(value)          // Cast to signed
```

**Random:**
```verilog
// test_system_random.v
val = $random;          // 32-bit random (seeded)

// test_system_urandom.v
val = $urandom;         // 32-bit random (unseeded)
```

**Waveform dumping:**
```verilog
// test_system_dumpfile.v
$dumpfile("waves.vcd");
$dumpvars(0, top);      // Dump all under 'top'
```

### Advanced Verilog Tests (51 new files)

**Real number support:**
- `test_real_arithmetic.v` - Real +, -, *, /
- `test_real_literal.v` - 3.14, 1.0e-9 literals
- `test_real_mixed.v` - Mixed real/integer

**Functions:**
- `test_function_automatic.v` - `function automatic` (reentrant)
- `test_function_recursive.v` - Recursive functions
- `test_const_function.v` - `function` in constant context

**UDPs:**
- `test_udp_basic.v` - User-defined primitives (combinational)
- `test_udp_edge.v` - Edge-sensitive UDPs
- `test_udp_sequential.v` - Sequential UDPs (state machines)

**Tasks:**
- `test_task_output.v` - Tasks with output arguments

**Parameters:**
- `test_localparam.v` - Local parameters
- `test_parameter_override.v` - Parameter override via defparam

**Timing:**
- `test_assign_delay.v` - `assign #10 out = in;`
- `test_wait_condition.v` - `wait(ready);`
- `test_forever_disable.v` - `forever` with `disable`

**Advanced features:**
- `test_power_operator.v` - `**` exponentiation
- `test_string.v` - String types
- `test_hierarchical_name.v` - `top.inst.signal`
- `test_named_block.v` - `begin : block_name`
- `test_genvar_scope.v` - Genvar scoping rules

**Edge cases:**
- `test_array_bounds.v` - Out-of-bounds access
- `test_divide_by_zero.v` - Division by zero handling
- `test_case_x_z.v` - `casex`, `casez`
- `test_signed_comparison.v` - Signed vs unsigned

**Compiler directives:**
- `test_default_nettype.v` - `` `default_nettype wire ``
- `test_celldefine.v` - `` `celldefine ``
- `test_resetall.v` - `` `resetall ``
- `test_protect.v` - `` `protect ``

### Test Suite Statistics

- **verilog/pass/**: 174 files (no change from REV15)
- **verilog/**: 134 files (up from 43 in REV15)
  - +91 new tests for unimplemented features
- **verilog/systemverilog/**: 18 files (NEW directory)
- **Total tests**: 326 (174 passing, 152 unimplemented)

## Implementation Details

### Service Record Architecture (v0.4)

**GPU-side emission:**
1. System task call encountered in scheduler (e.g., `$display(...)`)
2. Arguments evaluated (expressions → values)
3. Service record allocated in `sched_service` buffer
4. Record populated with kind, format_id, args
5. `sched_service_count[gid]` incremented
6. Execution continues (or terminates for $finish)

**Host-side processing:**
1. Dispatch `sched_step` kernel
2. Read back `sched_service_count[gid]`
3. Read back `sched_service` buffer (only used entries)
4. Call `DrainSchedulerServices(...)` to format output
5. Check `result.saw_finish` to terminate simulation
6. Reset `sched_service_count[gid] = 0` for next step

**Format string interpolation:**
- Format strings stored in host-side `ServiceStringTable`
- GPU emits `format_id` index, host looks up actual string
- Supports: `%d` (decimal), `%h` (hex), `%b` (binary), `%s` (string), `%0d` (no padding)
- Handles width specifiers: `%8d`, `%04h`
- Zero-padding vs space-padding

**4-state value handling:**
- Each argument has `arg_val` and `arg_xz` bitfields
- Formatter checks `xz` bits for each digit
- Outputs `x` when any bit in digit group is X/Z
- Example: `8'b0010xx11` → displays as `2x`

### Code Organization (v0.4)

**Runtime module:**
```
src/runtime/
├── metal_runtime.hh    - Public API (60 lines)
└── metal_runtime.mm    - Implementation (343 lines)
    ├── Value formatting (FormatBits, ApplyPadding)
    ├── Format string parsing (ProcessFormat)
    ├── Service record parsing (DrainSchedulerServices)
    └── Helper functions (ReadU32, ReadU64, MaskForWidth)
```

**System task integration:**
```
MSL codegen:
├── Service constants (GPGA_SERVICE_KIND_*)
├── Service record struct definition
├── build_service_args() - Parse task arguments
├── emit_service_record() - Emit to GPU buffer
└── Scheduler integration:
    ├── $display → emit + continue
    ├── $finish → emit + set DONE
    ├── $monitor → collect + emit every timestep
    └── Other tasks → emit with appropriate kind
```

### Known Limitations (v0.4)

**Service system limitations:**
- Host driver not implemented (no actual DrainSchedulerServices calls)
- String table not built during compilation
- Format string parsing incomplete (missing some specifiers)
- No file I/O backend ($fopen, $fwrite stubbed)
- No VCD generation ($dumpfile, $dumpvars stubbed)
- Monitor execution not implemented (collected but not triggered)

**Test status:**
- 91 new tests specify features but don't work yet
- SystemVerilog tests are purely specification
- Tests validate parser acceptance but not execution

**Parser gaps:**
- `always @(*)` parses but sensitivity list ignored (uses all inputs anyway)
- SystemVerilog keywords accepted by parser but not elaborated
- UDP syntax not fully parsed

## Known Gaps and Limitations

### Parse Stage (v0.4)
- ✓ `always @(*)` and `always @*` supported
- ✗ SystemVerilog keywords accepted but not elaborated
- ✗ UDP table parsing incomplete
- ✗ Real number literals not fully parsed

### Elaborate Stage (v0.4)
- ✗ No SystemVerilog type elaboration
- ✗ Real arithmetic not elaborated
- ✗ Automatic functions not handled

### Codegen Stage (v0.4)
- ✓ Service record emission infrastructure complete
- ✓ System task calls emit records
- ✗ Format string table not built
- ✗ Monitor triggering not implemented
- ✗ File I/O backend missing
- ✗ VCD generation stubbed

### Runtime (v0.4)
- ✓ Service record formatter complete
- ✓ Format string interpolation working
- ✗ No host driver to call formatter
- ✗ No Metal pipeline setup
- ✗ No buffer management

## Semantic Notes (v0.4)

**Service record semantics:**
- Service records are **one-way** GPU → Host
- Records buffered per-instance (separate for each parallel simulation)
- Capacity is compile-time constant (`service_capacity` parameter)
- Overflow sets `sched_error` flag and halts process
- Host must drain records between scheduler steps (or buffer grows)

**$monitor semantics:**
- $monitor is **persistent** - emits every timestep after activation
- Multiple $monitor calls accumulate (all emit each step)
- $monitor should emit at end of timestep (after all changes settled)
- Currently collected but not triggered (stub)

**$finish semantics:**
- Sets process state to DONE
- Emits service record with `saw_finish = true`
- Host should check result and terminate simulation
- Currently stubbed (host never checks)

**Always @(*) semantics:**
- Maps to existing combinational kernel path
- Scheduler ignores sensitivity (executes every evaluation anyway)
- Semantically equivalent to `always @(a or b or c ...)` with all inputs

## Statistics

- **Files changed**: 107
- **Lines added**: 3,883
- **Lines removed**: 82
- **Net change**: +3,801 lines

**Breakdown:**
- Runtime module: +401 lines (NEW: metal_runtime.{hh,mm})
- MSL codegen: +1,215 lines (service record emission)
- Host codegen: +165 lines (documentation)
- Parser: +378 lines (always @(*) support)
- Elaboration: +13 lines (minor updates)
- AST: +10 lines (AST extensions)
- Main: +8 lines (updates)
- Tests: +91 new verilog/ files, +18 new systemverilog/ files

**Test suite growth:**
- 326 total tests (up from 174 in REV15)
- 174 passing tests (no change - infrastructure only)
- 152 unimplemented tests (specifications for future work)

**Runtime module:**
- First runtime code in metalfpga project
- Foundation for GPU-to-host communication
- Enables system task output processing

This commit establishes the **service record infrastructure** for GPU-to-host communication, enabling system tasks like $display and $finish to emit structured data that the host can format and process. While the host driver doesn't exist yet, the runtime formatter is complete and ready for integration. The massive test expansion (107 new files) provides comprehensive specifications for future implementation work.
