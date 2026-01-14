# MetalFPGA: The Road Ahead

**Document Date**: January 10, 2026
**Current Version**: v0.9001
**Status**: Parallelism Phase Complete, Optimization & Feature Enhancement Phase

---

## Executive Summary

MetalFPGA has reached a critical inflection point. The foundational architecture is complete:
- Full Verilog-2005 parser and elaborator
- Metal 4 scheduler VM with bytecode execution
- Proc-parallel GPU execution (Phases 0-5 complete)
- Comprehensive testing infrastructure
- ~40k lines of production code

**The parallel scheduler is functional. Now we make it fast.**

This document charts the path from current state (v0.9001) through optimization, feature enhancement, real-world validation, and ultimately to the vision: **cycle-accurate hardware simulation at interactive speeds on Apple Silicon**.

---

## Current State Assessment

### What Works ✅

**Language Support:**
- Verilog-2005 parsing and elaboration
- Module hierarchy and instantiation
- Sequential and combinational logic
- Memory arrays and LUTs
- Generate blocks
- Parameters and localparams
- Timing delays
- Event controls (@posedge, @negedge, @*)

**GPU Execution:**
- Metal 4 scheduler VM
- Proc-parallel phases (ready eval, compact, exec)
- GPU-driven indirect dispatch
- Batched command encoding
- Service record ring buffer
- VCD output support
- 4-state logic primitives

**Infrastructure:**
- CMake build system
- GitHub Actions CI
- Comprehensive test suites (5,168 golden tests)
- Parallelism verification scripts
- Environment-based tuning controls

### What Needs Work 🚧

**Performance:**
- Still achieving hertz-level progress on PicoRV32
- GPU occupancy low (small ready lists)
- Many idle iterations advancing time
- Not yet hitting interactive framerates

**Missing Features:**
- Time-jump/next-event advance
- Multiple iterations per dispatch
- Real-time simulation mode (--sim)
- Advanced system tasks ($random, file I/O)
- Specify blocks (full timing constraints)

**Validation:**
- PicoRV32 compiles but runs very slowly
- Need more complex design validation
- Performance benchmarking incomplete
- Power user workflows undefined

---

## Strategic Priorities (Near-Term)

### 1. Performance: Break Through the Hertz Barrier 🎯 **CRITICAL**

**Problem**: Parallelism phases 0-5 are implemented but performance remains at hertz-level on complex designs like PicoRV32.

**Root Cause** (from tuning notes):
- Scheduler advances time with many idle iterations
- Ready lists are small (most procs blocked on time/edge waits)
- GPU parallelism underutilized per iteration
- Algorithmic cadence, not threadgroup sizing

**Solution Path:**

#### 1.1 Phase 6: Threadgroup Tuning & Verification
**Timeline**: 2-3 weeks
**Owner**: Core team

**Objectives:**
- Complete tuning sweep with `METALFPGA_COUNTS="1 2"` (not `COuNTS` typo!)
- Lock in optimal baseline: `exec_ready_tg=64`, `ready_tg=256`, `alias=off`
- Validate multi-sim behavior (count > 1)
- Document performance characteristics per design class

**Deliverables:**
- Updated [METAL4_PARALLELISM_TUNING_NOTES.md](METAL4_PARALLELISM_TUNING_NOTES.md)
- Baseline performance metrics table
- Environment variable defaults locked in
- GPU occupancy profiling data

**Success Criteria:**
- Reproducible tuning results
- Multi-sim throughput gain measured
- Documented "best practices" per workload type

---

#### 1.2 Time-Jump / Next-Event Advance 🚀 **GAME CHANGER**
**Timeline**: 4-6 weeks
**Owner**: Scheduler team
**Priority**: HIGHEST IMPACT

**Problem**: Huge numbers of idle iterations where `ready:0 blocked:25 done:20`

**Solution**: Compute minimum next wake time across all blocked processes and jump `sched_time` directly.

**Implementation Plan:**

**Phase A: Data Structure (Week 1)**
```metal
// Per-sim min wake time tracking
device uint64_t* sched_min_wake_time [[buffer(BIND_MIN_WAKE)]];

// Per-proc wake time (optional, for debugging)
device uint64_t* proc_wake_times [[buffer(BIND_PROC_WAKE)]];
```

**Phase B: Wake Time Computation (Week 2)**
```metal
kernel void gpga_MODULE_sched_compute_wake(
    device GpgaSchedulerState* sched_state,
    device uint64_t* proc_wake_times,
    device atomic_uint64_t* sched_min_wake_time,
    uint gid [[thread_position_in_grid]]) {

    // For each blocked proc, compute next wake time
    uint64_t wake = ComputeNextWakeTime(proc, sched);

    // Atomic min to find earliest wake across all procs
    atomic_fetch_min_explicit(&sched_min_wake_time[sim_id], wake, ...);
}
```

**Phase C: Time Jump Logic (Week 3)**
```metal
// In sched_step, after detecting no ready procs:
if (ready_count == 0 && sched.status == RUNNING) {
    uint64_t min_wake = sched_min_wake_time[gid];

    if (min_wake > sched.time && min_wake != UINT64_MAX) {
        // Jump time directly to next event
        sched.time = min_wake;
        // Reset min_wake for next iteration
        sched_min_wake_time[gid] = UINT64_MAX;
    }
}
```

**Phase D: Validation (Week 4)**
- VCD output must remain identical
- Event ordering preserved
- Delta cycles still handled
- No event skipped

**Phase E: Optimization (Weeks 5-6)**
- Reduce atomic contention (hierarchical min?)
- Coalesce wake time computation with ready eval
- Profile GPU time before/after

**Expected Impact:**
- **10-100x speedup** on designs with sparse events
- PicoRV32: hertz → KHz range
- Removes idle iteration bottleneck
- GPU occupancy still low but iterations productive

**Risks:**
- Correctness: event ordering, delta cycles
- Debugging: time jumps harder to trace
- Edge cases: infinite loops, stuck simulations

**Mitigation:**
- Comprehensive VCD comparison (automated)
- Debug mode: disable time-jump, compare outputs
- Max jump limit (prevent runaway)
- Telemetry: log jump sizes, frequency

---

#### 1.3 Multiple Iterations Per Dispatch 🔄
**Timeline**: 3-4 weeks (after 1.2)
**Owner**: Runtime team

**Problem**: CPU-GPU round trips every scheduler tick

**Solution**: Move scheduler loop onto GPU, run N iterations before CPU sync

**Implementation:**

```metal
kernel void gpga_MODULE_sched_multi_step(
    device GpgaSchedulerState* sched_state,
    constant GpgaSchedParams& params,
    uint gid [[thread_position_in_grid]]) {

    GpgaSchedulerState sched = sched_state[gid];

    // GPU-side loop: run many scheduler steps
    for (uint iter = 0; iter < params.iterations_per_dispatch; ++iter) {
        // Ready eval
        // Compact
        // Exec
        // Time advance or jump

        // Early exit conditions
        if (sched.status != RUNNING) break;
        if (service_buffer_full(gid)) break;
    }

    sched_state[gid] = sched;
}
```

**Host-Side Control:**
```cpp
// Adaptive batch sizing based on service buffer capacity
uint32_t iterations = CalculateIterations(service_capacity, drain_cadence);

params.iterations_per_dispatch = iterations;
```

**Expected Impact:**
- Reduce CPU waits by 10-100x
- Amortize dispatch overhead
- Service buffer becomes limiting factor (good problem!)

**Integration with Time-Jump:**
- Each iteration can jump time
- Effective "sim time per dispatch" increases massively
- PicoRV32: KHz → MHz range possible

---

### 2. Real-Time Simulation Mode (--sim) 🎮

**Timeline**: 2-3 weeks (parallel with 1.2)
**Owner**: Runtime team
**Depends on**: Time-jump implementation

**Vision**: Deadline-aware execution for interactive demos (NES, Game Boy cores)

**From [REALTIME_SIM_PLAN.md](REALTIME_SIM_PLAN.md):**

**New CLI Flags:**
```bash
metalfpga_cli nes.v --sim \
    --sim-rate-hz 1789773 \     # NES CPU clock
    --sim-speed 1.0 \            # Real-time
    --sim-headroom-ms 5 \        # Jitter buffer
    --sim-service-interval-ms 2  # Drain cadence
```

**Implementation:**

**A. Timebase Mapping:**
```cpp
class SimController {
    uint64_t start_wall_time_ns;
    uint64_t start_sim_time;
    double sim_rate_hz;
    double speed_multiplier;

    uint64_t ComputeTargetSimTime(uint64_t wall_elapsed_ns);
    uint32_t ComputeIterations(uint64_t backlog_time);
};
```

**B. Pacing Loop:**
```cpp
while (running) {
    // Compute how much sim time we should have reached
    uint64_t target_sim = controller.ComputeTargetSimTime(wall_elapsed);
    uint64_t current_sim = sched_state.time;

    // Adjust dispatch size based on drift
    if (current_sim < target_sim) {
        // Behind: increase iterations
        iterations = controller.CatchUp(target_sim - current_sim);
    } else {
        // Ahead: sleep
        controller.Sleep(current_sim - target_sim);
    }

    // Dispatch
    DispatchScheduler(iterations);

    // Drain service records at fixed cadence
    if (ShouldDrain()) DrainServiceRecords();
}
```

**C. I/O Ring Buffers:**
```cpp
// For NES demo
RingBuffer<uint8_t> audio_samples(48000);  // 1 sec buffer
RingBuffer<uint32_t> video_frames(3);      // Triple-buffer

// Surface to CoreAudio, Metal texture at fixed rates
```

**Expected Use Cases:**
- NES core: stable 60 Hz video, glitch-free audio
- Game Boy: real-time with turbo mode (speed > 1.0)
- Custom demos: interactive hardware visualization

**Success Criteria:**
- `--run` still deterministic, fast-as-possible
- `--sim` maintains deadline within headroom
- Drift warnings when sim falls behind
- Frame skipping optional, configurable

---

### 3. PicoRV32 Validation & Optimization 🔬

**Timeline**: Ongoing (2-4 weeks)
**Owner**: Validation team

**Current Status**: Compiles, runs, but incredibly slow

**Goals:**
1. Achieve **interactive framerate** execution
2. Validate correctness against known-good traces
3. Document performance characteristics
4. Use as benchmark for optimizations

**Milestones:**

**M1: Baseline Performance (Week 1)**
- Measure current throughput (instructions/sec)
- Profile GPU utilization
- Identify bottlenecks (ready list size, iteration count)
- Document diagnostics output

**M2: Time-Jump Integration (Week 3-4)**
- Apply time-jump optimization
- Measure speedup (expect 10-100x)
- Validate VCD output vs baseline
- Document performance gain

**M3: Multi-Iteration Integration (Week 6-7)**
- Apply multi-iteration optimization
- Target: KHz-MHz instruction rate
- Validate instruction traces
- Compare against Verilator, Icarus

**M4: Correctness Validation (Week 8)**
- Run PicoRV32 test suite
- Compare register traces
- Memory dump validation
- Instruction coverage analysis

**Success Criteria:**
- **Target**: 10 KHz+ instruction rate (vs current ~Hz)
- All tests pass
- VCD output matches reference
- Documented performance profile

---

## Strategic Priorities (Medium-Term)

### 4. Advanced Features & System Tasks 🛠️

**Timeline**: 4-8 weeks (after perf optimizations)

**Missing System Tasks:**
- `$random`, `$urandom` (RNG)
- `$fopen`, `$fwrite`, `$fclose` (File I/O)
- `$test$plusargs`, `$value$plusargs` (partially done)
- `$readmemh`/`$readmemb` (already implemented)
- `$dumpfile`/`$dumpvars` (VCD works, needs polish)

**Implementation Priority:**
1. **$random** - Many testbenches need this
2. **File I/O** - Validation, logging, data loading
3. **Plusargs** - Test parameterization
4. **Advanced VCD** - Selective dumping, performance

**Effort**: 1-2 weeks per feature

---

### 5. Comprehensive Testbench Execution 🧪

**Timeline**: Ongoing, 6-8 weeks

**Current**: Frontend tests pass, runtime partial

**Goals:**
1. All IEEE 1364-2005 tests execute (not just parse)
2. Golden test suite: 95%+ pass rate
3. Automated nightly regression
4. Performance regression tracking

**Test Tiers:**

**Tier 1: Smoke Tests (always run)**
- Basic combinational logic
- Simple sequential
- Clock generation
- VCD output

**Tier 2: Functional Tests (CI)**
- Module instantiation
- Memory arrays
- Generate blocks
- System tasks

**Tier 3: Complex Designs (nightly)**
- PicoRV32
- NES core components
- Large parameter sweeps
- Stress tests

**Infrastructure:**
- Automated test runner
- Result tracking database
- Performance metrics collection
- Regression alerts

---

### 6. Developer Experience & Documentation 📚

**Timeline**: Ongoing, 4-6 weeks

**Current Gaps:**
- Limited user documentation
- No quickstart guide
- Tuning opaque
- Error messages need improvement

**Deliverables:**

**6.1 User Documentation**
- Quick Start Guide (30 min to first success)
- CLI Reference (comprehensive flag docs)
- Performance Tuning Guide (when to use what)
- Troubleshooting Guide (common errors)

**6.2 Developer Documentation**
- Architecture Overview (how it all fits together)
- Contribution Guide (how to add features)
- Code Tour (key files and functions)
- Testing Guide (how to validate changes)

**6.3 Examples & Tutorials**
- `examples/00_blinky/` - Hello World
- `examples/01_counter/` - Sequential logic
- `examples/02_alu/` - Combinational design
- `examples/03_memory/` - RAM/ROM usage
- `examples/04_picorv32/` - Complex CPU
- `examples/05_nes_ppu/` - Real-world IP

**6.4 Error Handling**
- Better parse error messages (line numbers, context)
- Runtime error diagnostics (GPU timeout reasons)
- Suggestions for common mistakes
- Debug mode helpers

---

## Strategic Priorities (Long-Term)

### 7. The NES Flex Demo 🎯 **VISION MILESTONE**

**Timeline**: 3-4 months (after perf + features complete)
**Status**: Deferred until fundamentals solid
**From**: [FLEXGOAL.md](FLEXGOAL.md)

**Vision**: Super Mario Bros running via Verilog → Metal compilation

**Chain:**
```
NES Verilog RTL (6502 + PPU + APU)
    ↓
metalfpga Compiler
    ↓
Metal Compute Kernels
    ↓
Apple GPU Execution
    ↓
Playable NES Games @ 60 Hz
```

**Why This Matters:**
- **Proof of concept**: Real-world RTL compiles and runs
- **Marketing**: "We compiled Super Mario to GPU shaders"
- **Validation**: Handles 10K+ line designs
- **Educational**: Hardware → GPU mapping visible

**Prerequisites:**
1. ✅ Verilog-2005 complete
2. ✅ GPU execution working
3. 🚧 Performance adequate (>= real-time)
4. 🚧 System tasks complete ($readmemh, etc.)
5. ⏳ Real-time mode (--sim)
6. ⏳ I/O infrastructure (video/audio)

**Milestones:**

**M1: CPU Only (Month 1)**
- Compile 6502 core
- Execute instructions
- Validate against traces
- Terminal output only

**M2: CPU + PPU (Month 2)**
- Full NES system
- VCD waveform validation
- Frame timing correct
- No rendering yet

**M3: Full Emulation (Month 3)**
- Frame buffer extraction
- Metal texture rendering
- Controller input
- Audio via CoreAudio

**M4: Polish & Release (Month 4)**
- Performance optimization
- UI/UX for demo
- Video recording
- Blog post, conference talk

**Success Criteria:**
- Super Mario Bros playable at 60 Hz
- Cycle-accurate (VCD validates)
- Interactive (controller responsive)
- Stable (no crashes, glitches)

**Fallback Options** (if too ambitious):
- Chip-8 (simpler, ~500 lines)
- Game Boy (medium complexity)
- RISC-V core (modern, clean)

---

### 8. Multi-Sim Throughput Scaling 📈

**Timeline**: 2-3 months (after single-sim optimized)

**Vision**: Leverage GPU parallelism for **simulation farms**

**Use Case**: Design space exploration
```bash
metalfpga_cli design.v --run --count 1000 \
    --param-sweep "WIDTH=8:16:32:64" \
    --export-results results.json
```

**Benefits:**
- 1000 parameter combinations in one GPU dispatch
- Total throughput >> single-sim
- Perfect for regression, fuzzing, coverage

**Current State:**
- `--count > 1` works but not heavily tuned
- Parallelism plan focused on count=1 first

**Optimization Needed:**
- Load balancing across sims
- Per-sim overhead reduction
- Service record aggregation
- Result collection efficiency

**Expected Impact:**
- 100-1000x throughput for batch validation
- "GPU simulation farm" capability
- Enables genetic algorithms, fuzzing, coverage-driven testing

---

### 9. Portability & Ecosystem 🌍

**Timeline**: 6-12 months

**Current**: macOS Metal 4 only

**Goals:**
1. iOS/iPadOS support (same Metal 4 API)
2. Test on all Metal hardware (M1-M4, AMD GPUs)
3. Characterize performance across devices
4. App Store distribution (NES demo as standalone app)

**Phases:**

**9.1 iOS/iPadOS Port (Q2 2026)**
- Compile for iOS target
- Touch input mapping
- Performance validation
- App Store submission

**9.2 Hardware Characterization (Q3 2026)**
- M1, M2, M3, M4 benchmarks
- AMD GPU (Mac Pro, eGPU) testing
- Document "recommended hardware"
- Auto-tune for device

**9.3 App Bundling (Q3 2026)**
- Template: Verilog → standalone .app
- Auto-detect inputs/outputs
- Signal mapping UI
- App Store guidelines compliance

**9.4 Cloud Compilation Service (2027)**
- Web UI: upload Verilog → download .app
- Backend: metalfpga as a service
- Core database (community-contributed designs)
- GitHub Actions integration

---

### 10. SystemVerilog Subset ⚡

**Timeline**: 2027 and beyond

**Current**: Verilog-2005 complete

**Goals**: Subset of SystemVerilog-2012 features

**Priority Features:**
1. **Interfaces** - Clean module connections
2. **Always_comb, always_ff** - Intent clarity
3. **Logic type** - Better than reg/wire confusion
4. **Packed arrays** - `logic [7:0][3:0]`
5. **Enums** - Readable state machines
6. **Assertions** - Validation (subset)

**Non-Goals** (too complex for v1):**
- Classes, OOP
- Randomization
- Full UVM
- Coverage (maybe later)

**Effort**: 6-12 months

**Value**: Modern HDL designs use SystemVerilog, expanding compatibility

---

## Research Directions 🔬

### 11. Invariant-Driven Simulation (Long-Term Vision)

**From [JOURNEY.md](JOURNEY.md) philosophy:**

> "Define invariants. Enforce absolutely. Trust emergence."

**Thesis**: GPU kernel architecture generalizes beyond Verilog

**Potential Domains:**
1. **Quantum Circuit Simulation** (unitary evolution)
2. **Fluid Dynamics** (conservation laws)
3. **Molecular Dynamics** (ab-initio chemistry)
4. **Game Engines** (physics, causality)

**MetalFPGA as Proof-of-Concept:**
- Verilog invariants → GPU kernels → correct behavior
- Same methodology: formal constraints → kernel enforcement → emergence

**Future Projects:**
- **metalnullvector** (spacetime simulation, already planned)
- **metalquantum** (quantum gate simulation)
- **metalchem** (molecular dynamics)

**Common Architecture:**
- Event scheduler (repurposed)
- Ring buffer state storage
- Causal validation framework
- Metal kernel patterns

---

## Execution Strategy

### Team Structure (if scaling)

**Core Team:**
- Scheduler/Runtime Lead (performance, correctness)
- Frontend Lead (parser, elaborator, new features)
- Validation Lead (testing, golden tests, benchmarks)
- Documentation Lead (user docs, examples, guides)

**Current Reality**: Likely solo/small team, prioritize ruthlessly

### Prioritization Framework

**P0 - Critical Path (blocks everything):**
- Time-jump implementation
- Multi-iteration dispatch
- PicoRV32 validation

**P1 - High Impact (major value):**
- Real-time mode (--sim)
- System tasks completion
- Documentation & examples

**P2 - Medium Impact (nice to have):**
- Multi-sim tuning
- Error message improvements
- Advanced VCD features

**P3 - Future Work (deferred):**
- NES demo (wait until P0/P1 done)
- iOS port
- SystemVerilog

### Release Cadence

**v0.95 (Feb 2026)**: Time-Jump + Multi-Iteration
- Performance breakthrough
- PicoRV32 interactive
- Tuning complete

**v1.0 (Mar 2026)**: Production Ready
- System tasks complete
- 95%+ test pass rate
- Documentation comprehensive
- "Recommended for use"

**v1.5 (Jun 2026)**: Real-Time & Features
- --sim mode working
- NES demo (stretch goal)
- iOS support (if feasible)

**v2.0 (Q4 2026)**: Ecosystem
- App bundling
- Core database
- Cloud compilation (maybe)

**v3.0 (2027)**: SystemVerilog
- Modern HDL support
- Advanced features
- Community contributions

---

## Risk Assessment & Mitigation

### Technical Risks

**R1: Time-Jump Breaks Correctness**
- **Impact**: HIGH (invalidates all optimizations)
- **Probability**: MEDIUM (complex logic)
- **Mitigation**:
  - Exhaustive VCD comparison
  - Formal verification of event ordering
  - Gradual rollout with debug mode
  - Extensive test coverage

**R2: Performance Doesn't Improve Enough**
- **Impact**: HIGH (vision depends on it)
- **Probability**: LOW (architecture sound)
- **Mitigation**:
  - Multiple optimization paths (time-jump, multi-iter, multi-sim)
  - Profiling-driven development
  - Fallback to smaller demos if NES too slow

**R3: Metal API Changes (macOS updates)**
- **Impact**: MEDIUM (code churn)
- **Probability**: LOW (Metal 4 stable)
- **Mitigation**:
  - Stick to stable Metal 4 features
  - Test on beta macOS releases
  - Abstraction layer for Metal API

**R4: GPU Hardware Limitations**
- **Impact**: MEDIUM (some designs won't run)
- **Probability**: MEDIUM (unknown limits)
- **Mitigation**:
  - Document hardware requirements
  - Graceful degradation
  - CPU fallback mode (future)

### Resource Risks

**R5: Scope Creep**
- **Impact**: HIGH (never ship)
- **Probability**: HIGH (so much to do!)
- **Mitigation**:
  - Ruthless prioritization (P0 only until v1.0)
  - Fixed release dates
  - Feature freeze periods

**R6: Validation Completeness**
- **Impact**: HIGH (correctness issues)
- **Probability**: MEDIUM (complex designs)
- **Mitigation**:
  - Automated regression suite
  - Community beta testing
  - Gradual rollout of new features

---

## Success Metrics

### Technical Metrics

**Performance:**
- PicoRV32: **Target 10 KHz+ instruction rate** (vs current ~Hz)
- NES Core: **60 Hz sustained** (vs N/A)
- Simple designs: **> 1 MHz sim clock** (vs KHz)

**Correctness:**
- Golden test pass rate: **> 95%**
- VCD validation: **100% match on core tests**
- Regression rate: **< 1% per release**

**Coverage:**
- Verilog-2005: **> 90% language features**
- System tasks: **> 80% common tasks**
- IEEE compliance: **> 85% tests passing**

### User Metrics

**Adoption:**
- GitHub stars: 500+ (currently unknown)
- Active users: 50+ (current: 1-2)
- Community contributions: 10+ (current: 0)

**Documentation:**
- Quick start completion: **< 30 min** for new user
- Example coverage: **> 20 examples** across complexity levels
- Issue resolution time: **< 7 days** average

### Vision Metrics

**Demonstrable Impact:**
- NES demo: **Public video > 10K views**
- Conference talks: **> 2 accepted presentations**
- Blog posts: **> 3 technical deep-dives**
- Citations: **> 5 academic/industry references**

---

## The Road Map (Visual)

```
2026 Q1 (NOW → Mar)
├─ REV49: Phase 6 Tuning Complete ✓
├─ REV50: Time-Jump Implementation 🎯 CRITICAL
├─ REV51: Multi-Iteration Dispatch 🎯
└─ v0.95 Release: Performance Breakthrough

2026 Q2 (Apr → Jun)
├─ REV52-54: System Tasks & Features
├─ REV55: Real-Time Mode (--sim)
├─ PicoRV32 Validation Complete
└─ v1.0 Release: Production Ready 🎉

2026 Q3 (Jul → Sep)
├─ NES Demo Implementation (FLEXGOAL)
├─ iOS Port (if feasible)
├─ Documentation & Examples Polish
└─ v1.5 Release: Real-Time & Demos

2026 Q4 (Oct → Dec)
├─ Multi-Sim Optimization
├─ App Bundling Infrastructure
├─ Community Onboarding
└─ v2.0 Release: Ecosystem

2027
├─ SystemVerilog Subset
├─ Cloud Compilation Service
├─ Core Database
└─ v3.0 Release: Modern HDL
```

---

## Closing Thoughts

### The Paradigm Shift

MetalFPGA represents a fundamental shift in hardware simulation:

**Old Paradigm:**
- CPU-based event simulation
- Slow, sequential
- Approximation for speed (2-state, cycle-accurate)
- Proprietary tools ($$$)

**New Paradigm:**
- GPU-based parallel simulation
- Fast, massively parallel
- **Zero approximation** (4-state, event-accurate)
- Open source, Metal-native

### Why This Matters

**For FPGA Developers:**
- Fast prototyping before synthesis
- Parameter exploration at GPU speeds
- Educational: see hardware → GPU mapping

**For Retro Gaming:**
- Cycle-accurate cores → standalone apps
- No FPGA hardware required
- App Store distribution

**For Research:**
- Proof: GPU simulation scales
- Architecture: generalizes to other domains
- Foundation: metalfpga → metalnullvector → beyond

### The Vision, Refined

> **"From Verilog to Interactive Apps on Apple Silicon"**

Not just a simulator. A **compiler** that transforms hardware descriptions into GPU-accelerated applications.

Not just fast. **Correct**, with zero semantic approximation.

Not just a tool. A **paradigm** for invariant-driven simulation on GPUs.

### The Next 6 Months

**Feb 2026**: Time-jump breakthrough, PicoRV32 interactive
**Mar 2026**: v1.0 production release
**Jun 2026**: NES demo, real-time mode, iOS support
**Dec 2026**: Ecosystem, app bundling, community growth

**The parallel scheduler is functional.**
**Now we make it fast.**
**Then we make it beautiful.**
**Then we change how hardware is simulated.**

🚀 **Causality first. Always.** 🚀

---

**Document Ownership**: Core Team
**Review Cadence**: Monthly (update priorities)
**Next Review**: February 2026
**Feedback**: GitHub Discussions, Issues

---

## Appendix A: Quick Reference - What To Work On Now

**This Week:**
1. Fix `COuNTS` → `COUNTS` typo in tuning scripts
2. Complete count=2 validation runs
3. Lock in threadgroup defaults

**This Month:**
1. Time-jump implementation (Phases A-E)
2. VCD comparison automation
3. PicoRV32 baseline metrics

**This Quarter (Q1 2026):**
1. Time-jump + multi-iteration complete
2. PicoRV32 interactive framerate
3. v0.95 release

**This Year (2026):**
1. v1.0 production (Q1)
2. v1.5 real-time (Q2)
3. NES demo (Q3)
4. v2.0 ecosystem (Q4)

---

## Appendix B: Key Documents Reference

**Architecture & Design:**
- [JOURNEY.md](JOURNEY.md) - Origin story & philosophy
- [HOW_METALFPGA_WORKS.md](HOW_METALFPGA_WORKS.md) - Technical architecture
- [FLEXGOAL.md](FLEXGOAL.md) - NES demo vision

**Performance & Optimization:**
- [METAL4_SCHEDULER_VM_PARALLELISM_PLAN.md](METAL4_SCHEDULER_VM_PARALLELISM_PLAN.md) - Parallelism phases
- [METAL4_PARALLELISM_TUNING_NOTES.md](METAL4_PARALLELISM_TUNING_NOTES.md) - Current tuning results
- [REALTIME_SIM_PLAN.md](REALTIME_SIM_PLAN.md) - Real-time mode design

**Implementation:**
- [METAL4_RUNTIME_FLAGS.md](METAL4_RUNTIME_FLAGS.md) - Environment variables
- [SCHEDULER_VM_OPCODES.md](SCHEDULER_VM_OPCODES.md) - VM instruction set
- REV* series - Development checkpoints

**Testing & Validation:**
- `scripts/run_parallelism_verify.sh` - VCD comparison
- `scripts/run_parallelism_matrix.sh` - Parameter sweep testing
- `goldentests/` - IEEE compliance tests

---

**End of Roadmap**
*The journey continues...*
