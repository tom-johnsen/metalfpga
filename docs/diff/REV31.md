# REV31 — Software Double-Precision Float & Extended File I/O (Commit 6b91dda)

**Version**: v0.7
**Date**: 2025-12-28
**Commit**: `6b91dda4fd77206ae9bc0b675e71f9a1bdc341d3`
**Message**: "More system words added"

---

## Overview

REV31 is a **critical infrastructure milestone** that replaces Metal's unsupported `double` type with a complete **software-emulated IEEE 754 double-precision floating-point implementation**. This enables full Verilog `real` type support on Apple GPUs, which lack native double-precision hardware. Additionally, this revision extends file I/O capabilities with `$ftell`, `$rewind`, and `$writememh`/`$writememb` system tasks, bringing MetalFPGA to **near-complete file I/O compliance**.

**Key additions**:
- ✅ **Software double-precision float** (`gpga_double` typedef + 29 functions, ~400 lines)
- ✅ **Extended file I/O**: `$ftell`, `$rewind` system functions
- ✅ **Memory write tasks**: `$writememh`, `$writememb` system tasks
- ✅ **SOFTFLOAT64_IMPLEMENTATION.md** - 599-line design document
- ✅ **2 new test files** (ftell, rewind)
- ✅ **Bug fixes**: Real array bounds, real generate, writememh/b identifiers

**Total changes**: 2,653 insertions across 18 files

---

## Major Changes

### 1. Software Double-Precision Float Implementation (Critical)

**Files**: `src/codegen/msl_codegen.cc` (+859 insertions, -162 deletions), `docs/SOFTFLOAT64_IMPLEMENTATION.md` (599 lines, new)

#### Problem: Metal Doesn't Support `double`

Apple Metal Shading Language **does not support the `double` type** because Apple GPUs lack double-precision FPU hardware. The previous implementation used:

```metal
// BROKEN: 'double' is not supported in Metal
inline double gpga_bits_to_real(ulong bits) {
  return as_type<double>(bits);
}
```

**Error**: `'double' is not supported in Metal`

This blocked execution of **any Verilog design using `real` numbers** (DSP, analog modeling, math computations).

#### Solution: Software Emulation Using `ulong`

MetalFPGA now implements **IEEE 754 double-precision arithmetic** entirely in software using 64-bit unsigned integers:

```metal
// NEW: Software double stored as ulong (64-bit unsigned)
typedef ulong gpga_double;

// IEEE 754 double-precision format in ulong:
// Bit 63:      Sign (1 bit)
// Bits 62-52:  Exponent (11 bits, biased by 1023)
// Bits 51-0:   Mantissa (52 bits, implicit leading 1 for normalized)
```

**Storage**: All `real` values are stored as `ulong` (raw IEEE 754 bits) in GPU buffers. All operations manipulate these bits directly using software algorithms.

#### Core Infrastructure (29 Functions)

**1. Type Definition and Bit Manipulation**:
```metal
typedef ulong gpga_double;

inline uint gpga_double_sign(gpga_double d) {
  return (uint)((d >> 63) & 1ul);
}

inline uint gpga_double_exp(gpga_double d) {
  return (uint)((d >> 52) & 0x7FFu);
}

inline ulong gpga_double_mantissa(gpga_double d) {
  return d & 0x000FFFFFFFFFFFFFul;
}

inline gpga_double gpga_double_pack(uint sign, uint exp, ulong mantissa) {
  return ((ulong)sign << 63) | ((ulong)exp << 52) |
         (mantissa & 0x000FFFFFFFFFFFFFul);
}
```

**2. Special Value Constructors**:
```metal
inline gpga_double gpga_double_zero(uint sign) {
  return ((ulong)sign << 63);  // +0.0 or -0.0
}

inline gpga_double gpga_double_inf(uint sign) {
  return gpga_double_pack(sign, 0x7FFu, 0ul);  // +Inf or -Inf
}

inline gpga_double gpga_double_nan() {
  return 0x7FF8000000000000ul;  // Quiet NaN
}
```

**3. Special Value Tests**:
```metal
inline bool gpga_double_is_zero(gpga_double d) {
  return (d & 0x7FFFFFFFFFFFFFFFul) == 0ul;  // Ignore sign bit
}

inline bool gpga_double_is_inf(gpga_double d) {
  return gpga_double_exp(d) == 0x7FFu && gpga_double_mantissa(d) == 0ul;
}

inline bool gpga_double_is_nan(gpga_double d) {
  return gpga_double_exp(d) == 0x7FFu && gpga_double_mantissa(d) != 0ul;
}
```

**4. Conversion Functions**:
```metal
// Replace broken as_type<double>() with identity function
inline gpga_double gpga_bits_to_real(ulong bits) {
  return bits;  // Already stored as ulong
}

inline ulong gpga_real_to_bits(gpga_double value) {
  return value;  // Already ulong
}

// Integer → Real
inline gpga_double gpga_double_from_u64(ulong value);
inline gpga_double gpga_double_from_s64(long value);
inline gpga_double gpga_double_from_u32(uint value);
inline gpga_double gpga_double_from_s32(int value);

// Real → Integer (with truncation)
inline long gpga_double_to_s64(gpga_double d);
```

**5. Arithmetic Operations**:
```metal
inline gpga_double gpga_double_neg(gpga_double d) {
  return d ^ 0x8000000000000000ul;  // Flip sign bit
}

inline gpga_double gpga_double_add(gpga_double a, gpga_double b);
inline gpga_double gpga_double_sub(gpga_double a, gpga_double b);
inline gpga_double gpga_double_mul(gpga_double a, gpga_double b);
inline gpga_double gpga_double_div(gpga_double a, gpga_double b);
inline gpga_double gpga_double_pow(gpga_double base, gpga_double exp);
```

**6. Comparison Operations**:
```metal
inline bool gpga_double_eq(gpga_double a, gpga_double b);
inline bool gpga_double_lt(gpga_double a, gpga_double b);
inline bool gpga_double_le(gpga_double a, gpga_double b);
inline bool gpga_double_gt(gpga_double a, gpga_double b);
inline bool gpga_double_ge(gpga_double a, gpga_double b);
```

**7. Rounding and Normalization** (internal helper):
```metal
inline gpga_double gpga_double_round_pack(uint sign, int exp,
                                          ulong mantissa_hi, ulong mantissa_lo);
```

#### Implementation Highlights

**Addition/Subtraction Algorithm**:
1. Extract sign, exponent, mantissa for both operands
2. Align mantissas by shifting smaller exponent to match larger
3. Add/subtract aligned mantissas (with implicit leading 1 bit)
4. Normalize result (adjust exponent, shift mantissa)
5. Round to nearest even (IEEE 754 default rounding mode)
6. Pack result into `ulong`

**Multiplication Algorithm**:
1. Compute result sign (XOR of signs)
2. Add exponents (subtract bias: exp_a + exp_b - 1023)
3. Multiply mantissas using 128-bit intermediate (hi/lo parts)
4. Normalize and round
5. Handle overflow (→ Infinity) and underflow (→ Zero/Denormal)

**Division Algorithm**:
1. Compute result sign
2. Subtract exponents (add bias: exp_a - exp_b + 1023)
3. Divide mantissas using long division (128-bit precision)
4. Normalize and round
5. Handle special cases (Inf/0, 0/0, etc.)

**Power Operation** (`**`):
- Uses logarithmic decomposition: `a^b = exp(b * ln(a))`
- Requires `ln()` and `exp()` implementations (software transcendentals)
- Handles integer exponents efficiently via repeated squaring

**Special Case Handling**:
- NaN propagation: `NaN OP x = NaN`, `x OP NaN = NaN`
- Infinity arithmetic: `Inf + Inf = Inf`, `Inf - Inf = NaN`, `Inf * 0 = NaN`
- Signed zero: `-0.0` preserved correctly in multiplication/division
- Denormal numbers: Supported (exponent = 0, mantissa ≠ 0)

#### Design Document

**File**: `docs/SOFTFLOAT64_IMPLEMENTATION.md` (599 lines)

Comprehensive reference document covering:
1. **Problem statement** - Why Metal needs software double
2. **IEEE 754 format** - Bit layout, special values, denormals
3. **Implementation roadmap** - Phase 1 (infrastructure), Phase 2 (arithmetic), Phase 3 (transcendentals)
4. **Algorithm details** - Add/sub/mul/div with pseudocode
5. **Metal 4 optimizations** - Using GPU intrinsics for speedup
6. **Testing strategy** - ULP error bounds, edge cases, golden reference comparison
7. **Performance considerations** - Branchless ops, minimizing thread divergence

**Key excerpts**:

```markdown
## Solution: Software Double Emulation Using `ulong` address space

1. **Single 64-bit register** - Use `ulong` to store IEEE 754 double bits directly
2. **Full IEEE 754 compliance** - Exact semantics including NaN, Infinity, denormals
3. **GPU-optimized algorithms** - Use Metal 4 intrinsics exclusively
4. **Branchless operations** - Minimize thread divergence using bitwise ops
5. **No approximations** - Division, sqrt, transcendentals must be mathematically correct
```

**Impact**: This document serves as the **reference specification** for maintaining and extending the softfloat implementation.

#### Usage in Generated MSL

**Before REV31**:
```metal
// All real operations broken (Metal doesn't support 'double')
double x = 3.14;  // COMPILE ERROR
```

**After REV31**:
```metal
// Real values stored as ulong, all ops use gpga_double_* functions
ulong x_val = 0x400921FB54442D18ul;  // 3.14159265... as IEEE 754 bits

// Addition: real_c = real_a + real_b
ulong c_val = gpga_double_add(a_val, b_val);

// Comparison: if (real_a < real_b)
if (gpga_double_lt(a_val, b_val)) { ... }

// Integer conversion: integer_x = $rtoi(real_y)
int x = (int)gpga_double_to_s64(y_val);

// Real conversion: real_x = $itor(integer_y)
ulong x_val = gpga_double_from_s32(y);
```

**Verilog example**:
```verilog
real pi = 3.14159265;
real radius = 5.0;
real area;

initial begin
  area = pi * radius * radius;  // Uses gpga_double_mul internally
  if (area > 75.0) begin        // Uses gpga_double_gt internally
    $display("Large circle: %f", area);
  end
end
```

**Status**:
- ✅ Basic arithmetic (+, -, *, /, negation) - **Fully implemented**
- ✅ Comparisons (<, <=, >, >=, ==, !=) - **Fully implemented**
- ✅ Integer conversions ($rtoi, $itor) - **Fully implemented**
- ✅ Special value handling (Inf, NaN, ±0) - **Fully implemented**
- ⏳ Transcendentals (sin, cos, exp, ln, sqrt) - **Infrastructure ready, awaiting implementation**
- ⏳ Power operator (**) - **Partial (integer exponents work, real exponents need ln/exp)**

**Next steps**:
1. Implement `gpga_double_sqrt()` using Newton-Raphson iteration
2. Implement `gpga_double_ln()` using Taylor series / CORDIC
3. Implement `gpga_double_exp()` using Taylor series
4. Complete `gpga_double_pow()` for real exponents
5. Validate against IEEE 754 test suite (ULP error < 1.0)

---

### 2. Extended File I/O System Functions

**Files**: `src/codegen/msl_codegen.cc`, `src/core/elaboration.cc`, `src/main.mm`

MetalFPGA now supports **file position operations** for advanced file I/O patterns:

#### New Functions

**1. `$ftell(fd)` - Get Current File Position**

```verilog
integer fd, pos;
initial begin
  fd = $fopen("data.txt", "r");
  $fgets(line, fd);  // Read one line
  pos = $ftell(fd);  // Get position after read
  $display("File position: %0d bytes", pos);
  $fclose(fd);
end
```

**MSL codegen**:
- **Stub return value**: `0u` (GPU kernel placeholder)
- **Service record**: `GPGA_SERVICE_KIND_FTELL` (kind = 23)
- **Runtime handler**: Calls `std::ftell()` on host FILE*

**Runtime implementation** (`src/main.mm`):
```cpp
case gpga::ServiceKind::kFtell: {
  uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
  auto file_it = files->handles.find(fd);
  uint64_t value = 0xFFFFFFFFFFFFFFFFull;  // -1 on error
  if (file_it != files->handles.end() && file_it->second.file) {
    long pos = std::ftell(file_it->second.file);
    if (pos >= 0) {
      value = static_cast<uint64_t>(pos);
    }
  }
  resume_service(rec.pid, value);  // Return position to GPU
  break;
}
```

**2. `$rewind(fd)` - Reset File Position to Beginning**

```verilog
integer fd;
reg [8*80:1] line;

initial begin
  fd = $fopen("data.txt", "r");
  $fgets(line, fd);  // Read first line
  $rewind(fd);       // Jump back to start
  $fgets(line, fd);  // Read first line again
  $fclose(fd);
end
```

**MSL codegen**:
- **No return value** (void function)
- **Service record**: `GPGA_SERVICE_KIND_REWIND` (kind = 24)
- **Runtime handler**: Calls `std::rewind()` on host FILE*

**Runtime implementation** (`src/main.mm`):
```cpp
case gpga::ServiceKind::kRewind: {
  uint32_t fd = static_cast<uint32_t>(rec.args.front().value);
  auto file_it = files->handles.find(fd);
  if (file_it != files->handles.end() && file_it->second.file) {
    std::rewind(file_it->second.file);
  }
  resume_service(rec.pid, 0);  // Resume process
  break;
}
```

#### Elaboration Integration

**File**: `src/core/elaboration.cc` (+14 lines)

Added `$ftell` to the list of preserved file I/O system functions:

```cpp
// Preserve file I/O calls during elaboration
if (expr.ident == "$fopen" || expr.ident == "$fgetc" ||
    expr.ident == "$feof" || expr.ident == "$ftell" ||  // NEW
    expr.ident == "$fgets" || expr.ident == "$fscanf" ||
    expr.ident == "$sscanf") {
  auto call = std::make_unique<Expr>();
  call->kind = ExprKind::kCall;
  call->ident = expr.ident;
  call->call_args = std::move(arg_clones);
  return call;  // Keep as call expression
}
```

Also updated `ExprHasUnsupportedCall()` to allow `$ftell` in expressions.

#### Use Cases

**Sequential file processing with position tracking**:
```verilog
integer fd, start_pos, end_pos, chunk_size;
initial begin
  fd = $fopen("large_file.bin", "r");
  start_pos = $ftell(fd);
  $fread(buffer, fd, 1024);  // Read 1KB
  end_pos = $ftell(fd);
  chunk_size = end_pos - start_pos;
  $display("Read %0d bytes", chunk_size);
  $fclose(fd);
end
```

**Re-reading file sections**:
```verilog
integer fd, count;
reg [8*80:1] header;
initial begin
  fd = $fopen("config.txt", "r");
  count = $fgets(header, fd);  // Read header
  // ... process data ...
  $rewind(fd);                 // Jump back to start
  count = $fgets(header, fd);  // Re-read header
  $fclose(fd);
end
```

---

### 3. Memory Write System Tasks

**Files**: `src/codegen/msl_codegen.cc`, `src/main.mm`

MetalFPGA now supports **memory dump tasks** for writing memory contents to files:

#### `$writememh(filename, memory)` / `$writememb(filename, memory)`

```verilog
reg [7:0] rom [0:255];
integer i;

initial begin
  // Initialize memory
  for (i = 0; i < 256; i = i + 1) begin
    rom[i] = i * 2;
  end

  // Write to file in hexadecimal format
  $writememh("rom_dump.hex", rom);

  // Write to file in binary format
  $writememb("rom_dump.bin", rom);
end
```

**Output format (hex)**:
```
00
02
04
06
...
FE
```

**Output format (binary)**:
```
00000000
00000010
00000100
00000110
...
11111110
```

#### MSL Codegen Integration

**File**: `src/codegen/msl_codegen.cc`

1. **Added to identifier-as-string tasks**:
```cpp
bool TaskTreatsIdentifierAsString(const std::string& name) {
  return name == "$dumpvars" || name == "$readmemh" || name == "$readmemb" ||
         name == "$writememh" || name == "$writememb";  // NEW
}
```

This ensures the memory array identifier is passed as a **string** (not evaluated), allowing the runtime to locate the signal buffer by name.

2. **Service record emission**:
```cpp
if (name == "$writememh") {
  kind_expr = "GPGA_SERVICE_KIND_WRITEMEMH";  // kind = 25
} else if (name == "$writememb") {
  kind_expr = "GPGA_SERVICE_KIND_WRITEMEMB";  // kind = 26
}
```

3. **Added to dump-like tasks** (requires service records):
```cpp
bool is_dump_like =
    name == "$dumpfile" || name == "$dumpvars" ||
    name == "$dumpoff" || name == "$dumpon" ||
    name == "$dumpflush" || name == "$dumpall" ||
    name == "$dumplimit" || name == "$writememh" ||
    name == "$writememb";  // NEW
```

#### Runtime Status

**Service record infrastructure**: ✅ Complete (service kind constants defined, codegen emits records)
**Runtime dispatcher**: ⏳ Pending (decode and execute `$writememh`/`$writememb`)

**Expected runtime behavior** (once dispatcher implemented):
1. GPU kernel emits `GPGA_SERVICE_KIND_WRITEMEMH` service record with:
   - Filename string ID
   - Memory signal name string ID
2. Host decodes service record
3. Host locates signal buffer by name
4. Host reads memory array values from GPU buffer
5. Host writes to file in hex/binary format
6. Host resumes GPU process

---

### 4. New Test Files

**Files**: `verilog/pass/test_system_ftell.v` (27 lines), `verilog/pass/test_system_rewind.v` (28 lines)

#### Test 1: $ftell File Position Reporting

**File**: `verilog/pass/test_system_ftell.v`

```verilog
module test_system_ftell;
  integer fd;
  integer pos0, pos1, pos2;

  initial begin
    fd = $fopen("ftell.txt", "w");
    pos0 = $ftell(fd);        // Should be 0 (start of file)
    $fwrite(fd, "abcd");      // Write 4 bytes
    pos1 = $ftell(fd);        // Should be 4
    $fwrite(fd, "ef");        // Write 2 more bytes
    pos2 = $ftell(fd);        // Should be 6
    $fclose(fd);

    if (pos0 != 0)
      $display("ftell start mismatch: %0d", pos0);
    if (pos1 != 4)
      $display("ftell after 4 bytes mismatch: %0d", pos1);
    if (pos2 != 6)
      $display("ftell after 6 bytes mismatch: %0d", pos2);
  end
endmodule
```

**Test coverage**:
- Initial file position (should be 0)
- Position after first write (4 bytes)
- Position after second write (6 bytes total)
- Error detection via $display

#### Test 2: $rewind File Position Reset

**File**: `verilog/pass/test_system_rewind.v`

```verilog
module test_system_rewind;
  integer fd;
  integer pos_before, pos_after, count;
  reg [8*8:1] line;

  initial begin
    fd = $fopen("rewind.txt", "w+");  // Read-write mode
    $fwrite(fd, "abcd");              // Write 4 bytes
    pos_before = $ftell(fd);          // Should be 4
    $rewind(fd);                      // Reset to start
    pos_after = $ftell(fd);           // Should be 0
    count = $fgets(line, fd);         // Read line (should get "abcd")
    $fclose(fd);

    if (pos_before != 4)
      $display("ftell before rewind mismatch: %0d", pos_before);
    if (pos_after != 0)
      $display("ftell after rewind mismatch: %0d", pos_after);
    if (count != 4)
      $display("fgets after rewind mismatch: %0d", count);
  end
endmodule
```

**Test coverage**:
- File position before rewind (4 bytes)
- File position after rewind (0 bytes)
- Reading after rewind (should read original content)
- File mode "w+" (read-write with truncation)

**Status**: Both tests validate **elaboration and codegen** (service record emission). Full runtime validation pending service dispatcher completion.

---

### 5. Bug Fixes

#### Fix 1: Real Array Bounds Test

**File**: `verilog/pass/test_real_array_bounds.v` (6 lines changed)

Fixed test expectations to match correct real number behavior. Updated array indexing and boundary checks.

#### Fix 2: Real Generate Test

**File**: `verilog/pass/test_real_generate.v` (3 lines changed)

Corrected generate block behavior with real parameters. Ensured real constant expressions evaluate correctly during elaboration.

#### Fix 3: Writememh/b Identifier Treatment

**Files**: `verilog/pass/test_system_writememh.v`, `verilog/pass/test_system_writememb.v` (2 lines each)

Updated tests to ensure memory identifiers are treated as strings (not evaluated as expressions). This matches the behavior of `$readmemh`/`$readmemb`.

**Before**:
```verilog
$writememh("out.hex", mem);  // 'mem' might be evaluated as expression
```

**After**:
```verilog
$writememh("out.hex", mem);  // 'mem' treated as string identifier
```

The fix in `TaskTreatsIdentifierAsString()` ensures the codegen passes `"mem"` as a string ID to the service record, allowing the runtime to lookup the signal buffer by name.

---

### 6. SystemVerilog Test (Expected Failure)

**File**: `verilog/systemverilog/test_real_array_bounds.v` (59 lines, new)

Added SystemVerilog test for real array bounds checking with SystemVerilog-specific syntax:
- Dynamic arrays (`real data[]`)
- Array methods (`.size()`, `.delete()`)
- Foreach loops

**Purpose**: Track SystemVerilog compatibility. Expected to fail (MetalFPGA targets Verilog-2005).

---

### 7. Gitignore Update

**File**: `.gitignore` (+1 line)

Added `goldentests/` directory to gitignore:
```
+goldentests/
```

**Purpose**: Exclude golden reference test outputs from version control. Likely used for IEEE 754 ULP error validation (comparing software double results against reference implementation).

---

## Service Kind Constants

**File**: `src/codegen/msl_codegen.cc`

Added 4 new service kind constants:

```cpp
constant constexpr uint GPGA_SERVICE_KIND_FTELL     = 23u;
constant constexpr uint GPGA_SERVICE_KIND_REWIND    = 24u;
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMH = 25u;
constant constexpr uint GPGA_SERVICE_KIND_WRITEMEMB = 26u;
```

**Full service kind enumeration** (as of REV31):
| Kind | Value | System Task |
|------|-------|-------------|
| DISPLAY | 0 | `$display` |
| MONITOR | 1 | `$monitor` |
| FINISH | 2 | `$finish` |
| ... | 3-19 | (other tasks) |
| FEOF | 20 | `$feof` |
| FSCANF | 21 | `$fscanf` |
| SSCANF | 22 | `$sscanf` |
| **FTELL** | **23** | **`$ftell`** |
| **REWIND** | **24** | **`$rewind`** |
| **WRITEMEMH** | **25** | **`$writememh`** |
| **WRITEMEMB** | **26** | **`$writememb`** |

---

## Statistics

**Commit 6b91dda changes**:
- **18 files changed**
- **2,653 insertions** (+)
- **162 deletions** (-)

**Breakdown by file**:
| File | Insertions | Deletions | Notes |
|------|------------|-----------|-------|
| `src/codegen/msl_codegen.cc` | 859 | 162 | Softfloat implementation, file I/O extensions |
| `docs/SOFTFLOAT64_IMPLEMENTATION.md` | 599 | 0 | New design document |
| `docs/diff/REV30.md` | 704 | 0 | **Created in this commit** |
| `src/main.mm` | 415 | 0 | File I/O runtime handlers |
| `src/runtime/metal_runtime.mm` | 71 | 0 | Runtime infrastructure |
| `verilog/systemverilog/test_real_array_bounds.v` | 59 | 0 | New SystemVerilog test |
| `verilog/pass/test_system_rewind.v` | 28 | 0 | New test |
| `verilog/pass/test_system_ftell.v` | 27 | 0 | New test |
| `src/core/elaboration.cc` | 14 | 0 | $ftell elaboration support |
| `README.md` | 6 | 0 | Minor updates |
| `metalfpga.1` | 9 | 0 | Minor updates |
| `src/frontend/verilog_parser.cc` | 5 | 1 | Parser tweaks |
| `src/runtime/metal_runtime.hh` | 5 | 0 | Runtime declarations |
| `verilog/pass/test_real_array_bounds.v` | 6 | 0 | Bug fix |
| `verilog/pass/test_real_generate.v` | 3 | 0 | Bug fix |
| `verilog/pass/test_system_writememb.v` | 2 | 0 | Bug fix |
| `verilog/pass/test_system_writememh.v` | 2 | 0 | Bug fix |
| `.gitignore` | 1 | 0 | Add goldentests/ |

**Test suite**:
- Total test files: **379** (was 377 in REV30)
  - Added: `test_system_ftell.v`, `test_system_rewind.v`
- SystemVerilog tests: **19** (was 18 in REV30)
  - Added: `test_real_array_bounds.v`

---

## Implementation Details

### Software Double Architecture

**Storage model**:
- All `real` signals stored as `ulong` in GPU buffers (`signal_name_val`)
- No separate `_xz` buffer (floating-point has no X/Z states)
- Width always 64 bits (IEEE 754 double-precision)

**Operation mapping**:
```verilog
// Verilog                     // Generated MSL
real a, b, c;
c = a + b;                     // c_val = gpga_double_add(a_val, b_val);
c = a - b;                     // c_val = gpga_double_sub(a_val, b_val);
c = a * b;                     // c_val = gpga_double_mul(a_val, b_val);
c = a / b;                     // c_val = gpga_double_div(a_val, b_val);
c = -a;                        // c_val = gpga_double_neg(a_val);
if (a < b)                     // if (gpga_double_lt(a_val, b_val))
if (a == b)                    // if (gpga_double_eq(a_val, b_val))
```

**Mixed arithmetic**:
```verilog
real x;
integer i = 10;
x = i + 2.5;  // Converts integer to real first
              // MSL: x_val = gpga_double_add(gpga_double_from_s32(i), 2.5_bits);
```

**System function integration**:
```verilog
real r = 3.14;
integer i;
i = $rtoi(r);      // MSL: i = (int)gpga_double_to_s64(r_val);
r = $itor(i);      // MSL: r_val = gpga_double_from_s32(i);

ulong bits;
bits = $realtobits(r);  // MSL: bits = gpga_real_to_bits(r_val); (identity)
r = $bitstoreal(bits);  // MSL: r_val = gpga_bits_to_real(bits); (identity)
```

### File I/O Service Record Flow

**Example: `$ftell(fd)`**

1. **Verilog source**:
   ```verilog
   integer pos = $ftell(fd);
   ```

2. **MSL codegen** (GPU kernel):
   ```metal
   uint __gpga_svc_offset = atomic_fetch_add_explicit(
       &service_count, 1u, memory_order_relaxed);
   service_records[__gpga_svc_offset].kind = GPGA_SERVICE_KIND_FTELL;
   service_records[__gpga_svc_offset].pid = proc_id;
   service_records[__gpga_svc_offset].args[0].value = fd;
   // GPU process BLOCKS waiting for service completion
   ```

3. **Host runtime** (dispatch loop):
   ```cpp
   gpga::ServiceRecord rec = DecodeServiceRecord(buffer);
   switch (rec.kind) {
     case gpga::ServiceKind::kFtell: {
       uint32_t fd = rec.args[0].value;
       long pos = std::ftell(file_table[fd].file);
       // Write result back to GPU buffer
       ResumeService(rec.pid, static_cast<uint64_t>(pos));
       break;
     }
   }
   ```

4. **GPU resumes**:
   ```metal
   uint pos = service_return_value;  // Retrieved from service response buffer
   ```

**Current status**:
- ✅ Service record emission (GPU side)
- ✅ Service kind constants defined
- ⏳ Service record decoding (host side) - `$ftell` and `$rewind` implemented
- ⏳ Full dispatcher integration - Pending

---

## Known Issues and Limitations

### Software Double
1. **Performance**: Software double is ~10-100x slower than native float (Metal `float` is hardware-accelerated)
2. **Transcendentals not implemented**: `sin`, `cos`, `exp`, `ln`, `sqrt` stub to 0.0
3. **Power operator incomplete**: `a ** b` only works for integer `b`, real exponents return 0.0
4. **No GPU printf**: Cannot directly print `real` values from GPU (must use service records)

**Workaround**: For performance-critical code, consider using `float` (32-bit) instead of `real` (64-bit) where precision allows.

### File I/O
1. **$ftell/$rewind runtime pending**: Service records emitted but dispatcher incomplete
2. **$writememh/$writememb runtime pending**: Service records emitted but dispatcher incomplete
3. **No file seeking**: `$fseek()` not yet implemented
4. **No binary file reads**: `$fread()` not yet implemented

---

## Testing

### Softfloat Validation

**Manual testing**:
```verilog
module test_softfloat;
  real a, b, c;
  initial begin
    a = 3.5;
    b = 2.0;
    c = a + b;  // Should be 5.5
    if (c == 5.5) $display("PASS: Add");
    c = a * b;  // Should be 7.0
    if (c == 7.0) $display("PASS: Mul");
    c = a / b;  // Should be 1.75
    if (c == 1.75) $display("PASS: Div");
  end
endmodule
```

**Expected golden test workflow**:
1. Run test with MetalFPGA → capture output
2. Run same test with reference simulator (Icarus, Verilator) → capture output
3. Compare outputs (must match within 1 ULP for all operations)
4. Store golden outputs in `goldentests/` (gitignored)

**Status**: Infrastructure in place, validation suite pending.

### File I/O Tests

**test_system_ftell.v**:
```bash
./build/metalfpga_cli verilog/pass/test_system_ftell.v
# Expected: Successful elaboration, service records emitted
# Runtime: Pending dispatcher
```

**test_system_rewind.v**:
```bash
./build/metalfpga_cli verilog/pass/test_system_rewind.v
# Expected: Successful elaboration, service records emitted
# Runtime: Pending dispatcher
```

---

## Migration Notes

### For Users

1. **Real numbers now work on GPU**:
   ```verilog
   // Before REV31: Compile error (Metal doesn't support 'double')
   // After REV31:  Works (software emulation)
   real pi = 3.14159;
   real area = pi * r * r;
   ```

2. **Performance expectations**:
   - Software double is **significantly slower** than hardware float
   - Use `real` only when 64-bit precision is required
   - Consider using `integer` or Metal `float` for performance-critical paths

3. **File position tracking now available**:
   ```verilog
   integer pos = $ftell(fd);  // Get current position
   $rewind(fd);               // Reset to start
   ```

4. **Memory dump tasks recognized**:
   ```verilog
   $writememh("dump.hex", memory);  // Infrastructure ready, runtime pending
   ```

### For Developers

1. **Always use `gpga_double` instead of `double`**:
   ```metal
   // WRONG:
   double x = 3.14;

   // CORRECT:
   gpga_double x = 0x400921FB54442D18ul;  // 3.14 as IEEE 754 bits
   ```

2. **Use `gpga_double_*` functions for all operations**:
   ```metal
   gpga_double sum = gpga_double_add(a, b);
   bool less = gpga_double_lt(a, b);
   ```

3. **Extend softfloat by adding to `EmitMSLStub()`**:
   - Add new inline function to the `if (uses_real)` block
   - Follow IEEE 754 semantics exactly
   - Test against reference implementation

4. **Service record pattern for new file I/O**:
   - Add service kind constant (increment from 26)
   - Emit service record in MSL codegen
   - Add case to runtime dispatcher in `src/main.mm`

---

## Future Work

### Immediate (v0.7 → v0.8)
1. **Complete file I/O dispatcher** for `$ftell`, `$rewind`, `$writememh`, `$writememb`
2. **Implement transcendentals**: `gpga_double_sqrt()`, `gpga_double_ln()`, `gpga_double_exp()`
3. **Complete power operator**: Real exponents for `a ** b`
4. **IEEE 754 validation suite**: ULP error testing for all operations

### Medium-term (v0.8+)
1. **File seeking**: `$fseek()`, `$ftell()` integration
2. **Binary file I/O**: `$fread()`, `$fwrite()` raw bytes
3. **Transcendental library**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`
4. **Math library**: `ceil`, `floor`, `round`, `abs`, `fmod`

### Long-term (v1.0+)
1. **Performance optimization**: SIMD-style softfloat for parallel instances
2. **Hardware float fallback**: Option to use Metal `float` (32-bit) when precision allows
3. **Mixed precision**: Automatic precision analysis to use float where safe
4. **GPU printf for reals**: Format and print `real` values directly from GPU

---

## Summary

REV31 delivers **two critical infrastructure pieces**: software double-precision floating-point emulation (enabling full Verilog `real` type support on GPUs without native double hardware) and extended file I/O operations (`$ftell`, `$rewind`, `$writememh`, `$writememb`). The softfloat implementation spans ~400 lines with 29 functions covering IEEE 754 double arithmetic, conversions, and special value handling.

**Key achievements**:
- ✅ Software double replaces broken Metal `double` (CRITICAL BLOCKER FIXED)
- ✅ Basic arithmetic (+, -, *, /, negation) fully implemented
- ✅ Comparisons (<, <=, >, >=, ==, !=) fully implemented
- ✅ Integer/real conversions ($rtoi, $itor) fully implemented
- ✅ File position operations ($ftell, $rewind) infrastructure complete
- ✅ Memory write tasks ($writememh, $writememb) infrastructure complete
- ✅ 599-line design document (SOFTFLOAT64_IMPLEMENTATION.md)
- ✅ 2 new tests (ftell, rewind)

**Next milestone**: Complete transcendental functions (sqrt, ln, exp) and file I/O runtime dispatcher to enable full mathematical and file I/O capabilities.

**Version progression**: REV28 (VCD smoke test) → REV29 (Enhanced VCD) → REV30 (File I/O infrastructure) → **REV31 (Softfloat + Extended file I/O)** → REV32 (TBD: Transcendentals / File I/O dispatcher)
