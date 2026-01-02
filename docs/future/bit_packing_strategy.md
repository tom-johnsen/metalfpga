# Bit-Packing Strategy for GPU-Based Verilog Simulation

## Overview

Bit-packing can provide significant performance improvements in GPU-based simulation, but **only when memory bandwidth is your bottleneck**. On GPUs, this is often the case, making bit-packing a valuable optimization—but one that should be applied selectively and strategically.

This document outlines when, where, and how to implement bit-packing in MetalFPGA.

---

## When Bit-Packing Helps A Lot

### High-Value Scenarios

1. **Huge counts of 1-bit state**
   - Valid flags
   - Dirty bits
   - Event pending bits
   - Monitor dirty flags
   - Ready/block bits
   - Process state flags (READY/BLOCKED/DONE)

2. **You're bandwidth-limited**
   - Reading/writing tons of scheduler state every dispatch
   - Memory transactions dominate execution time
   - Profiling shows high memory traffic

3. **Working set spills out of cache**
   - Smaller state representation means more fits in L2/unified cache
   - Fewer cache misses
   - Better locality for frequently-accessed state

### Why It Matters for Mature Simulators

Feature-complete simulators accumulate tons of metadata:
- Watchers and monitors
- X/Z tracking
- Net driver resolution
- NBA (non-blocking assignment) queues
- Debug/trace state

All this metadata creates memory pressure. Bit-packing keeps it under control.

---

## When Bit-Packing Can Hurt

### Anti-Patterns to Avoid

1. **Hot bits with frequent contention**
   - If many threads update bits in the same byte/word, you need read-modify-write
   - Requires atomic operations
   - Can completely wipe out bandwidth gains
   - **BUT**: In MetalFPGA, each `gid` owns its state, so this is mostly avoided ✅

2. **Trading 1 load/store for multiple ops**
   - Simple stores become "load → mask → shift → OR → store"
   - Can become ALU-bound instead of memory-bound
   - Bit shifts are cheap, but the overhead adds up

3. **Loss of coalescing/alignment**
   - A flat `uint` array is extremely GPU-friendly
   - Random bit addressing can turn into scattered byte traffic
   - Misaligned accesses kill performance

---

## The Sweet Spot: Pack Into 32-bit or 64-bit Words

### ❌ Don't Do This: Shared Bytes
```c
device uchar* packed_flags;  // BAD: alignment issues, byte atomics
```

### ✅ Do This: Word-Aligned Bitsets
```c
device uint* bitset_words;   // GOOD: 32-bit aligned, coalesced access
device ulong* bitset_words;  // ALSO GOOD: 64-bit for wider lanes
```

### Why Words Are Better

- **Aligned**: Fits GPU memory transaction boundaries
- **Fewer memory transactions**: 32 bits per load vs 8
- **Fast bit operations**: Native support, no weird addressing
- **Better atomics**: 32-bit atomics are well-behaved on GPUs
- **Coalescing-friendly**: Adjacent threads access adjacent words

### Bit Indexing Pattern

```c
uint word_index = bit >> 5;           // Divide by 32
uint bit_mask = 1u << (bit & 31);     // Modulo 32
uint value = bitset_words[word_index] & bit_mask;
```

---

## Where to Use Packing in MetalFPGA

### 🟢 Excellent Candidates

These will give you measurable wins:

1. **`sched_event_pending`** (bitset per gid)
   ```c
   // Instead of: device uint sched_event_pending[gid * EVENT_COUNT + e]
   // Use: device uint event_pending_words[(gid * event_words) + word]
   uint event_words = (EVENT_COUNT + 31) / 32;
   ```

2. **Per-process state flags** (READY/BLOCKED/DONE)
   ```c
   // Pack into 2 bits per process
   // Can fit 16 processes per uint
   ```

3. **Monitor dirty flags**
   ```c
   // One bit per watched signal
   // Allows fast "which signals changed?" scans
   ```

4. **Service presence flags**
   ```c
   // "Does this gid need monitor service?"
   // "Does this gid need dump service?"
   ```

5. **X/Z state masks** (for 4-state logic)
   ```c
   // Separate bitplanes:
   //   - value_bits[...]
   //   - xz_mask_bits[...]
   ```

### 🟡 Maybe Candidates

Benchmark before committing:

- X/Z state bits (if kept as separate bitplanes)
- Sensitivity list flags
- Net driver conflict detection bits

### 🔴 Bad Candidates

Don't pack these—you'll just add overhead:

- **PC (program counter) values**: Multi-bit, frequently updated
- **`wait_time` values**: 64-bit timestamps
- **Indices/IDs**: Don't compress well
- **Numeric data**: Widths vary, no space savings

---

## Practical Implementation Plan

### Phase 1: Add Optional Packing (Later, Not Now)

```bash
./metalfpga_cli --pack-bits input.v
```

1. Add codegen option that packs **only 2-3 specific arrays** initially:
   - Event pending bits
   - Monitor dirty flags
   - Process state flags

2. Keep unpacked reference backend for:
   - Correctness checking
   - Debugging
   - Regression testing

3. Maintain both paths until packing proves valuable

### Phase 2: Benchmark Realistic Workloads

Test with varied scenarios:

- **Small design, huge `params.count`**: Many parallel instances
- **Large design, small `params.count`**: Complex single instance
- **Heavy monitor/dump activity**: Lots of metadata traffic

Profile to see if you're actually bandwidth-limited.

### Phase 3: Expand Based on Data

Only add more packing if Phase 2 shows clear wins (>10% speedup).

---

## Architecture for Unbound Buffers

Since MetalFPGA uses unbound buffers everywhere, bit-packing integrates naturally.

### Best Practices

#### 1. Keep "Raw Pointer + Offset" Helpers

Write reusable bit accessor functions:

```metal
inline uint get_bit(device uint* base, uint gid, uint stride_words, uint bit) {
    uint offset = gid * stride_words + (bit >> 5);
    uint mask = 1u << (bit & 31);
    return (base[offset] & mask) ? 1u : 0u;
}

inline void set_bit(device uint* base, uint gid, uint stride_words, uint bit) {
    uint offset = gid * stride_words + (bit >> 5);
    uint mask = 1u << (bit & 31);
    base[offset] |= mask;
}

inline void clear_bit(device uint* base, uint gid, uint stride_words, uint bit) {
    uint offset = gid * stride_words + (bit >> 5);
    uint mask = 1u << (bit & 31);
    base[offset] &= ~mask;
}
```

Benefits:
- Swap layouts later without rewriting codegen
- Consistent access patterns
- Easy to debug

#### 2. Prefer Word-Range Operations

**Old (unpacked):**
```metal
for (uint e = 0; e < EVENT_COUNT; ++e) {
    sched_event_pending[(gid * EVENT_COUNT) + e] = 0u;
}
```

**New (packed):**
```metal
uint event_words = (EVENT_COUNT + 31) / 32;
for (uint w = 0; w < event_words; ++w) {
    event_pending_words[(gid * event_words) + w] = 0u;
}
```

**Big win**: Fewer stores, better coalescing.

#### 3. Make Packing Optional During Development

```c++
#ifdef ENABLE_BIT_PACKING
    // Packed layout
    uint event_words = (EVENT_COUNT + 31) / 32;
    device uint* event_pending_words [[buffer(N)]];
#else
    // Unpacked layout (easier debugging)
    device uint* sched_event_pending [[buffer(N)]];
#endif
```

This is critical while `$monitor`, `$dumpvars`, and service plumbing evolve.

---

## Metal-Specific Considerations

### Alignment and Type Punning

**Avoid mixing types on the same buffer:**

```metal
// ❌ DON'T: Aliasing issues
device uint* as_words = buffer;
device uchar* as_bytes = (device uchar*)buffer;  // Dangerous!
```

**Stick to word-based access:**
- Keep everything `device uint*` or `device ulong*`
- Avoid byte-level punning
- Prevents alignment issues and codegen surprises

### Atomic Operations (If Needed)

If you ever need atomics (rare, since each `gid` owns its state):

```metal
// ✅ GOOD: 32-bit atomic
atomic_fetch_or_explicit((device atomic_uint*)&bitset[word], mask, memory_order_relaxed);

// ❌ BAD: Byte atomics are slower/limited
// Don't pack into bytes if you need atomics
```

---

## High-Impact Use Case: Dirty Tracking

Once feature-complete, you'll have:
- Tons of per-signal/per-process metadata
- Lots of "dirty" tracking to avoid scanning everything

### Bitsets Enable Efficient Iteration

Instead of:
```metal
// Scan all signals every time
for (uint sig = 0; sig < SIGNAL_COUNT; ++sig) {
    if (signal_changed[sig]) {
        // Process
    }
}
```

Use bitsets + bit scanning:
```metal
// Scan only set bits
for (uint w = 0; w < signal_words; ++w) {
    uint word = changed_words[w];
    while (word != 0) {
        uint bit = ctz(word);           // Count trailing zeros
        uint sig = (w * 32) + bit;
        // Process signal sig
        word &= ~(1u << bit);           // Clear processed bit
    }
}
```

This can be a **bigger win than RAM savings** for sparse updates.

---

## 4-State Logic: Bitplane Representation

If supporting X/Z values properly:

### Separate Bitplanes
```metal
device uint* value_bits;      // Actual 0/1 values
device uint* xz_mask_bits;    // 1 = X or Z, 0 = known
```

Benefits:
- Compresses cleanly
- SIMD-friendly operations
- Fast propagation: `result_xz = a_xz | b_xz;`
- Natural for GPU parallelism

### Operations
```metal
// AND with X/Z propagation
uint result_val = a_val & b_val;
uint result_xz = a_xz | b_xz;  // X/Z spreads
```

---

## Recommended Timeline

| Phase | Focus | When |
|-------|-------|------|
| **Now** | Get Verilog working unpacked | Months 0-2 |
| **Soon** | GPU runtime, basic codegen | Months 2-6 |
| **Later** | Add packing for events + flags | Months 6-9 |
| **Much Later** | Full 4-state with bitplanes | Months 9-12 |
| **Future** | Advanced dirty tracking | Months 12+ |

Don't optimize prematurely. Bit-packing is a **mature simulator optimization**, not a bootstrapping requirement.

---

## Metrics to Watch

Profile for these before/after packing:

1. **Memory bandwidth utilization**: `Metal System Trace`
2. **Cache hit rates**: L2 hit percentage
3. **Occupancy**: Are you memory-bound or compute-bound?
4. **Execution time**: Wall-clock improvement (target >10%)

If you're **compute-bound** (ALU at 100%, memory idle), bit-packing won't help much.

If you're **memory-bound** (memory at 100%, ALU idle), packing can give 2-4x gains.

---

## Questions to Answer Before Packing

1. **What is `params.count` in your runtime?**
   - Instances? Test vectors? Parallel lanes?

2. **Typical design scale?**
   - `PROC_COUNT`: How many processes?
   - `EVENT_COUNT`: How many events?
   - Signal count: How many watched signals?

3. **Profiling data:**
   - What % of time is memory access?
   - What's the working set size?
   - Does it fit in cache?

With these answers, we can pinpoint **exact arrays** to pack for maximum gain.

---

---

## Core Invariants for Bit-Packed State

These invariants are **first-class requirements**. Future optimizations must preserve these semantics. Violating these invariants will silently break correctness.

### Ownership and Concurrency

**I1 — Single-writer per gid region**
- For any `gid`, only the thread handling that `gid` writes that gid's packed words during a dispatch
- **Consequence**: No atomics required for bitset set/clear in the hot path
- **Verification**: Static analysis or runtime bounds checking in debug builds

**I2 — No cross-gid writes**
- A thread must never write another gid's packed region
- **Consequence**: Enables lock-free parallel execution
- **Verification**: Buffer addressing must be `base + (gid * stride) + offset` only

**I3 — Host does not mutate device bitsets mid-dispatch**
- Host updates occur only between dispatches (unless explicitly synchronized)
- **Consequence**: No device-host race conditions
- **Verification**: Host-side state updates gated by dispatch completion

### Layout and Addressing

**I4 — Word-addressable packing**
- All packed bitsets use 32-bit `uint` words (aligned), never byte packing
- **Rationale**: GPU memory coalescing, atomic availability, alignment
- **Consequence**: `sizeof(packed_word) == 4`, `alignof == 4`

**I5 — Deterministic indexing**
- `word_index = bit >> 5`, `mask = 1u << (bit & 31)` is the **only** allowed mapping
- **Rationale**: Consistency, debuggability, no surprises
- **Consequence**: All bit access uses this formula, no variations

**I6 — Stable stride**
- For a given generated kernel, each packed structure has a fixed `words_per_gid` stride
- No variable-length per-gid packing
- **Consequence**: `stride = (count + 31) / 32` computed at codegen time, constant per kernel
- **Verification**: Static assert that `words_per_gid` matches codegen constant

### Semantics Tied to Scheduler Phases

**I7 — Event pending is edge-like within a timestep**
- `event_pending` bits may be set during ACTIVE phase
- Bits are *consumed* during the unblock phase
- Bits are cleared exactly once per timestep boundary
- **Consequence**: Events are level-to-edge converted by the scheduler
- **Verification**: Check that pending bits never "stick" across timesteps

**I8 — Clear point is explicit and unique**
- The scheduler has **one canonical point** where it clears `event_pending` (and other "pulse" bits)
- Never clear in multiple places
- **Consequence**: No race between set and clear
- **Location**: End of NBA phase, before time advancement

**I9 — No immediate wakeups outside the unblock boundary**
- A waiter blocked on an event transitions to READY **only** during the unblock scan phase
- Setting the bit alone never mutates process state
- **Consequence**: Deferred wakeup semantics, matches Verilog LRM
- **Verification**: Process state changes only in designated phase

### Correctness and Observability

**I10 — Bitset operations are pure transforms**
- `set/clear/test` must not have side effects beyond the target word(s)
- **Consequence**: Operations are composable and reorderable within constraints
- **Verification**: Helper functions marked `inline` and side-effect free

**I11 — Debug equivalence**
- Packed and unpacked representations must produce **identical** observable behavior:
  - Same wakeups at same times
  - Same STOPPED/FINISHED decisions
  - Same service emission ordering
  - Same final state
- **Consequence**: Unpacked mode is the reference implementation
- **Verification**: Dual-mode CI tests, shadow checking

**I12 — No silent overflow**
- If packed structures overflow (e.g., change lists, service queues):
  - Must become a scheduler error or STOP condition
  - Never silently drop data
- **Consequence**: Fail-fast on resource exhaustion
- **Verification**: Bounds checking in critical paths

### ABI Stability (Device ↔ Host, Codegen ↔ Runtime)

**I13 — Versioned structures**
- Every packed block that crosses a boundary (service records especially) is versioned
- Decoded by version, not by compiler struct layout
- **Consequence**: Forward/backward compatibility possible
- **Format**: `struct Header { uint32_t version; uint32_t size; ... }`

**I14 — No reliance on struct packing**
- Layout defined in terms of **explicit word offsets**, not C/C++ struct packing rules
- **Rationale**: Cross-language ABI (Metal ↔ C++), cross-compiler compatibility
- **Consequence**: Manual offset calculations, static assertions to verify
- **Example**: `offset_events = 0; offset_dirty = event_words; ...`

---

## Engineering Guardrails

These practices **enforce the invariants** and must be followed:

### G1 — One Helper API for All Bit Access

**Requirement**: All bit operations go through a single, audited API

```metal
// bit_ops.h - THE ONLY PLACE FOR BIT MANIPULATION
inline uint get_bit(device uint* base, uint word_offset, uint bit);
inline void set_bit(device uint* base, uint word_offset, uint bit);
inline void clear_bit(device uint* base, uint word_offset, uint bit);
inline void clear_word_range(device uint* base, uint start_word, uint count);
```

**Benefits**:
- Audit once, use everywhere
- Easy to swap implementations (instrumentation, shadow checking)
- Enforces I5 (deterministic indexing)

**Verification**: `grep -r "1u <<" *.metal` should only hit bit_ops.h

### G2 — Static Asserts on Word Counts/Offsets

**Requirement**: Host and device must agree on layout

```cpp
// Host side (C++)
constexpr uint32_t EVENT_WORDS = (EVENT_COUNT + 31) / 32;
static_assert(EVENT_WORDS * 32 >= EVENT_COUNT, "Insufficient event words");

// Device side (Metal)
constant uint GPGA_EVENT_WORDS = (GPGA_EVENT_COUNT + 31u) / 32u;
static_assert(GPGA_EVENT_WORDS == 5u, "Event word count mismatch"); // Codegen fills in expected value
```

**Benefits**:
- Catch layout drift at compile time
- Document assumptions
- Enforce I6 (stable stride)

**Verification**: Build breaks on mismatch, not runtime failures

### G3 — Invariant Testbench Suite

**Requirement**: Small, focused tests for each invariant

Minimal tests that must pass in both packed and unpacked modes:

1. **Event set/unblock/clear behavior** (I7, I8, I9)
   ```verilog
   module test_event_semantics;
     event e;
     reg flag;
     initial begin
       flag = 0;
       @(e);  // Block
       flag = 1;  // Should set after trigger
     end
     initial begin
       #10 -> e;  // Trigger at time 10
       #1;  // Time 11
       if (flag != 1) $error("Event wakeup failed");
     end
   endmodule
   ```

2. **`$stop` halts dispatch but preserves state** (I3, I12)
   ```verilog
   module test_stop_state;
     reg [7:0] counter;
     initial begin
       counter = 0;
       #10 counter = 5;
       $stop;  // Pause here
       #10 counter = 10;  // Should resume from counter=5
     end
   endmodule
   ```

3. **`$monitor` fires at most once per timestep** (I7)
   ```verilog
   module test_monitor_once;
     reg [7:0] val;
     initial begin
       $monitor("val=%d", val);
       val = 0;
       val = 1;  // Same timestep - should see 1, not 0 then 1
       #1 val = 2;  // Next timestep
     end
   endmodule
   ```

**Benefits**:
- Codifies expected behavior
- Regression detection
- Documents semantics

**Verification**: CI runs both packed and unpacked, compares outputs

### G4 — Optional Shadow Mode

**Requirement**: In debug builds, maintain unpacked mirrors for validation

```metal
#ifdef ENABLE_SHADOW_CHECKING
device uint* event_pending_packed [[buffer(N)]];
device uint* event_pending_unpacked [[buffer(N+1)]];  // Shadow copy

// After every bitset operation:
if (get_bit(event_pending_packed, ...) != event_pending_unpacked[...]) {
    sched_error[gid] = ERROR_SHADOW_MISMATCH;
}
#endif
```

**Benefits**:
- Catches packing bugs immediately
- Validates I11 (debug equivalence)
- Can be disabled in production

**Cost**: 2x memory, slower, but only in debug

**Verification**: Shadow mismatches halt simulation with diagnostic

---

## Checklist Before Enabling Bit-Packing

Use this checklist when introducing bit-packing to a new structure:

- [ ] **Invariants documented**: Which invariants apply to this structure?
- [ ] **Helper API used**: All access goes through bit_ops.h?
- [ ] **Static asserts added**: Host/device layout agreement verified?
- [ ] **Testbench written**: Invariant test covers this structure?
- [ ] **Shadow mode tested**: Runs in shadow mode without errors?
- [ ] **Profiling data**: Confirmed memory bandwidth is the bottleneck?
- [ ] **Unpacked fallback**: Debug mode still available?
- [ ] **Performance gain**: Measured >10% improvement?

If any checkbox is unchecked, **don't ship the optimization**.

---

## Invariant Violation Recovery

### Detection

- **Compile-time**: Static asserts fail
- **Runtime (debug)**: Shadow mode detects mismatch
- **Runtime (production)**: Scheduler error codes, `sched_error[gid]`

### Response

1. **Fail-fast**: Set `sched_status = STATUS_ERROR`
2. **Preserve state**: Don't corrupt other gids
3. **Diagnostic**: Log which invariant failed (error code)
4. **Recovery**: Host can inspect state, dump diagnostics

### Error Codes

```metal
constexpr uint ERROR_NONE = 0u;
constexpr uint ERROR_SHADOW_MISMATCH = 1u;
constexpr uint ERROR_OVERFLOW = 2u;
constexpr uint ERROR_INVALID_BIT_INDEX = 3u;
constexpr uint ERROR_CROSS_GID_WRITE = 4u;  // Debug only
```

---

## Summary

### ✅ Do This

- Pack event pending bits into 32-bit words
- Pack monitor dirty flags
- Pack process state flags
- Keep word-aligned (not byte-aligned)
- Make packing optional during development
- Profile before expanding
- **Follow all invariants (I1-I14)**
- **Implement all guardrails (G1-G4)**
- **Use the checklist before shipping**

### ❌ Don't Do This

- Pack everything blindly
- Use byte-level packing
- Pack numeric data (PC, timestamps, indices)
- Optimize prematurely
- Forget to keep an unpacked debug mode
- **Violate any invariant "just this once"**
- **Skip static asserts "because it's obvious"**
- **Ship without shadow-mode testing**

### 🎯 Expected Gains

For a mature, feature-complete simulator:
- **2-4x reduction** in scheduler state size
- **1.5-2x speedup** if memory-bandwidth limited
- **Better cache utilization** for large designs
- **Faster iteration** over sparse change sets

### 🛡️ Correctness Guarantees

With invariants enforced:
- **Deterministic**: Same input → same output, every time
- **Debuggable**: Unpacked mode is reference, shadow mode validates
- **Maintainable**: Invariants prevent "clever" optimizations that break semantics
- **Verifiable**: Tests codify expected behavior

But remember: **First make it work, then make it fast.** Bit-packing is a powerful tool for the optimization phase, not the implementation phase. And when you do optimize, **invariants are not optional**—they're the foundation of correctness.
