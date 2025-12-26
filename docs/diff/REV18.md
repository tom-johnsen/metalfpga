# REV18 - README update and bit-packing strategy documentation

**Commit:** 06b0d48
**Date:** Fri Dec 26 14:49:49 2025 +0100
**Message:** Updated readme

## Overview

Documentation-only commit updating the README to reflect v0.4 development status and adding comprehensive **bit-packing strategy documentation** (717 lines). The bit-packing doc provides detailed guidance on GPU memory optimization techniques for the scheduler and 4-state logic systems, explaining when to pack bits, when not to, and how to implement packing efficiently on Metal GPUs.

## Pipeline Status

| Stage | Status | Notes |
|-------|--------|-------|
| **Parse** | ✓ Functional | No changes |
| **Elaborate** | ✓ Functional | No changes |
| **Codegen (2-state)** | ✓ MSL emission | No changes |
| **Codegen (4-state)** | ✓ Complete | No changes |
| **Host emission** | ✓ Enhanced | No changes |
| **Runtime** | ⚙ Partial | No changes |

## User-Visible Changes

**README Updates:**
- Status updated to **v0.4 (in development)**
- Test count updated to **174 passing tests** (from 114)
- High-priority roadmap items:
  - Event scheduling system (in development)
  - `time` data type and `$time` function
  - Named events (`event`, `->`)
- New documentation links:
  - Bit Packing Strategy
  - Verilog Reference
  - Async Debugging
- Expanded project structure documentation

**New Documentation:**
- **docs/bit_packing_strategy.md** (717 lines NEW)
  - GPU memory optimization guide
  - When bit-packing helps (and hurts)
  - Implementation patterns for Metal
  - Specific recommendations for MetalFPGA scheduler

## Architecture Changes

No architecture changes - documentation only.

## Documentation Added

### bit_packing_strategy.md (717 lines NEW)

Comprehensive guide on **GPU memory optimization** through bit-packing:

**Key sections:**

**When Bit-Packing Helps:**
- Huge counts of 1-bit state (flags, dirty bits, ready/block bits)
- Bandwidth-limited workloads (scheduler state traffic)
- Working set spills out of cache (metadata pressure)

**When Bit-Packing Hurts:**
- Hot bits with frequent contention (atomic overhead)
- Trading 1 load/store for multiple ops (ALU-bound instead of memory-bound)
- Loss of coalescing/alignment (scattered byte traffic)

**Sweet Spot: Word-Aligned Bitsets:**

```metal
// ❌ DON'T: Shared bytes
device uchar* packed_flags;  // BAD: alignment issues

// ✅ DO: Word-aligned bitsets
device uint* bitset_words;   // GOOD: 32-bit aligned, coalesced
device ulong* bitset_words;  // ALSO GOOD: 64-bit for wider lanes
```

**Excellent Candidates for MetalFPGA:**
1. **`sched_event_pending`** - Event flag bitset per instance
2. **Per-process state flags** - READY/BLOCKED/DONE (2 bits each)
3. **Monitor dirty flags** - Which signals changed this timestep
4. **Service presence flags** - Service needed indicators
5. **X/Z state masks** - 4-state logic bitplanes

**Bad Candidates:**
- PC (program counter) values - multi-bit, frequently updated
- `wait_time` values - 64-bit timestamps
- Indices/IDs - don't compress well
- Numeric data - variable width, no space savings

**Implementation Patterns:**

```metal
// Bit accessor helpers
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

// Word-range clearing (better than bit-by-bit)
uint event_words = (EVENT_COUNT + 31) / 32;
for (uint w = 0; w < event_words; ++w) {
    event_pending_words[(gid * event_words) + w] = 0u;
}
```

**Phased Implementation Plan:**

1. **Phase 1**: Add optional packing for 2-3 specific arrays
   - Event pending bits
   - Monitor dirty flags
   - Process state flags
   - Keep unpacked reference for validation

2. **Phase 2**: Benchmark realistic workloads
   - Small design, huge `params.count`
   - Large design, small `params.count`
   - Heavy monitor/dump activity
   - Profile for bandwidth bottlenecks

3. **Phase 3**: Expand based on data
   - Only add more packing if >10% speedup observed

**Specific Recommendations:**

**Event pending array:**
```metal
// Current (unpacked):
device uint* sched_event_pending;  // count * EVENT_COUNT entries

// Proposed (packed):
device uint* event_pending_words;   // count * event_words entries
// where event_words = (EVENT_COUNT + 31) / 32
```

**Process state (READY/BLOCKED/DONE):**
```metal
// Pack into 2 bits per process
// Can fit 16 processes per uint
uint state_words = (PROC_COUNT + 15) / 16;
```

**Why Word Alignment Matters:**
- Fits GPU memory transaction boundaries (32-byte or 64-byte)
- Fewer memory transactions (32 bits per load vs 8)
- Fast native bit operations
- Better atomic support (32-bit atomics well-behaved)
- Coalescing-friendly (adjacent threads → adjacent words)

**Anti-Pattern Warnings:**

```metal
// ❌ BAD: Byte-level packing
device uchar flags[count * FLAG_COUNT];  // Alignment hell

// ❌ BAD: Bit-level struct packing
struct __attribute__((packed)) ProcessState {
    uint pc : 24;      // Causes unaligned access
    uint state : 2;    // Compiler may generate byte ops
    uint wait : 3;
};

// ✅ GOOD: Word-aligned with manual bit operations
device uint* process_state_words;
// Extract state: (word >> (pid * 2)) & 0x3
```

**Debugging Considerations:**

```c++
// Make packing optional during development
#ifdef ENABLE_BIT_PACKING
    emit_packed_event_access(out, ...);
#else
    emit_unpacked_event_access(out, ...);
#endif
```

**Performance Expectations:**

- **Win scenarios**: 20-40% speedup when bandwidth-limited
- **Neutral scenarios**: ~5% overhead when compute-limited
- **Loss scenarios**: 2x slowdown if atomic contention introduced

**Document emphasizes:**
- Profile before optimizing
- Keep unpacked reference implementation
- Don't pack everything - be selective
- Word alignment is critical on GPUs
- Avoid atomic operations on packed words when possible

### README.md Updates

**Status section:**
```diff
-**Current Status**: v0.3 - Full generate/loop coverage...
+**Current Status**: v0.4 (in development) - Event scheduling
+system in progress...
```

**Test count:**
```diff
-114 passing test cases in `verilog/pass/`.
+174 passing test cases in `verilog/pass/`.
```

**High priority roadmap additions:**
- Event scheduling system (in development for v0.4)
- `time` data type and `$time` system function
- Named events (`event` keyword and `->` trigger)

**Medium priority additions:**
- `timescale` directive

**Documentation links added:**
- Bit Packing Strategy
- Verilog Reference
- Async Debugging

**Project structure expanded:**
```diff
 src/
   frontend/       # Verilog parser and AST
   core/           # Elaboration and flattening
+  ir/             # Intermediate representation
   codegen/        # MSL and host code generation
+  msl/            # Metal Shading Language backend
   runtime/        # Metal runtime wrapper
   utils/          # Diagnostics and utilities
 verilog/
-  pass/           # Passing test cases
+  pass/           # Passing test cases (174 files)
   test_*.v        # Additional test coverage
-docs/gpga/        # Documentation
+docs/
+  gpga/           # Core documentation
+  bit_packing_strategy.md  # GPU memory optimization
+  VERILOG_REFERENCE.md     # Language reference
+  ASYNC_DEBUGGING.md       # Async circuit debugging
```

## Test Coverage

No test changes - documentation only.

### Test Suite Statistics

- **verilog/pass/**: 174 files (no change)
- **verilog/**: 134 files (no change)
- **verilog/systemverilog/**: 19 files (no change)
- **Total tests**: 327 (no change)

## Implementation Details

No implementation - this commit is purely documentation.

**Purpose of bit-packing doc:**
- Future optimization guide for when scheduler becomes bandwidth-limited
- Prevent premature optimization mistakes (packing wrong things)
- Establish patterns for Metal GPU efficiency
- Document rationale for current unpacked implementation

**When to apply:**
- After runtime is complete and working
- When profiling shows memory bandwidth bottleneck
- As optional optimization flag (`--pack-bits`)

## Known Gaps and Limitations

No changes to implementation gaps - documentation only.

## Semantic Notes (v0.4)

**Bit-packing philosophy:**
- **Don't optimize prematurely** - unpacked is simpler and correct
- **Profile first** - measure bandwidth usage before packing
- **Pack selectively** - only high-value targets (flags, dirty bits)
- **Keep reference** - maintain unpacked codegen for validation
- **Word-align everything** - critical for GPU coalescing

**GPU memory characteristics (Metal):**
- 32-byte memory transactions (coalesced access)
- L2 cache shared across all threads
- High latency, high bandwidth (optimize for bandwidth)
- Atomic operations expensive (avoid on packed words)

**MetalFPGA-specific considerations:**
- Each `gid` owns its state (no cross-thread contention)
- Scheduler state accessed every dispatch (high traffic)
- Event pending flags sparse (good packing candidate)
- Process states small enum (2 bits sufficient)

## Statistics

- **Files changed**: 2
- **Lines added**: 734
- **Lines removed**: 4
- **Net change**: +730 lines

**Breakdown:**
- README: +21 lines, -4 lines = +17 net
- bit_packing_strategy.md: +717 lines (NEW)

**Documentation:**
- Comprehensive GPU optimization guide
- Metal-specific best practices
- MetalFPGA scheduler recommendations
- Phased implementation plan

This documentation-only commit provides critical guidance for future GPU memory optimization work. The bit-packing strategy doc (717 lines) is a comprehensive reference covering when to pack, when not to pack, and how to implement packing efficiently on Metal GPUs. The README updates reflect the project's progress to v0.4 and improved test coverage (174 passing tests).
