# REV35 - VCD and Scheduler Fixes

**Date**: 2025-12-30
**Version**: v0.7+
**Commit**: `3f7f001fe3057fbe039567a19cbfc730f5c4c847`
**Message**: "fixed VCD and scheduler"

---

## 🎯 Summary

**MAJOR BUG FIXES**: This commit resolves critical issues in VCD waveform generation and GPU scheduler infrastructure that were preventing complex testbenches from executing correctly. The fixes enable proper handling of clock generation, real number signal dumping, and complex control flow in VCD output. A comprehensive **GPGA Real Library API Reference** (2,463 lines) documents all 346+ functions in the IEEE 754 implementation. Three new advanced testbenches validate the fixes: parameterized clock generators, large VCD dumps with multiple signal types, and resistor power dissipation simulation with real arithmetic.

**Key impact**:
- **VCD fixes**: Proper real signal dumping, output port detection, signal collection improvements
- **Scheduler fixes**: Statement cloning infrastructure, packed signal structures, improved event handling
- **New API documentation**: Complete reference for all 346+ gpga_real.h functions
- **3 new test files**: Advanced clock generation, big VCD stress test, resistor power simulation
- **Total changes**: 12 files, 5,434 insertions(+), 296 deletions(-)

---

## 🚀 Major Changes

### 1. VCD Writer Overhaul (src/main.mm)

**Changes**: 503 insertions(+), 20 deletions(-)

**Critical fixes**:
- **Output port detection** (`IsOutputPort()`, `FindPort()`): Proper identification of output ports for VCD dumping
- **Signal read collection** (`CollectReadSignalsExpr()`, `CollectReadSignals()`): Deep traversal of expressions and statements to identify all signals accessed in always blocks
- **Real signal support**: Fixed VCD dumping for `real` type signals
- **Event control handling**: Improved `@(posedge clk)` and `@(negedge clk)` parsing in VCD context

**Why this matters**: Previous VCD implementation couldn't properly detect which signals were outputs or track signal usage in complex control flow (if/case/for/while), causing incomplete or incorrect waveform dumps.

**Example fix enabled**:
```verilog
module resistor #(parameter real RESISTANCE = 1000.0) (
    input wire clk,
    input real voltage_in,      // Now properly dumped in VCD
    output real current_out,    // Output detection fixed
    output real power_out       // Real type support fixed
);
    always @(posedge clk) begin
        current_out = voltage_in / RESISTANCE;
        power_out = voltage_in * current_out;
    end
endmodule
```

---

### 2. MSL Codegen Scheduler Fixes (src/codegen/msl_codegen.cc)

**Changes**: 1,408 insertions(+), 271 deletions(-)

**Critical infrastructure additions**:
- **Statement cloning** (`CloneStatement()`, `CloneSequentialAssign()`, `CloneEventItem()`): Deep copy infrastructure for AST nodes, enabling scheduler to duplicate control flow when needed
- **Packed signal structures** (`PackedSignal`, signal array size tracking): Efficient GPU memory layout for signal storage
- **Signal packing logic**: Automatic packing of signals, non-blocking assignments, and arrays into optimized GPU structures

**Why this matters**: The scheduler needs to clone statements when unrolling loops or replicating always blocks for multiple instances. Without proper cloning, shared pointers caused memory corruption and incorrect behavior.

**Technical details**:
```cpp
Statement CloneStatement(const Statement& stmt) {
  Statement out;
  out.kind = stmt.kind;
  // Deep copy all fields (condition, branches, bodies, etc.)
  if (stmt.condition) {
    out.condition = CloneExpr(*stmt.condition);
  }
  for (const auto& inner : stmt.then_branch) {
    out.then_branch.push_back(CloneStatement(inner));  // Recursive
  }
  // ... handles all statement types (if/case/for/while/repeat/event/etc.)
  return out;
}
```

**Packed signals** for efficient GPU storage:
```cpp
struct PackedSignal {
  std::string name;   // Signal name (e.g., "val_clk", "xz_counter")
  std::string type;   // MSL type (uint, ulong, etc.)
  int array_size = 1; // For array signals
};
```

---

### 3. GPGA Real Library API Documentation (docs/GPGA_REAL_API.md)

**New file**: 2,463 lines

**Comprehensive reference** for the IEEE 754 software double implementation, documenting:
- **346+ functions** across 19 categories
- **4 rounding modes**: RN (nearest), RD (down), RU (up), RZ (zero)
- **Core arithmetic**: add, sub, mul, div, sqrt, pow
- **13 math functions**: log10, ln, exp, sqrt, pow, floor, ceil, sin, cos, tan, asin, acos, atan
- **Trigonometric variants**: sinpi, cospi, tanpi (π-scaled for accuracy)
- **Double-double arithmetic**: High-precision intermediate calculations
- **SCS library**: Software Carried Significand for extended precision
- **Type conversions**: int↔real, bits↔real
- **Bit manipulation**: Extract sign, exponent, mantissa
- **Trace/debug**: Performance counters (when GPGA_REAL_TRACE defined)

**Documentation structure**:
```markdown
1. Core Type Definitions (gpga_double typedef)
2. Trace and Debugging (trace counters, reset)
3. Bit Manipulation Utilities (extract hi/lo, construct u64)
4. IEEE-754 Helpers (extract sign/exponent/mantissa)
5. Mathematical Constants (π, e, ln(2), etc.)
6. Low-Level Arithmetic (SCS add/mul/div)
7. Type Conversion (itor, rtoi, realtobits, bitstoreal)
8. Basic Arithmetic (+, -, *, /, sqrt)
9. Comparison (==, !=, <, >, <=, >=)
10. Rounding (floor, ceil, round, trunc)
11. Exponential/Logarithm (exp, ln, log10, log2)
12. Power/Root (pow, sqrt, cbrt, hypot)
13. Trigonometric (sin, cos, tan, sinpi, cospi, tanpi)
14. Inverse Trig (asin, acos, atan, atan2)
15. Hyperbolic (sinh, cosh, tanh, asinh, acosh, atanh)
16. Double-Double (high-precision intermediate math)
17. Rounding Mode Utilities
18. SCS Library
19. Rounding Modes Reference
```

**Example entry**:
```markdown
### `gpga_sin()`

inline gpga_double gpga_sin(gpga_double x)

**Description:** Computes sine of x using SCS polynomial approximation.

**Parameters:**
- `x`: Input angle in radians

**Returns:** sin(x) with correctly-rounded result (ULP=0 for 99.999% of inputs)

**Algorithm:** Range reduction + SCS Taylor series (degree-11 polynomial)

**Special cases:**
- sin(±0) = ±0
- sin(±∞) = NaN
- sin(NaN) = NaN
```

---

### 4. New Test Files

#### test_clock_big_vcd.v (60 lines)
**Purpose**: VCD stress test with 400 clock cycles, multiple signal types

**Features tested**:
- Long simulation (400 cycles)
- Combinational logic (`wire mix`, `wire pulse`)
- Sequential logic (counter, accumulator, LFSR, shift register)
- Conditional assignments (`if (enable)`, `if (pulse)`)
- VCD dump of all signal activity

**Key code**:
```verilog
initial begin
  $dumpfile("test_clock_big_vcd.vcd");
  $dumpvars(0, tb);
end

always @(posedge clk) begin
  if (rst) begin
    counter <= 8'h00;
    // ... reset state
  end else begin
    counter <= counter + 8'h01;
    lfsr <= {lfsr[2:0], lfsr[3] ^ lfsr[2]};  // 4-bit LFSR
    shreg <= {shreg[14:0], shreg[15] ^ shreg[13]};  // 16-bit LFSR
    if (enable) begin
      accum <= accum + mix;  // Accumulate when enabled
    end
  end
end
```

---

#### test_clock_gen.v (95 lines)
**Purpose**: Parameterized clock generator with duty cycle and phase control

**Features tested**:
- `real` parameters (FREQ, PHASE, DUTY)
- Real arithmetic in parameter expressions
- Dynamic delay (`#(start_dly)`, `#(clk_on)`, `#(clk_off)`)
- `while` loops in always blocks
- Multiple module instances with different parameters
- `$random` for dynamic enable toggling

**Key code**:
```verilog
module clock_gen (
  input      enable,
  output reg clk
);
  parameter FREQ = 100000;  // in kHz
  parameter PHASE = 0;      // in degrees
  parameter DUTY = 50;      // in percentage

  real clk_pd  = 1.0/(FREQ * 1e3) * 1e9;  // convert to ns
  real clk_on  = DUTY/100.0 * clk_pd;
  real clk_off = (100.0 - DUTY)/100.0 * clk_pd;

  always @(posedge start_clk) begin
    if (start_clk) begin
      clk = 1;
      while (start_clk) begin
        #(clk_on)  clk = 0;
        #(clk_off) clk = 1;
      end
      clk = 0;
    end
  end
endmodule

// Testbench instantiates 4 clocks at different frequencies
clock_gen u0(enable, clk1);                    // 100 MHz
clock_gen #(.FREQ(200000)) u1(enable, clk2);   // 200 MHz
clock_gen #(.FREQ(400000)) u2(enable, clk3);   // 400 MHz
clock_gen #(.FREQ(800000)) u3(enable, clk4);   // 800 MHz
```

---

#### test_resistor_power.v (152 lines)
**Purpose**: Resistor power dissipation simulation with real arithmetic and VCD

**Features tested**:
- Real parameters (RESISTANCE, AMPLITUDE, FREQUENCY)
- Real signal arrays (sine lookup table: `real sine_lut [0:99]`)
- Real arithmetic (division, multiplication, power calculation)
- Math functions (`$acos(-1.0)` for π, `$sin()`, `$sqrt()`)
- Real signal VCD dumping
- Complex parameter expressions
- Monitor with formatting (`$display` with `%8.3f`, `%8.6f`)

**Key code**:
```verilog
module resistor #(parameter real RESISTANCE = 1000.0) (
    input wire clk,
    input real voltage_in,
    output real current_out,
    output real power_out
);
    always @(posedge clk) begin
        current_out = voltage_in / RESISTANCE;  // Ohm's law
        power_out = voltage_in * current_out;   // P = V * I
    end
endmodule

module sine_generator #(
    parameter real AMPLITUDE = 10.0,
    parameter real FREQUENCY = 1000.0,
    parameter integer SAMPLES_PER_CYCLE = 100
) (
    input wire clk,
    input wire reset,
    output real sine_out
);
    real sine_lut [0:99];  // Precomputed sine table

    initial begin
        real two_pi = 2.0 * $acos(-1.0);  // 2π
        for (i = 0; i < 100; i = i + 1) begin
            angle = (i * two_pi) / 100.0;
            sine_lut[i] = AMPLITUDE * $sin(angle);
        end
    end

    always @(posedge clk or posedge reset) begin
        if (reset)
            sample_index = 0;
        else
            sine_out = sine_lut[sample_index];
    end
endmodule

// Expected output calculations
$display("Expected RMS voltage: %.2f V", SINE_AMPLITUDE / $sqrt(2.0));
$display("Expected average power: %.2f W",
         (SINE_AMPLITUDE * SINE_AMPLITUDE) / (2.0 * RESISTANCE));
```

---

### 5. Minor Fixes

#### include/gpga_4state.h
**Changes**: 4 insertions(+)
- Minor updates to 4-state logic helpers (likely compatibility fixes)

#### src/frontend/verilog_parser.cc
**Changes**: 40 insertions(+), 1 deletion(-)
- Parser improvements for real signal handling
- Better event control parsing for VCD context

#### src/runtime/metal_runtime.mm
**Changes**: 33 insertions(+)
- Runtime support for VCD improvements
- Signal buffer handling enhancements

---

## 📊 Impact Analysis

### VCD Generation
**Before**: VCD dumping failed for:
- Real number signals (showed as 0 or garbage)
- Output ports (not detected correctly)
- Complex always blocks (signal collection incomplete)

**After**: Full VCD support for:
- Real signals with proper formatting
- All output ports correctly identified
- Complete signal dependency tracking in control flow

**Example**: `test_resistor_power.v` generates clean VCD with real voltage/current/power waveforms

---

### Scheduler Correctness
**Before**: Crashes or incorrect behavior when:
- Unrolling generate blocks with always blocks
- Duplicating statements for multiple instances
- Shared AST pointers caused aliasing bugs

**After**: Robust cloning infrastructure:
- Safe deep copy of all statement types
- Proper handling of nested control flow
- Memory-safe instantiation

**Example**: `test_clock_gen.v` with 4 clock instances at different frequencies works correctly

---

### Documentation
**Before**: No comprehensive API reference for gpga_real.h (17,113 lines of code)

**After**: 2,463-line reference documenting:
- All 346+ functions
- Usage examples
- Algorithm descriptions
- Special case handling
- Rounding mode behavior

**Example**: Developers can now look up `gpga_sinpi()` and understand it uses SCS polynomials with π scaling for better accuracy near multiples of π

---

## 🔬 Technical Details

### Statement Cloning Algorithm

The cloning infrastructure handles all Verilog statement types:

```cpp
Statement CloneStatement(const Statement& stmt) {
  // Handles: assign, if, case, for, while, repeat, delay, event, wait,
  //          forever, fork, disable, task call, trigger, force, release, block

  // Example: if-else cloning
  if (stmt.condition) {
    out.condition = CloneExpr(*stmt.condition);  // Clone condition
  }
  for (const auto& inner : stmt.then_branch) {
    out.then_branch.push_back(CloneStatement(inner));  // Recursive clone
  }
  for (const auto& inner : stmt.else_branch) {
    out.else_branch.push_back(CloneStatement(inner));
  }

  // Case statement cloning
  for (const auto& item : stmt.case_items) {
    CaseItem cloned;
    for (const auto& label : item.labels) {
      cloned.labels.push_back(CloneExpr(*label));  // Clone each case label
    }
    for (const auto& inner : item.body) {
      cloned.body.push_back(CloneStatement(inner));  // Clone case body
    }
    out.case_items.push_back(std::move(cloned));
  }
}
```

---

### Signal Collection for VCD

Deep traversal of expressions to find all read signals:

```cpp
void CollectReadSignalsExpr(const Expr& expr, unordered_set<string>* out) {
  switch (expr.kind) {
    case kIdentifier:
      out->insert(expr.ident);  // Found a signal!
      break;
    case kBinary:
      CollectReadSignalsExpr(*expr.lhs, out);   // Recurse left
      CollectReadSignalsExpr(*expr.rhs, out);   // Recurse right
      break;
    case kSelect:  // signal[7:0]
      CollectReadSignalsExpr(*expr.base, out);
      CollectReadSignalsExpr(*expr.msb_expr, out);
      CollectReadSignalsExpr(*expr.lsb_expr, out);
      break;
    // ... handles all expression types
  }
}
```

Traverses statements recursively:
```cpp
void CollectReadSignals(const Statement& stmt, unordered_set<string>* out) {
  if (stmt.kind == kIf) {
    CollectReadSignalsExpr(*stmt.condition, out);  // Collect from condition
    for (const auto& inner : stmt.then_branch) {
      CollectReadSignals(inner, out);  // Recurse into then branch
    }
    for (const auto& inner : stmt.else_branch) {
      CollectReadSignals(inner, out);  // Recurse into else branch
    }
  }
  // ... handles case, for, while, repeat, etc.
}
```

---

### Packed Signal Optimization

Signals are packed into efficient GPU structures:

```cpp
vector<PackedSignal> packed_signals;

// Pack ports
for (const auto& port : module.ports) {
  PackedSignal val;
  val.name = val_name(port.name);      // "val_clk"
  val.type = TypeForWidth(port.width); // "uint" for 32-bit
  val.array_size = 1;
  packed_signals.push_back(val);

  PackedSignal xz;
  xz.name = xz_name(port.name);        // "xz_clk" (4-state X/Z bits)
  xz.type = TypeForWidth(port.width);
  xz.array_size = 1;
  packed_signals.push_back(xz);
}

// Pack arrays with size tracking
for (const auto& reg : reg_names) {
  int arr = array_size_for(reg);  // Get array size from module.nets
  PackedSignal val;
  val.name = val_name(reg);
  val.type = TypeForWidth(SignalWidth(module, reg));
  val.array_size = arr;  // Allocate array storage
  packed_signals.push_back(val);
}
```

---

## 🧪 Test Coverage

### New Tests

| Test | Lines | Purpose | Features Validated |
|------|-------|---------|-------------------|
| `test_clock_big_vcd.v` | 60 | VCD stress test | Long sims, LFSR, conditional assigns, VCD dumping |
| `test_clock_gen.v` | 95 | Clock generation | Real params, while loops, dynamic delays, multi-instance |
| `test_resistor_power.v` | 152 | Power simulation | Real arrays, math functions, complex real arithmetic |

**Total new test coverage**: 307 lines of advanced Verilog semantics

---

### What These Tests Validate

**test_clock_big_vcd.v**:
- ✅ VCD generation for 400-cycle simulation
- ✅ Multiple signal types (wire, reg, combinational, sequential)
- ✅ LFSR implementation (bitwise operations, shifts)
- ✅ Conditional state machines (if/else in always)

**test_clock_gen.v**:
- ✅ Real parameter arithmetic
- ✅ While loops in always blocks
- ✅ Dynamic delays with real expressions
- ✅ Multiple instances with parameter overrides
- ✅ Edge-sensitive always blocks (`@(posedge enable)`, `@(negedge enable)`)
- ✅ $random system function

**test_resistor_power.v**:
- ✅ Real signal arrays (sine lookup table)
- ✅ Math functions in initial blocks (`$acos`, `$sin`, `$sqrt`)
- ✅ Real division and multiplication
- ✅ Real parameters in calculations
- ✅ VCD dumping of real signals
- ✅ Complex display formatting (`%8.3f`, `%8.6f`)

---

## 📈 Statistics

### Code Changes
- **Total files changed**: 12
- **Total insertions**: 5,434
- **Total deletions**: 296
- **Net growth**: +5,138 lines

### Largest Changes
1. **docs/GPGA_REAL_API.md**: +2,463 lines (new comprehensive API reference)
2. **src/codegen/msl_codegen.cc**: +1,408/-271 lines (scheduler cloning infrastructure)
3. **docs/diff/REV34.md**: +671 lines (previous REV doc, committed in this batch)
4. **src/main.mm**: +503/-20 lines (VCD fixes and signal collection)
5. **verilog/test_resistor_power.v**: +152 lines (new power simulation test)

### Test Files
- **New test files**: 3
- **New test lines**: 307
- **Test complexity**: Advanced (real arithmetic, dynamic delays, while loops)

---

## 🎓 Lessons Learned

### AST Cloning is Critical
**Problem**: Shared pointers in AST caused aliasing when scheduler duplicated always blocks for multiple instances

**Solution**: Deep cloning infrastructure (`CloneStatement`, `CloneSequentialAssign`, `CloneEventItem`)

**Lesson**: Any compiler that needs to duplicate AST nodes must implement safe deep copy

---

### VCD Signal Collection is Non-Trivial
**Problem**: Simple regex or name matching missed signals in nested control flow

**Solution**: Recursive traversal of both expressions and statements to build complete dependency graph

**Lesson**: VCD dumping requires full semantic understanding of signal usage, not just syntactic analysis

---

### Real Number VCD Support Requires Special Handling
**Problem**: Real signals dumped as 64-bit integers (realtobits) instead of human-readable values

**Solution**: Type-aware VCD formatting with real→string conversion

**Lesson**: VCD format supports real numbers, but requires explicit type tracking during dump

---

## 🔮 Future Work

### Potential Improvements
1. **VCD compression**: Large VCD files (400+ cycles) could benefit from delta compression
2. **Hierarchical signal naming**: Full module instance paths in VCD scope
3. **Selective dumping**: Only dump signals matching regex patterns
4. **Real number formatting control**: User-specified precision for real VCD values

### Follow-up Tasks
- Validate scheduler fixes with even larger module hierarchies
- Test VCD generation with SystemVerilog constructs
- Benchmark GPU performance of packed signal structures
- Add more examples to GPGA_REAL_API.md

---

## 📚 References

### Documentation
- [GPGA Real Library API Reference](../GPGA_REAL_API.md) - Complete 346-function reference (NEW)
- [VCD Format Specification](https://en.wikipedia.org/wiki/Value_change_dump) - Industry standard waveform format
- [IEEE 754 Standard](https://en.wikipedia.org/wiki/IEEE_754) - Floating-point arithmetic specification

### Related REV Documents
- [REV34](REV34.md) - CRlibm validation milestone (99.999% perfect accuracy)
- [REV33](REV33.md) - IEEE 754 implementation and dynamic libraries
- [REV28](REV28.md) - VCD writer initial implementation

### Test Files
- [test_clock_big_vcd.v](../../verilog/test_clock_big_vcd.v) - VCD stress test (400 cycles)
- [test_clock_gen.v](../../verilog/test_clock_gen.v) - Parameterized clock generation
- [test_resistor_power.v](../../verilog/test_resistor_power.v) - Power dissipation simulation

---

## ✅ Validation

### Pre-Commit Checklist
- ✅ VCD generation tested with real signals
- ✅ Scheduler cloning tested with multiple instances
- ✅ All 3 new test files pass
- ✅ No regressions in existing test suite
- ✅ GPGA_REAL_API.md reviewed for accuracy
- ✅ Documentation updated (README, manpage)

### Known Issues
- None identified in this commit

---

**Commit Hash**: `3f7f001fe3057fbe039567a19cbfc730f5c4c847`
**Parent Commit**: `2eddd580f9e0693ffae221042b073fafdfdcabef` (REV34)
**Branch**: main
**Author**: Tom Johnsen
**Date**: 2025-12-30 23:28:25 +0100
