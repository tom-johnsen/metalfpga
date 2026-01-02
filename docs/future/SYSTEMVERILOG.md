# SystemVerilog Support in MetalFPGA

**Status:** Design document
**Priority:** Post-Verilog-2005 completion
**Scope:** SystemVerilog features beyond IEEE 1364-2005 Verilog

---

## Executive Summary

MetalFPGA has achieved **complete Verilog-2005 (IEEE 1364-2005) support** as of v0.80085. SystemVerilog (IEEE 1800-2017) adds substantial language features on top of Verilog-2005, ranging from simple enhancements to complex verification constructs.

This document categorizes SystemVerilog features by implementation complexity and architectural fit with MetalFPGA's GPU-based Metal Shading Language (MSL) compilation model, providing a strategic roadmap for incremental adoption.

**Key Finding:** ~70% of synthesizable SystemVerilog features can be implemented with minor to moderate effort, while verification-specific features (assertions, randomization, OOP) require architectural decisions about GPU vs CPU execution.

---

## Background: Verilog-2005 vs SystemVerilog

### What MetalFPGA Already Has (Verilog-2005)

✅ **Complete as of v0.80085:**
- Module hierarchy, parameters, generate blocks
- Wire/reg nets, multi-dimensional arrays
- Blocking/non-blocking assignments
- Always blocks (combinational, sequential, latch)
- Tasks and functions
- System tasks (`$display`, `$monitor`, `$finish`, etc.)
- Compiler directives (`` `define``, `` `ifdef``, `` `include``)
- 4-state logic (0, 1, X, Z) via val/xz representation
- Timing checks (`$setup`, `$hold`, `$width`, etc.)
- VCD waveform generation

### What SystemVerilog Adds

SystemVerilog extends Verilog across three major domains:

1. **Design features** (synthesizable RTL enhancements)
2. **Verification features** (testbench constructs)
3. **Assertion features** (temporal logic checkers)

**Philosophy:** Incrementally add **design features first** (high value, architectural compatibility), defer complex verification features until VPI/DPI infrastructure matures.

---

## Feature Categories & Implementation Strategy

### Category 1: Trivial Extensions (Quick Wins)

**Effort:** Low (parser + trivial elaboration changes)
**Value:** High (common idioms, better code quality)
**Timeline:** Can be added incrementally alongside bug fixes

#### 1.1 Enhanced Data Types

| Feature | Description | Implementation |
|---------|-------------|----------------|
| **`logic`** | Unified wire/reg type | Parser: treat as `reg`, allow in all contexts |
| **`bit`** | 2-state (0/1 only) | Map to `logic` (ignore X/Z semantics for now) |
| **`byte`, `shortint`, `int`, `longint`** | Sized integer types | Map to equivalent `logic [N:0]` |
| **`enum`** | Enumerated types | Parser: expand to `localparam` constants |
| **`typedef`** | Type aliases | Elaborator: symbol table tracking |
| **`struct`/`union`** | Aggregate types | Flatten to concatenated `logic` vectors |

**Example:**
```systemverilog
// SystemVerilog
typedef enum logic [1:0] {
  IDLE = 2'b00,
  RUN  = 2'b01,
  DONE = 2'b10
} state_t;

state_t state, next_state;
```

**MetalFPGA lowering:**
```verilog
// Internal representation
localparam IDLE = 2'b00;
localparam RUN  = 2'b01;
localparam DONE = 2'b10;
logic [1:0] state, next_state;
```

**Files to modify:**
- `src/frontend/verilog_parser.cc` — Add keywords, type parsing
- `src/frontend/ast.hh` — New AST node types (`TypedefDecl`, `EnumDecl`)
- `src/core/elaboration.cc` — Type resolution and flattening

---

#### 1.2 Enhanced Literals

| Feature | Example | Implementation |
|---------|---------|----------------|
| **`'0`, `'1`, `'x`, `'z`** | Fill all bits | Expand to sized literal during parsing |
| **Binary underscore separators** | `8'b1010_1010` | Lexer: strip underscores |
| **Time literals** | `10ns`, `1us` | Convert to timescale-relative integers |

**Example:**
```systemverilog
logic [31:0] mask = '1;  // All ones: 32'hFFFFFFFF
```

---

#### 1.3 Enhanced Operators

| Operator | Meaning | Implementation |
|----------|---------|----------------|
| **`++`, `--`** | Increment/decrement | Desugar to `x = x + 1` |
| **`+=`, `-=`, etc.** | Compound assignment | Desugar to `x = x + y` |
| **`inside`** | Set membership | Expand to OR chain: `(x == A \|\| x == B)` |
| **Wildcard equality `==?`** | Don't-care matching | Custom 4-state comparison |

**Example:**
```systemverilog
if (opcode inside {ADD, SUB, MUL})
  // Becomes: if (opcode == ADD || opcode == SUB || opcode == MUL)
```

---

#### 1.4 Enhanced Procedural Blocks

| Feature | Description | Implementation |
|---------|-------------|----------------|
| **`always_comb`** | Explicit combinational | Parse as `always @(*)`, lint for blocking only |
| **`always_ff`** | Explicit sequential | Parse as `always @(...)`, lint for non-blocking |
| **`always_latch`** | Explicit latch | Parse as `always @(...)`, lint for level-sensitive |
| **`unique`/`priority` case** | Case statement hints | Ignored (lint feature) or emit warnings |

**Value:** Clearer intent, easier linting (can warn on misuse).

---

### Category 2: Moderate Complexity (High Value)

**Effort:** Moderate (new elaboration logic, MSL codegen extensions)
**Value:** High (essential for modern SystemVerilog designs)
**Timeline:** Prioritize after Category 1 complete

#### 2.1 Interfaces

**What:** Bundles of signals with modports (directional views).

**Example:**
```systemverilog
interface axi_if;
  logic [31:0] awaddr;
  logic        awvalid;
  logic        awready;

  modport master (output awaddr, awvalid, input awready);
  modport slave  (input awaddr, awvalid, output awready);
endinterface

module cpu(axi_if.master bus);
  assign bus.awaddr = addr_reg;
endmodule

module mem(axi_if.slave bus);
  assign bus.awready = ready_reg;
endmodule
```

**Implementation Strategy:**

1. **Parser:** New AST nodes for `interface`, `modport`, interface instances
2. **Elaborator:** Flatten interface to individual wires during elaboration
3. **Name resolution:** `bus.awaddr` → `cpu_bus_awaddr` (flat name)
4. **MSL codegen:** No changes (already flattened to wires)

**Flattening example:**
```verilog
// After elaboration
module cpu(...);
  output [31:0] cpu_bus_awaddr;
  output        cpu_bus_awvalid;
  input         cpu_bus_awready;
endmodule
```

**Files to modify:**
- `src/frontend/verilog_parser.cc` — Interface parsing
- `src/core/elaboration.cc` — Interface instantiation, modport resolution
- `src/frontend/ast.hh` — `InterfaceDecl`, `ModportDecl` nodes

---

#### 2.2 Packages and Import

**What:** Namespaces for types, parameters, functions.

**Example:**
```systemverilog
package cpu_pkg;
  typedef enum logic [2:0] {
    ADD, SUB, MUL, DIV
  } opcode_t;

  parameter int DATA_WIDTH = 32;
endpackage

module alu import cpu_pkg::*;
  input cpu_pkg::opcode_t opcode;
  // ...
endmodule
```

**Implementation Strategy:**

1. **Parser:** Parse `package` blocks, `import` statements
2. **Elaborator:** Maintain package symbol table, resolve imports
3. **Type resolution:** Look up `cpu_pkg::opcode_t` in package scope
4. **Flattening:** Copy imported definitions into using module

**Files to modify:**
- `src/frontend/verilog_parser.cc` — Package/import parsing
- `src/core/elaboration.cc` — Package symbol table, import resolution

---

#### 2.3 Enhanced Generate Constructs

SystemVerilog improves Verilog-2005 generate syntax:

| Feature | Verilog-2005 | SystemVerilog |
|---------|--------------|---------------|
| **Loop variable** | Must declare in separate `genvar` | Can declare inline: `for (genvar i = 0; ...)` |
| **If/case labels** | Requires `begin: label` | Optional labels |
| **Nesting** | Clunky syntax | Cleaner nesting |

**Implementation:** Relax parser restrictions (already have generate logic from Verilog-2005).

---

#### 2.4 Unpacked Arrays

**Verilog-2005:** Only packed arrays allowed: `logic [31:0] data;`
**SystemVerilog:** Unpacked dimensions: `logic [31:0] mem [0:1023];`

**Example:**
```systemverilog
logic [7:0] memory [0:255];  // 256-element array of 8-bit values
int queue [$];                // Dynamic array (verification only)
```

**Implementation:**

1. **Parser:** Support `[size]` after variable name
2. **MSL codegen:** Map to Metal buffer arrays:
   ```metal
   device uint* memory [[buffer(5)]];  // Flat 256*4-byte buffer
   ```

**Limitation:** Dynamic arrays (`$`) require runtime allocation—defer to verification phase.

---

#### 2.5 Always_comb Automatic Sensitivity

Already works in Verilog-2005 `always @(*)`, but SystemVerilog formalizes it.

**Additional check:** Lint that `always_comb` doesn't have edge-sensitive logic.

---

### Category 3: Architectural Decisions Required

**Effort:** High (requires design choices about GPU/CPU partitioning)
**Value:** Medium to High (depends on use case)
**Timeline:** After VPI/DPI infrastructure complete

#### 3.1 Assertions (SVA - SystemVerilog Assertions)

**What:** Temporal property checking (concurrent/immediate assertions).

**Example:**
```systemverilog
// Concurrent assertion: req → ack within 3 cycles
property p_handshake;
  @(posedge clk) req |-> ##[1:3] ack;
endproperty
assert property (p_handshake);

// Immediate assertion: check condition now
assert (data_valid) else $error("Data invalid!");
```

**Challenge:** Assertions run **throughout simulation**, not just at end.

**GPU Implementation Options:**

**Option A: GPU-Based Monitoring (Efficient)**
- Compile assertions into Metal kernels that run alongside simulation
- Use atomic counters to track failures
- **Pro:** Zero overhead (runs in parallel with simulation)
- **Con:** Complex codegen (temporal operators → state machines)

**Option B: VPI-Based Monitoring (Simple)**
- Assertions run as VPI callbacks on CPU
- Monitor signals via unified memory
- **Pro:** Easier to implement (standard VPI checker library)
- **Con:** Batch-based checking (not cycle-accurate)

**Recommended:** Start with Option B (VPI-based), optimize to Option A later.

**Example VPI-based assertion:**
```c
// VPI callback checks property every 1000 cycles
PLI_INT32 check_handshake(p_cb_data cb) {
  if (get_signal("req") && !get_signal("ack")) {
    if (++wait_count > 3) {
      vpi_printf("ASSERTION FAILED: handshake timeout\n");
    }
  } else {
    wait_count = 0;
  }
}
```

**Files to add:**
- `src/assertions/sva_parser.cc` — SVA grammar parser
- `src/assertions/sva_monitor.cc` — Generate VPI monitors
- Integration with VPI runtime (see [VPI_DPI_UNIFIED_MEMORY.md](VPI_DPI_UNIFIED_MEMORY.md))

---

#### 3.2 Constrained Randomization

**What:** `rand` variables with constraint solving for testbench stimulus.

**Example:**
```systemverilog
class Transaction;
  rand bit [7:0] addr;
  rand bit [31:0] data;

  constraint valid_addr {
    addr inside {[0:127]};
  }
endclass

Transaction t = new();
t.randomize();  // Solve constraints, assign random values
```

**Challenge:** Constraint solving is CPU-intensive, doesn't map to GPU well.

**Implementation Strategy:**

**CPU-based (recommended):**
- Constraint solver runs on CPU (use existing libraries like Z3, or simple backtracking)
- Solved values written to GPU buffers via unified memory
- **Works well for:** Testbench initialization, periodic stimulus injection

**Not suitable for:** In-kernel randomization (defeats GPU parallelism)

**Files to add:**
- `src/verification/constraint_solver.cc` — Constraint parser & solver
- Integration with VPI (inject randomized values as `vpi_put_value`)

---

#### 3.3 Object-Oriented Programming (Classes)

**What:** Classes, inheritance, polymorphism for testbenches.

**Example:**
```systemverilog
virtual class BaseBFM;
  pure virtual task drive(Transaction t);
endclass

class AXIBFM extends BaseBFM;
  virtual task drive(Transaction t);
    // Drive AXI bus
  endtask
endclass
```

**Reality Check:** Classes are **testbench-only** (not synthesizable).

**Implementation Decision:**

**Defer OOP to external tools:**
- MetalFPGA focuses on **synthesizable** RTL → GPU kernels
- Testbenches written in C++/Swift using VPI/DPI
- **Rationale:** Replicating C++ OOP in a hardware simulator is low ROI

**Alternative:** Support basic `class` syntax for **data containers** only:
```systemverilog
class Packet;
  bit [7:0] header;
  bit [31:0] payload;
endclass

Packet p = new();  // Becomes a struct
```

---

#### 3.4 Covergroups and Functional Coverage

**What:** Automatic tracking of signal value combinations for verification completeness.

**Example:**
```systemverilog
covergroup cg @(posedge clk);
  cp_opcode: coverpoint opcode {
    bins add_sub = {ADD, SUB};
    bins mul_div = {MUL, DIV};
  }
endgroup
```

**Implementation Strategy:**

**GPU-assisted counting:**
- Compile coverpoints into atomic counters in Metal
- CPU reads coverage statistics at end of simulation
- **Efficient:** Parallel coverage tracking, zero overhead

**Example MSL:**
```metal
kernel void simulate(..., device atomic_uint* coverage_bins) {
  uint opcode = fetch_opcode();

  if (opcode == ADD || opcode == SUB) {
    atomic_fetch_add_explicit(&coverage_bins[0], 1, memory_order_relaxed);
  } else if (opcode == MUL || opcode == DIV) {
    atomic_fetch_add_explicit(&coverage_bins[1], 1, memory_order_relaxed);
  }
}
```

**Files to add:**
- `src/coverage/covergroup_parser.cc` — Parse covergroup syntax
- `src/codegen/coverage_codegen.cc` — Generate atomic counter logic
- `src/runtime/coverage_report.cc` — Post-simulation reporting

---

### Category 4: Low Priority / Out of Scope

**Effort:** Very High
**Value:** Low (niche use cases or better handled externally)
**Timeline:** Not planned

#### 4.1 DPI (Direct Programming Interface)

**Status:** See dedicated document [VPI_DPI_UNIFIED_MEMORY.md](VPI_DPI_UNIFIED_MEMORY.md)
**Summary:** Planned as part of VPI infrastructure (Phase 4).

#### 4.2 Hierarchical Names in Constraints

**Example:** `constraint c { top.cpu.pc < 1024; }`

**Challenge:** Requires runtime design introspection.
**Defer to:** VPI-based constraint evaluation.

#### 4.3 Dynamic Processes (`fork`/`join`)

**What:** Spawn concurrent threads in simulation.

**Challenge:** MetalFPGA uses fixed threadgroup dispatch—dynamic process spawning doesn't map cleanly.
**Workaround:** Model as explicit FSMs or defer to testbench.

#### 4.4 Fine-Grained Timing (`#delay` in expressions)

**Example:** `assign #5 y = x;` (5-unit delay on assignment)

**Status:** Already unsupported in Verilog-2005 (MetalFPGA models register-transfer level, not gate delays).
**No change:** Continue ignoring intra-assignment delays.

---

## Recommended Implementation Roadmap

### Phase 1: Essential Design Features (Low-Hanging Fruit)

**Target:** 3-6 months post-Verilog-2005
**Goal:** Support 80% of common SystemVerilog RTL idioms

**Features:**
1. ✅ `logic`, `bit`, `byte`, `int` types
2. ✅ `typedef`, `enum`, `struct`/`union`
3. ✅ Enhanced literals (`'0`, `'1`, underscores)
4. ✅ `always_comb`, `always_ff`, `always_latch`
5. ✅ `unique`/`priority` case (lint only)
6. ✅ Compound operators (`+=`, `++`, etc.)
7. ✅ `inside` operator

**Deliverable:** MetalFPGA can compile modern SystemVerilog RTL (e.g., lowRISC Ibex, SiFive cores).

**Success Metric:** 95% of files in open-source SystemVerilog projects parse successfully.

---

### Phase 2: Intermediate Features (Interfaces & Packages)

**Target:** 6-12 months post-Phase 1
**Goal:** Full module composition and code reuse support

**Features:**
1. ✅ Interfaces with modports
2. ✅ Packages and `import`
3. ✅ Unpacked arrays
4. ✅ Enhanced generate syntax

**Deliverable:** Support complex SoC designs with interface-based composition (e.g., AMBA AXI ecosystems).

**Success Metric:** Can compile a full RISC-V SoC with AXI interconnects.

---

### Phase 3: Verification Infrastructure (Assertions & Coverage)

**Target:** Post-VPI/DPI implementation
**Goal:** Basic assertion and coverage support

**Dependencies:**
- ✅ VPI runtime complete (see [VPI_DPI_UNIFIED_MEMORY.md](VPI_DPI_UNIFIED_MEMORY.md))
- ✅ Unified memory callback system working

**Features:**
1. ✅ Immediate assertions (`assert`, `assume`)
2. ✅ Concurrent assertions (SVA) — VPI-based monitoring
3. ✅ Basic covergroups — GPU atomic counters
4. ✅ Coverage reporting

**Deliverable:** Testbenches can run with inline assertions and auto-generate coverage reports.

**Success Metric:** Detect protocol violations in 1000-cycle testbench with <5% overhead.

---

### Phase 4: Advanced Verification (Randomization)

**Target:** Post-Phase 3 (if demand exists)
**Goal:** Constrained-random stimulus generation

**Features:**
1. ✅ `rand` variables
2. ✅ Constraint solver (basic inequalities, set membership)
3. ✅ `randomize()` method (CPU-based)

**Deliverable:** Generate random testbench stimulus with constraints.

**Success Metric:** 1000 constrained-random transactions generated in <100ms.

---

## Key Architectural Insights

### 1. Synthesizable vs. Verification Constructs

| Category | MetalFPGA Strategy |
|----------|-------------------|
| **Synthesizable RTL** | Compile to GPU kernels (MSL) — **core focus** |
| **Testbench verification** | CPU-based (VPI/DPI) or external tools — **secondary** |

**Rationale:** MetalFPGA's value is **fast RTL simulation on GPU**. Verification features that don't map to GPU parallelism should run on CPU or be deferred to external tools (Python, C++, etc.).

---

### 2. Flattening Strategy

**Key principle:** Most SystemVerilog features **desugar to Verilog-2005 semantics**.

**Examples:**
- **`typedef`** → Symbol table aliases
- **`enum`** → `localparam` constants
- **`struct`** → Concatenated bit vectors
- **`interface`** → Flattened wires
- **`always_comb`** → `always @(*)`

**Advantage:** Minimal MSL codegen changes—complexity stays in frontend/elaborator.

---

### 3. GPU vs. CPU Partitioning

**Run on GPU (Metal kernels):**
- Combinational/sequential logic
- Memories and arrays
- Coverage counters (atomic ops)
- Possibly: compiled assertions (advanced)

**Run on CPU (Objective-C++ runtime):**
- VPI/DPI callbacks
- Constraint solving
- Complex assertions (initially)
- File I/O, system tasks

**Unified memory = glue:** CPU and GPU share signal buffers, enabling low-latency interaction.

---

## Migration Path for Users

### Current Verilog-2005 Code

**No changes required.** All existing Verilog-2005 designs continue to work.

---

### SystemVerilog Code (Post-Phase 1)

**Before (must port to Verilog-2005):**
```systemverilog
typedef enum logic [1:0] {IDLE, RUN, DONE} state_t;
state_t state;
always_comb begin
  case (state)
    IDLE: next_state = RUN;
    RUN:  next_state = DONE;
    DONE: next_state = IDLE;
  endcase
end
```

**After Phase 1 (works directly):**
No changes—MetalFPGA natively supports this!

---

### Interface-Based Code (Post-Phase 2)

**Before (must manually flatten interfaces):**
```systemverilog
interface axi_if;
  logic awvalid, awready;
endinterface

module cpu(axi_if.master bus);
```

**After Phase 2 (works directly):**
No flattening needed—MetalFPGA handles interfaces natively.

---

## Testing Strategy

### Golden Tests

**Approach:** Reuse existing SystemVerilog test suites (same strategy as Verilog-2005).

**Targets:**
1. **Phase 1:** sv-tests (open-source SV compliance suite)
2. **Phase 2:** lowRISC Ibex core, SiFive E20 core
3. **Phase 3:** UVM testbenches (assertion-heavy)

**Metric:** Pass rate on vendor-neutral test suites (Verilator, Icarus Verilog, commercial sims).

---

### Differential Testing

**Method:** Run same SystemVerilog design on:
- MetalFPGA (GPU simulation)
- Verilator (C++ simulation)
- Icarus Verilog (reference)

**Compare:** Waveforms (VCD), assertion failures, coverage reports.

---

## Performance Expectations

### Phase 1 Features (Trivial Extensions)

**Performance impact:** **None.** All features desugar to existing Verilog-2005 constructs.

**Example:** `always_comb` compiles to identical MSL as `always @(*)`.

---

### Phase 2 Features (Interfaces)

**Performance impact:** **None.** Interfaces flatten during elaboration (pre-MSL codegen).

**Elaboration time:** +5-10% (symbol table lookups), negligible for runtime.

---

### Phase 3 Features (Assertions)

**VPI-based assertions:** ~2% overhead (see [VPI_DPI_UNIFIED_MEMORY.md](VPI_DPI_UNIFIED_MEMORY.md))

**GPU-based assertions (future):** <0.1% overhead (parallel monitoring).

---

### Phase 4 Features (Randomization)

**Constraint solving:** CPU-bound, runs **outside** simulation loop.

**Impact:** None on simulation throughput (pre-generates stimulus).

---

## Compatibility Matrix

### Feature Support Levels

| Feature | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
|---------|---------|---------|---------|---------|
| `logic`, `bit`, `int` | ✅ | ✅ | ✅ | ✅ |
| `typedef`, `enum`, `struct` | ✅ | ✅ | ✅ | ✅ |
| `always_comb`, `always_ff` | ✅ | ✅ | ✅ | ✅ |
| Enhanced operators | ✅ | ✅ | ✅ | ✅ |
| Interfaces | ❌ | ✅ | ✅ | ✅ |
| Packages | ❌ | ✅ | ✅ | ✅ |
| Unpacked arrays | ❌ | ✅ | ✅ | ✅ |
| Immediate assertions | ❌ | ❌ | ✅ | ✅ |
| Concurrent assertions | ❌ | ❌ | ✅ | ✅ |
| Covergroups | ❌ | ❌ | ✅ | ✅ |
| Constrained random | ❌ | ❌ | ❌ | ✅ |
| Classes (basic) | ❌ | ❌ | ❌ | ⚠️ (limited) |
| DPI | ❌ | ❌ | ⚠️ (via VPI) | ✅ |

**Legend:**
- ✅ Fully supported
- ⚠️ Partial support
- ❌ Not supported

---

## Open Questions & Design Decisions

### 1. 2-State vs 4-State Simulation

**Question:** Should `bit` variables use 2-state simulation (faster) or fall back to 4-state?

**Options:**
- **A:** Treat `bit` as `logic` (4-state) — simple, but defeats purpose of 2-state types
- **B:** Maintain separate 2-state buffers — complex, but faster for `bit`-heavy designs
- **C:** Compile-time flag: `-4state` (default) vs `-2state` (opt-in optimization)

**Recommendation:** **Option A** for Phase 1 (simplicity), **Option C** for Phase 2 (performance).

---

### 2. Assertion Execution Model

**Question:** When do assertions run?

**Options:**
- **Batched (VPI):** Check every N cycles (simple, works with existing scheduler)
- **Cycle-accurate (GPU):** Check every cycle (complex codegen, true SVA semantics)

**Recommendation:** **Batched** for Phase 3, **GPU-based** for Phase 4 (if performance-critical).

---

### 3. Unsynthesizable Construct Handling

**Question:** What happens when user includes OOP/procedural code?

**Options:**
- **Error:** Reject unsupported features (strict)
- **Warning:** Ignore with diagnostic (lenient)
- **Fallback:** Run on CPU via DPI (hybrid)

**Recommendation:** **Warning** for Phase 1-2 (tell user to refactor), **Fallback** for Phase 3+ (DPI-based).

---

## Success Metrics

### Adoption Metrics

| Milestone | Metric | Target |
|-----------|--------|--------|
| **Phase 1 complete** | % of GitHub SV projects that parse | 80% |
| **Phase 2 complete** | RISC-V cores compiled successfully | 5+ cores |
| **Phase 3 complete** | UVM testbenches running | 1+ full testbench |
| **Phase 4 complete** | Constrained-random tests passing | 10,000+ tests |

---

### Performance Metrics

| Feature | Overhead Target | Why |
|---------|----------------|-----|
| **Type system** (Phase 1) | 0% | Desugars to Verilog-2005 |
| **Interfaces** (Phase 2) | 0% runtime, +10% elaboration | Flattened pre-codegen |
| **Assertions** (Phase 3) | <5% | VPI batching, unified memory |
| **Coverage** (Phase 3) | <1% | GPU atomic counters |
| **Randomization** (Phase 4) | N/A | Offline (pre-simulation) |

---

## References

### Standards

- **IEEE 1800-2017:** SystemVerilog Language Reference Manual
- **IEEE 1364-2005:** Verilog Hardware Description Language (MetalFPGA baseline)

### Related Documents

- [VPI_DPI_UNIFIED_MEMORY.md](VPI_DPI_UNIFIED_MEMORY.md) — VPI/DPI implementation strategy
- [TIMING_CHECKS_IMPLEMENTATION.md](../TIMING_CHECKS_IMPLEMENTATION.md) — Current timing check implementation
- [GPGA_SCHED_API.md](../GPGA_SCHED_API.md) — Scheduler architecture

### Open-Source Implementations

- **Verilator:** 2-state SystemVerilog compiler (reference for flattening strategy)
- **sv-tests:** Compliance test suite (https://github.com/SymbiFlow/sv-tests)
- **Surelog/UHDM:** Full SV parser (potential reference for AST design)

### Example Codebases

- **lowRISC Ibex:** RISC-V core (heavy interface/package use)
- **SiFive Freedom:** SoC platform (interface-based composition)
- **OpenTitan:** Secure chip (assertions, covergroups, DPI)

---

## Summary

**Strategic Approach:**
1. **Phase 1 (Quick Wins):** Add syntactic sugar (types, operators) — low effort, high compatibility
2. **Phase 2 (Composition):** Interfaces and packages — enable modern design methodology
3. **Phase 3 (Verification):** Assertions and coverage — leverage VPI infrastructure
4. **Phase 4 (Advanced):** Randomization — complete UVM-style testbench support

**Architectural Fit:**
- ✅ **Synthesizable RTL** → GPU kernels (excellent fit)
- ⚠️ **Testbench constructs** → VPI/DPI (acceptable via unified memory)
- ❌ **Non-synthesizable OOP** → External tools (low priority)

**Competitive Positioning:**
- **Verilator:** Fast 2-state C++ compiler, limited 4-state/assertion support
- **MetalFPGA:** GPU-accelerated 4-state simulation, incremental SystemVerilog adoption
- **Differentiation:** Metal GPU backend + unified memory = unique performance/verification tradeoff

**Bottom Line:** SystemVerilog support is **feasible and valuable**, with clear incremental path that builds on MetalFPGA's existing strengths.
