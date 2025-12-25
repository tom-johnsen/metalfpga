# 4-State Logic Implementation Guide

## Overview

This document outlines what needs to happen to extend metalfpga from 2-state logic (0/1) to 4-state logic (0/1/X/Z), enabling support for `casex`, `casez`, and full Verilog semantics.

**Current Status**: metalfpga uses pure 2-state logic throughout the compilation pipeline.

**Goal**: Support 4-state values where:
- `0` = logic low
- `1` = logic high
- `X` = unknown/uninitialized
- `Z` = high impedance

---

## Current Architecture (2-State)

### Value Flow Through Compiler
```
Verilog Source
    ↓
Parser → AST (uint64_t literals)
    ↓
Elaboration (int64_t constant folding)
    ↓
Flattened Module (2-state expressions)
    ↓
MSL Codegen (uint/ulong arithmetic)
    ↓
Metal Shader (native GPU operations)
```

### Current Assumptions
1. All literals are `uint64_t` (ast.hh:59)
2. All parameters are `int64_t` (elaboration.cc:112)
3. Comparisons return `0u` or `1u` only (msl_codegen.cc:447-452)
4. Unconnected inputs default to `0` (elaboration.cc:1307)
5. Operators use native C++/Metal operations
6. Single value per signal in MSL buffers

---

## Required Changes by Component

### 1. Frontend: Parser (verilog_parser.cc)

**Current**: Only accepts digits 0-9, a-f in literals (lines 1402-1420)

**Changes Needed**:
- Accept `x`, `X`, `z`, `Z`, `?` in number literals
- Examples: `4'b10x1`, `8'hzz`, `1'bz`
- Parse `?` as wildcard (equivalent to `z` in case items)

**Implementation**:
```cpp
// In ParseNumberLiteral():
if (c == 'x' || c == 'X' || c == 'z' || c == 'Z' || c == '?') {
  // Set X/Z bits appropriately
  // 'x'/'X' → unknown bit
  // 'z'/'Z'/'?' → high-impedance bit
}
```

**Estimated**: ~100 lines

---

### 2. AST: Literal Representation (ast.hh, ast.cc)

**Current**:
```cpp
struct Expr {
  uint64_t number;  // Single value
  int number_width;
  char base_char;
  // ...
};
```

**Changes Needed**:
```cpp
struct Expr {
  uint64_t value_bits;  // Logic values (0/1)
  uint64_t x_bits;      // Unknown bits (X)
  uint64_t z_bits;      // High-impedance bits (Z)
  int number_width;
  char base_char;
  // ...

  // Helper methods:
  bool IsFullyDetermined() const; // No X/Z bits set
  bool HasX() const;
  bool HasZ() const;
};
```

**Encoding Scheme**:
For each bit position:
- `x_bits[i]=0, z_bits[i]=0` → use `value_bits[i]` (0 or 1)
- `x_bits[i]=1, z_bits[i]=0` → X (unknown)
- `x_bits[i]=0, z_bits[i]=1` → Z (high-impedance)
- `x_bits[i]=1, z_bits[i]=1` → reserved (treat as X)

**Estimated**: ~50 lines

---

### 3. Constant Expression Evaluation (ast.cc)

**Current**: Uses standard C++ operators (lines 62-210)

**Changes Needed**: Implement 4-state logic tables for all operators

#### Operator X/Z Propagation Rules

**Bitwise AND (`&`)**:
```
0 & X = 0    1 & X = X    X & X = X    Z & X = X
0 & Z = 0    1 & Z = X    X & Z = X    Z & Z = X
```

**Bitwise OR (`|`)**:
```
0 | X = X    1 | X = 1    X | X = X    Z | X = X
0 | Z = X    1 | Z = 1    X | Z = X    Z | Z = X
```

**Bitwise XOR (`^`)**:
```
0 ^ X = X    1 ^ X = X    X ^ X = X    Z ^ X = X
0 ^ Z = X    1 ^ Z = X    X ^ Z = X    Z ^ Z = X
```

**Bitwise NOT (`~`)**:
```
~0 = 1    ~1 = 0    ~X = X    ~Z = X
```

**Arithmetic (`+`, `-`, `*`, `/`)**:
- Any X or Z operand → result is all X
- Division by zero → result is all X (current: error)

**Comparison (`==`, `!=`, `<`, `>`, etc.)**:
- Any X or Z in operands → result is X (1'bx)
- Current: returns 0 or 1 only

**Shifts (`<<`, `>>`)**:
- X/Z in shift amount → result is all X
- Shifting in from right/left → shift in 0 (not X)
- X/Z bits in data get shifted normally

**Ternary (`?:`)**:
```
1 ? A : B  → A
0 ? A : B  → B
X ? A : B  → bitwise merge (if A==B use that, else X)
Z ? A : B  → bitwise merge
```

**Implementation**:
```cpp
// In EvalConstExpr():
FourStateValue EvalConstExpr4State(const Expr& e) {
  switch (e.kind) {
    case ExprKind::BinaryOp:
      auto lhs = EvalConstExpr4State(e.lhs);
      auto rhs = EvalConstExpr4State(e.rhs);
      return Apply4StateOp(e.op, lhs, rhs);
    // ...
  }
}

FourStateValue Apply4StateOp(BinaryOp op, FourStateValue lhs, FourStateValue rhs) {
  // Implement lookup tables or algorithmic logic
  // Handle all combinations of 0/1/X/Z
}
```

**Estimated**: ~200 lines

---

### 4. Elaboration: Parameters and Widths (elaboration.cc)

**Current**: Parameters stored as `std::unordered_map<std::string, int64_t>` (line 112)

**Critical Decision**: Should parameters support X/Z?

#### Option A: Parameters Remain 2-State (Recommended)
- Parameters must be determinate for synthesis
- Simpler implementation
- Consistent with synthesis tools
- **Change**: Keep `int64_t`, validate no X/Z in parameter expressions

#### Option B: Parameters Support 4-State
- More flexible for testbench parameters
- Complex: affects width calculation, array sizing, etc.
- **Change**: Replace with `FourStateValue` type

**Recommendation**: **Option A** - Parameters must be determinate

**Changes Needed**:
```cpp
// Validate parameter expressions are 2-state:
if (paramValue.HasX() || paramValue.HasZ()) {
  ReportError("Parameter values cannot contain X or Z");
}
```

**Width/Range Calculations**: Must remain 2-state (indices must be determinate)

**Estimated**: ~50 lines

---

### 5. MSL Code Generation: Type System (msl_codegen.cc)

**Current**: Single value per signal
```cpp
device uint* input_port [[buffer(0)]];
device uint* wire_signal [[buffer(1)]];
uint local_var;
```

**Changes Needed**: Dual-plane storage (value + X/Z)

#### Storage Strategy Options

**Option 1: Separate Buffers** (Recommended)
```cpp
device uint* signal_val [[buffer(N)]];
device uint* signal_xz [[buffer(N+1)]];  // Combined X|Z bits
```
- Simple implementation
- Easy to optimize (if xz==0, use fast 2-state path)
- 2x buffer count

**Option 2: Struct Approach**
```cpp
struct FourState { uint val; uint xz; };
device FourState* signal [[buffer(N)]];
```
- Cleaner interface
- May have alignment/packing issues in Metal
- Harder to optimize

**Option 3: Packed Approach**
```cpp
device ulong* signal [[buffer(N)]];  // Lower 32 bits = val, upper 32 = xz
```
- Same buffer count
- Limited to 32-bit signals (or use uint2/uint4 vectors)
- Harder to read/debug

**Recommendation**: **Option 1** - Separate buffers

**Implementation**:
```cpp
// Type selection (update lines 66-68):
const char* TypeFor4State(int width) {
  if (width > 32) return "ulong";  // Can store up to 64 bits
  return "uint";
}

// Buffer declarations (lines 607-610, 617-619):
void EmitPortBuffers() {
  for (auto& port : module.ports) {
    fprintf(out, "  device %s* %s_val [[buffer(%d)]],\n",
            TypeFor4State(port.width), port.name, bufidx++);
    fprintf(out, "  device %s* %s_xz [[buffer(%d)]],\n",
            TypeFor4State(port.width), port.name, bufidx++);
  }
}

// Signal access:
void EmitSignalRead(const Signal& sig) {
  // Read both planes
  fprintf(out, "%s_val[gid]", sig.name);
  // Track xz bits separately
}
```

**Estimated**: ~200 lines

---

### 6. MSL Code Generation: Operators (msl_codegen.cc)

**Current**: Direct C++ operators (lines 420-458)

**Changes Needed**: 4-state operator functions

**Implementation Strategy**:

#### Emit Helper Functions in MSL Preamble
```metal
// At top of generated .metal file:

struct FourState {
  uint val;
  uint xz;  // Combined X and Z bits
};

FourState fs_and(FourState a, FourState b, uint width) {
  uint mask = (width == 32) ? ~0u : ((1u << width) - 1);
  FourState result;

  // If either has X/Z, result may have X/Z
  // 0 & anything = 0 (determined)
  // 1 & X/Z = X

  uint a_is_zero = (~a.val) & (~a.xz) & mask;  // Bits that are definitely 0
  uint b_is_zero = (~b.val) & (~b.xz) & mask;

  result.val = (a.val & b.val) & mask;

  // X/Z propagation: if either input has X/Z and other is not-zero
  result.xz = ((a.xz | b.xz) & ~(a_is_zero | b_is_zero)) & mask;

  return result;
}

FourState fs_or(FourState a, FourState b, uint width) {
  // Similar logic: 1 | anything = 1, else propagate X/Z
}

FourState fs_xor(FourState a, FourState b, uint width) {
  // Any X/Z in inputs → X in output
}

FourState fs_not(FourState a, uint width) {
  FourState result;
  result.val = ~a.val;
  result.xz = a.xz;  // X/Z preserved
  return result;
}

FourState fs_eq(FourState a, FourState b, uint width) {
  // If any X/Z in either operand → 1'bx
  // Else compare values
  FourState result;
  if ((a.xz | b.xz) != 0) {
    result.val = 0;
    result.xz = 1;  // Result is X
  } else {
    result.val = (a.val == b.val) ? 1 : 0;
    result.xz = 0;
  }
  return result;
}

// Similar for all operators...
```

#### Update Operator Emission
```cpp
// In EmitBinaryOp() (lines 432-458):
void EmitBinaryOp(BinaryOp op, Expr lhs, Expr rhs) {
  if (fourStateMode) {
    fprintf(out, "fs_%s(", OperatorName(op));
    EmitExpr(lhs);
    fprintf(out, ", ");
    EmitExpr(rhs);
    fprintf(out, ", %d)", std::max(lhs.width, rhs.width));
  } else {
    // Current 2-state path
  }
}
```

**Estimated**: ~300 lines (helper functions + emission)

---

### 7. Optimizations

**Current**: Uses simplification passes (elaboration.cc:551-622)
- Example: `x & all_ones` → `x`

**Changes Needed**: X-aware optimizations

**Safe Optimizations with X/Z**:
```
x & 0       → 0       (always safe)
x | all_1   → all_1   (always safe)
x & all_1   → x       (safe, preserves X/Z)
x | 0       → x       (safe, preserves X/Z)
```

**Unsafe Optimizations**:
```
x ^ x       → 0       (UNSAFE: if x contains X, result should be X)
x - x       → 0       (UNSAFE: if x contains X, result should be X)
```

**Implementation**:
```cpp
// Update IsAllOnes/IsAllZeros to check X/Z bits:
bool IsAllOnes(const Expr& e) {
  if (e.HasX() || e.HasZ()) return false;
  return (e.value_bits == ((1ull << e.width) - 1));
}
```

**Estimated**: ~50 lines

---

### 8. Default/Uninitialized Values

**Current**: Unconnected inputs default to `0` (elaboration.cc:1307)

**Changes Needed**:
- Option A: Default to `X` (more realistic, matches simulators)
- Option B: Keep `0` (safer for synthesis)

**Verilog Standard**: Uninitialized regs are `X`

**Implementation**:
```cpp
// In elaboration, unconnected ports:
FourStateValue defaultValue;
defaultValue.value_bits = 0;
defaultValue.x_bits = ~0ull;  // All X
defaultValue.z_bits = 0;

// In MSL initialization:
signal_val[gid] = 0;
signal_xz[gid] = ~0u;  // All X initially
```

**Estimated**: ~20 lines

---

### 9. Case Statement Extensions

Once 4-state support exists, `casex`/`casez` become straightforward:

**`casez`**: Treat `?` in case items as don't-care
```verilog
casez (addr)
  4'b1???: out = 1;  // Match if addr[3]==1, ignore [2:0]
endcase
```

**Implementation**:
```cpp
// Generate comparison with mask:
// if ((addr_val & 4'b1000) == (4'b1000 & 4'b1000) &&
//     (addr_xz & 4'b1000) == 0)  // No X/Z in compared bits
```

**`casex`**: Additionally ignore `X` in comparison
```verilog
casex (state)
  4'b1xxx: next = IDLE;  // Match if state[3]==1, ignore X in [2:0]
endcase
```

**Implementation**: Same as `casez` but also mask out bits that are X in case item

**Estimated**: ~100 lines additional to case statement implementation

---

## Implementation Phases

### Phase 1: Core Infrastructure (Foundation)
**Goal**: Establish 4-state value representation

1. Extend `Expr` struct with `x_bits`, `z_bits` (ast.hh)
2. Update parser to accept X/Z/? in literals (verilog_parser.cc)
3. Add helper methods: `IsFullyDetermined()`, `HasX()`, `HasZ()`
4. Write unit tests for 4-state literal parsing

**Estimated**: ~150 lines, 1-2 days

**Test**: Parse `4'b10xz`, `8'hx5`, `4'b????` successfully

---

### Phase 2: Constant Folding (Propagation)
**Goal**: Implement 4-state arithmetic

5. Implement 4-state operator functions (ast.cc)
6. Update `EvalConstExpr()` to use 4-state operations
7. Add X/Z propagation for all operators
8. Update parameter validation (parameters must be 2-state)

**Estimated**: ~250 lines, 2-3 days

**Test**: Evaluate `4'b10x1 & 4'b1100` → `4'b10x0`

---

### Phase 3: Code Generation (MSL)
**Goal**: Generate 4-state Metal shaders

9. Implement dual-plane storage strategy (msl_codegen.cc)
10. Double buffer allocations (value + xz planes)
11. Emit 4-state helper functions in MSL preamble
12. Update operator emission to call 4-state functions
13. Update signal reads/writes for dual-plane access

**Estimated**: ~400 lines, 3-5 days

**Test**: Generate MSL that correctly propagates X through operations

---

### Phase 4: Testing and Validation
**Goal**: Ensure correctness

14. Create comprehensive test suite (test/4state/*.v)
15. Test X propagation through all operators
16. Test Z handling in various contexts
17. Verify default initialization to X
18. Compare results with commercial simulators (ModelSim, VCS)

**Estimated**: ~200 lines (tests), 2-3 days

---

### Phase 5: Case Extensions (Optional)
**Goal**: Enable casex/casez

19. Implement wildcard matching for casez
20. Implement X-ignore for casex
21. Test case statement X handling

**Estimated**: ~150 lines, 1-2 days

---

## Performance Implications

### Memory Impact
- **Storage**: 2x (dual-plane for all signals)
- **Buffers**: 2x buffer count in Metal kernel signatures
- **Bandwidth**: 2x memory traffic

### Computation Impact
- **Operators**: 2-10x slower (function calls vs. native ops)
- **Comparisons**: Additional X/Z checks
- **Throughput**: Estimated 3-5x overall slowdown

### Optimization Opportunities
1. **2-State Fast Path**: Detect signals with no X/Z, use native ops
2. **Compile-Time Detection**: If entire design is 2-state, skip 4-state codegen
3. **Hybrid Mode**: Only apply 4-state to signals that need it
4. **SIMD**: Use vector types (uint2/uint4) for parallel bit operations

---

## Critical Design Decisions

### Decision 1: Parameter Support
- **A**: Parameters remain 2-state (RECOMMENDED)
- **B**: Parameters support 4-state

**Recommendation**: **A** - matches synthesis tools, simpler

---

### Decision 2: Storage Strategy
- **A**: Separate buffers for val/xz (RECOMMENDED)
- **B**: Struct-based
- **C**: Packed into wider type

**Recommendation**: **A** - simplest, most flexible

---

### Decision 3: Default Values
- **A**: Uninitialized signals are X (RECOMMENDED - matches Verilog)
- **B**: Uninitialized signals are 0 (current behavior)

**Recommendation**: **A** - but make configurable via flag

---

### Decision 4: Compilation Modes
- **A**: Always 4-state (simple but slower)
- **B**: Compile-time flag: --2state vs --4state
- **C**: Automatic detection per-signal

**Recommendation**: **B** - flag-based, default to 2-state for performance

---

## Testing Strategy

### Test Cases Needed

**Basic X/Z Propagation**:
```verilog
module test_xz;
  wire [3:0] a = 4'b10x1;
  wire [3:0] b = 4'b1100;
  wire [3:0] result = a & b;  // Should be 4'b10x0
endmodule
```

**Comparison with X**:
```verilog
module test_cmp_x;
  wire [3:0] a = 4'b10x1;
  wire eq = (a == 4'b1001);  // Should be 1'bx
endmodule
```

**Ternary with X**:
```verilog
module test_ternary_x;
  wire sel = 1'bx;
  wire [7:0] result = sel ? 8'h55 : 8'hAA;  // Should be 8'hxx
endmodule
```

**Casez**:
```verilog
module test_casez;
  reg [3:0] addr;
  reg match;
  always @(*) casez (addr)
    4'b1???: match = 1;
    default: match = 0;
  endcase
endmodule
```

---

## Total Effort Estimate

**Lines of Code**: ~850-1000 lines modified/added
**Time**: 2-3 weeks for full implementation
**Complexity**: Medium-High (touches all compiler stages)

---

## Alternative: Partial 4-State Support

If full 4-state is too complex, consider **casez-only** support:

### Minimal Changes for Casez
1. Parse `?` in case item literals (parser)
2. Generate masked comparison in MSL (codegen)
3. **No general X/Z support** - only in case items

**Effort**: ~100 lines, 1-2 days
**Benefit**: Enables casez without full 4-state overhead

**Implementation**:
```cpp
// Case item with wildcards: 4'b1???
// Generate: if ((signal & 4'b1000) == (4'b1000 & 4'b1000))

uint mask = ~wildcard_bits;  // Where ? appears, mask bit is 0
if ((signal_val & mask) == (case_value & mask)) {
  // Match
}
```

This gives 80% of the benefit with 20% of the effort.

---

## Conclusion

4-state logic support is **feasible but significant**:
- Touches every compilation stage
- 2x memory overhead
- 3-5x performance impact
- ~850 lines of changes

**Recommended Approach**:
1. Start with **casez-only** (minimal approach)
2. If full semantics needed, implement in phases
3. Make 4-state **opt-in** via compiler flag
4. Keep 2-state as default for performance

**Decision Point**: Does your use case require full 4-state accuracy, or is casez with masking sufficient?
