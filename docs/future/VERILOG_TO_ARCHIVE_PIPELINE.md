# Verilog to Archive Pipeline

## Overview

MetalFPGA compiles Verilog HDL into native GPU binaries via a multi-stage pipeline. The final artifact is a `.mtl4archive` file containing pre-compiled Metal compute pipelines - analogous to Icarus Verilog's `.vvp` files, but containing native GPU machine code instead of interpreted bytecode.

---

## Pipeline Stages

```
┌─────────────────────────────────────────────────────────────────┐
│                    VERILOG SOURCE                               │
│                    design.v                                     │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│              STAGE 1: VERILOG COMPILATION                       │
│              metalfpga_cli                                      │
│                                                                 │
│  • Parse Verilog-2005 HDL                                       │
│  • Elaborate hierarchy                                          │
│  • Flatten netlist                                              │
│  • Generate MSL + Host C++                                      │
└────────────┬────────────────────────────┬───────────────────────┘
             │                            │
             ▼                            ▼
┌─────────────────────┐      ┌─────────────────────────┐
│   design.msl        │      │   design_host.cc        │
│                     │      │                         │
│  Metal Shading      │      │  Host runtime:          │
│  Language source    │      │  • Buffer management    │
│  (GPU kernel)       │      │  • Scheduler loop       │
│                     │      │  • Service system       │
│                     │      │  • VCD writer           │
└──────────┬──────────┘      └────────┬────────────────┘
           │                          │
           │                          ▼
           │              ┌─────────────────────────┐
           │              │   clang++               │
           │              │   -framework Metal      │
           │              │   -framework Foundation │
           │              └────────┬────────────────┘
           │                       │
           │                       ▼
           │              ┌─────────────────────────┐
           │              │   design_sim            │
           │              │   (host executable)     │
           │              └────────┬────────────────┘
           │                       │
           └───────────────────────┤
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────┐
│              STAGE 2: METAL SHADER COMPILATION                  │
│              (async, 5-10 minutes for large designs)            │
│                                                                 │
│  • Metal shader compiler (Apple toolchain)                      │
│  • Optimizations: register allocation, SIMD, occupancy         │
│  • GPU binary generation (native Metal IR)                      │
│  • Pipeline state object creation                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│              STAGE 3: PIPELINE HARVESTING                       │
│              METALFPGA_PIPELINE_HARVEST=1                       │
│                                                                 │
│  • Serialize compiled pipeline state objects                    │
│  • Bundle kernel function pointers                             │
│  • Include buffer binding metadata                             │
│  • Save to archive file                                        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│              FINAL ARTIFACT                                     │
│              metal4_pipelines.mtl4archive                       │
│                                                                 │
│  • Pre-compiled Metal compute pipelines                         │
│  • Native GPU machine code                                      │
│  • Instant load (<5 seconds)                                    │
│  • Ready for distribution                                       │
└─────────────────────────────────────────────────────────────────┘
```

---

## Detailed Workflow

### Stage 1: Verilog Compilation (metalfpga_cli)

**Input**: Verilog-2005 HDL source files

**Command**:
```bash
./metalfpga_cli design.v \
  --emit-msl design.msl \
  --emit-host design_host.cc
```

**Outputs**:
1. **design.msl** - Metal Shading Language kernel source
   - Contains GPU kernel implementing the flattened Verilog design
   - Single mega-kernel for maximum parallelism
   - Includes all flip-flops, combinational logic, timing checks

2. **design_host.cc** - Host-side C++ runtime
   - Metal framework integration
   - Buffer management (signals, state, service records)
   - Scheduler loop (clock advancement, event handling)
   - VCD writer (waveform export)
   - System task handlers ($display, $finish, $monitor)

**Processing steps**:
1. Parse Verilog source → AST
2. Elaborate module hierarchy → flat netlist
3. Optimize netlist (constant folding, dead code elimination)
4. Generate MSL kernel (all logic in single GPU kernel)
5. Generate host runtime (buffer setup, dispatch loop)

---

### Stage 2: Metal Shader Compilation

**Input**: design.msl (MSL source code)

**Command**:
```bash
# Build host executable
clang++ design_host.cc -o design_sim \
  -framework Metal \
  -framework Foundation \
  -std=c++17 \
  -O3

# Run with async compilation (Phase 1 optimization)
METALFPGA_PIPELINE_ASYNC=1 ./design_sim design.msl
```

**Processing** (inside Metal runtime):
1. **Metal shader compiler invocation**:
   - `newLibraryWithSource:options:completionHandler:`
   - Async compilation (doesn't block main thread)
   - Progress reporting (shows compile status)

2. **Compiler optimizations**:
   - Register allocation (minimize register pressure)
   - SIMD vectorization (parallel arithmetic)
   - Occupancy tuning (maximize GPU utilization)
   - Dead code elimination (remove unused signals)
   - Constant propagation (fold compile-time constants)

3. **Pipeline state object creation**:
   - Compile MSL kernel → GPU binary
   - Create `MTLComputePipelineState`
   - Cache pipeline for reuse

**Performance**:
- **Small designs** (test_clock_big_vcd): ~50ms compile time
- **Medium designs** (RISC-V core): ~5-10 minutes compile time
- **Large designs** (NES core): ~5-10 minutes compile time

**Problem** (current): Synchronous compilation blocks XPC connection on large designs
**Solution** (Phase 1): Async compilation + pipeline harvesting

---

### Stage 3: Pipeline Harvesting

**Input**: Compiled Metal pipeline (in-memory `MTLComputePipelineState`)

**Command**:
```bash
# Enable pipeline harvesting on first run
METALFPGA_PIPELINE_HARVEST=1 ./design_sim design.msl
```

**Processing**:
1. **Pipeline serialization**:
   - Extract compiled pipeline state
   - Serialize GPU binary (Metal IR)
   - Include buffer binding metadata
   - Store kernel function pointers

2. **Archive format**:
   - Container: NSKeyedArchiver (Apple standard) or custom binary format
   - Contents:
     - Pipeline state objects (GPU binaries)
     - Kernel signatures (entry points)
     - Buffer layout (argument table descriptors)
     - Metadata (kernel name, specialization constants)

3. **Write to disk**:
   - Default location: `artifacts/pipeline_cache/metal4_pipelines.mtl4archive`
   - Release location: `~/Library/Caches/metalfpga/metal4_pipelines.mtl4archive`
   - Custom path: `METALFPGA_PIPELINE_ARCHIVE=/path/to/archive.mtl4archive`

**Output**: `metal4_pipelines.mtl4archive` (pre-compiled GPU binaries)

---

### Stage 4: Archive Loading (Instant Startup)

**Input**: metal4_pipelines.mtl4archive

**Command**:
```bash
# Load pre-compiled pipelines (instant startup)
METALFPGA_PIPELINE_ARCHIVE_LOAD=1 ./design_sim design.msl
```

**Processing**:
1. **Archive deserialization**:
   - Load archive from disk (~5 seconds)
   - Deserialize pipeline state objects
   - Restore kernel function pointers
   - Rebuild buffer binding tables

2. **Pipeline cache population**:
   - Cache loaded pipelines in-memory
   - Skip shader compilation entirely
   - Ready for immediate GPU dispatch

3. **Simulation execution**:
   - Zero compilation overhead
   - Instant simulation start
   - Full GPU performance

**Performance**:
- **Archive load time**: ~5 seconds (all design sizes)
- **Compilation time**: 0ms (skipped)
- **Total startup**: ~5 seconds (vs. 5-10 minutes without cache)

---

## Comparison to Other Simulators

### Icarus Verilog (CPU-based interpreted)

```
design.v → iverilog → design.vvp → vvp design.vvp
         (compile)   (bytecode)    (interpret on CPU)
```

**Pros**: Fast compile, portable bytecode
**Cons**: Interpreted execution (slow), CPU-only (no GPU acceleration)

### Verilator (CPU-based compiled)

```
design.v → verilator → design.cpp → g++ → design_sim
         (translate)  (C++ code)   (compile) (native binary)
```

**Pros**: Fast execution (native CPU code), cycle-accurate
**Cons**: Long compile times, CPU-only (no GPU), limited parallelism

### MetalFPGA (GPU-based compiled)

```
design.v → metalfpga_cli → design.msl → Metal Compiler → metal4_pipelines.mtl4archive
         (translate)      (GPU code)   (GPU binary)    (cached pipelines)
```

**Pros**:
- Native GPU execution (massive parallelism)
- Cycle-accurate (it's the actual RTL)
- Instant reload (cached pipelines)
- Distributable (ship .mtl4archive with apps)

**Cons**:
- First compile is slow (5-10 min for large designs)
- Apple Silicon / Metal-only (not portable to other GPUs)

---

## Environment Variables

### Pipeline Caching

| Variable | Default | Description |
|----------|---------|-------------|
| `METALFPGA_PIPELINE_ARCHIVE` | `artifacts/pipeline_cache/metal4_pipelines.mtl4archive` (debug) or `~/Library/Caches/metalfpga/metal4_pipelines.mtl4archive` (release) | Override archive path |
| `METALFPGA_PIPELINE_ARCHIVE_LOAD=1` | Off | Load pipelines from archive (instant startup) |
| `METALFPGA_PIPELINE_ARCHIVE_SAVE=1` | Off | Save pipelines to archive on shutdown |
| `METALFPGA_PIPELINE_HARVEST=1` | Off | Alias for `ARCHIVE_SAVE=1` |
| `METALFPGA_PIPELINE_ASYNC=1` | Off | Enable async pipeline compilation (don't block) |
| `METALFPGA_PIPELINE_PRECOMPILE=1` | Off | Alias for `ASYNC=1` |
| `METALFPGA_PIPELINE_LOG=1` | Off | Log archive load/save operations |

### Example Workflows

**First run (harvest pipelines)**:
```bash
METALFPGA_PIPELINE_ASYNC=1 \
METALFPGA_PIPELINE_HARVEST=1 \
METALFPGA_PIPELINE_LOG=1 \
  ./design_sim design.msl
```

**Subsequent runs (load cached pipelines)**:
```bash
METALFPGA_PIPELINE_ARCHIVE_LOAD=1 \
  ./design_sim design.msl
```

**Custom archive location**:
```bash
METALFPGA_PIPELINE_ARCHIVE=/tmp/my_design.mtl4archive \
METALFPGA_PIPELINE_HARVEST=1 \
  ./design_sim design.msl
```

---

## Distribution Strategy

### For Standalone Applications

**Build workflow**:
```bash
# Developer (compile once)
./metalfpga_cli nes.v --emit-msl nes.msl --emit-host nes_host.cc
clang++ nes_host.cc -o nes_sim -framework Metal -framework Foundation
METALFPGA_PIPELINE_HARVEST=1 ./nes_sim nes.msl

# Package into .app bundle
./metalfpga_cli nes.v \
  --bundle NES.app \
  --pipeline-archive metal4_pipelines.mtl4archive \
  --template emulator

# Distribute NES.app (includes pre-compiled pipelines)
```

**User experience**:
```bash
# User downloads NES.app
# Double-click to launch
# → Loads metal4_pipelines.mtl4archive (~5 seconds)
# → Instant playable NES emulator!
```

### For Development Workflows

**CI/CD pipeline**:
```bash
# Stage 1: Compile Verilog (fast, deterministic)
./metalfpga_cli $DESIGN.v --emit-msl $DESIGN.msl --emit-host $DESIGN_host.cc

# Stage 2: Build host executable (fast)
clang++ $DESIGN_host.cc -o $DESIGN_sim -framework Metal -framework Foundation

# Stage 3: Harvest pipelines (slow, cache this artifact)
METALFPGA_PIPELINE_HARVEST=1 \
METALFPGA_PIPELINE_ARCHIVE=artifacts/$DESIGN.mtl4archive \
  ./$DESIGN_sim $DESIGN.msl

# Stage 4: Cache artifact for subsequent runs
# → artifacts/$DESIGN.mtl4archive can be committed to repo or uploaded to CI cache
```

**Developer workflow**:
```bash
# First time (or after Verilog changes)
make clean && make harvest  # Regenerate .mtl4archive

# Every subsequent run (instant)
make run  # Loads cached .mtl4archive
```

---

## Archive File Format

### Proposed Structure

```
metal4_pipelines.mtl4archive (binary container)
├── Header
│   ├── Magic: "MTL4ARCH" (8 bytes)
│   ├── Version: 1 (uint32)
│   ├── Kernel count: N (uint32)
│   └── Reserved (32 bytes)
│
├── Kernel 1
│   ├── Name: "main_kernel" (null-terminated string)
│   ├── Binary size: S (uint64)
│   ├── Binary data: [S bytes of Metal IR / GPU machine code]
│   ├── Argument table descriptor:
│   │   ├── Buffer count: B (uint32)
│   │   ├── Buffer 0: {index: 0, size: 1024, type: "device"}
│   │   ├── Buffer 1: {index: 1, size: 512, type: "constant"}
│   │   └── ...
│   └── Metadata:
│       ├── Threadgroup size: (uint32, uint32, uint32)
│       └── Specialization constants: {...}
│
└── Kernel N
    └── ...
```

### Serialization Options

**Option 1: NSKeyedArchiver (Apple standard)**
- Pros: Built-in, versioning support, automatic validation
- Cons: Less control over format, potential overhead

**Option 2: Custom Binary Format**
- Pros: Compact, fast, full control
- Cons: Manual versioning, more implementation work

**Recommendation**: Start with NSKeyedArchiver (easier), optimize to custom format if needed.

---

## Future Optimizations

### Incremental Compilation
- Hash Verilog source → detect changes
- Only recompile changed modules
- Reuse cached pipelines for unchanged modules

### Pipeline Specialization
- Multiple archives for different configurations:
  - `metal4_pipelines_4state.mtl4archive` (4-state logic enabled)
  - `metal4_pipelines_2state.mtl4archive` (2-state logic, faster)
  - `metal4_pipelines_debug.mtl4archive` (debug info, VCD enabled)

### Compression
- Compress GPU binaries (Metal IR is compressible)
- Target: 50-70% size reduction
- Trade-off: ~10-20% slower load time

### Cloud Compilation
- Upload `.v` → compile in cloud → download `.mtl4archive`
- Benefits: Offload slow compilation, share cache across team
- Use case: CI/CD, large designs

---

## Analogies

### Programming Languages

| Stage | C Compiler | Icarus Verilog | MetalFPGA |
|-------|------------|----------------|-----------|
| Source | `.c` | `.v` | `.v` |
| Intermediate | `.s` (assembly) | `.vvp` (bytecode) | `.msl` (MSL) |
| Binary | `.o` (object) | N/A | `.mtl4archive` (GPU binary) |
| Executable | `a.out` | `vvp` (interpreter) | `design_sim` (host + GPU) |
| Execution | Native CPU | Interpreted CPU | Native GPU |

### Game Engines

| Asset | Unreal Engine | Unity | MetalFPGA |
|-------|---------------|-------|-----------|
| Source | `.fbx` (3D model) | `.prefab` | `.v` (Verilog) |
| Intermediate | `.uasset` | `.asset` | `.msl` |
| Compiled | `.pak` (packaged) | `.assetbundle` | `.mtl4archive` |
| Runtime | Unreal Player | Unity Player | `design_sim` |
| Load Time | Instant (cached) | Instant (cached) | Instant (cached) |

---

## Summary

The **Verilog to Archive Pipeline** transforms human-readable HDL into native GPU binaries via a multi-stage compilation process:

1. **Verilog → MSL + Host** (metalfpga_cli, fast)
2. **MSL → GPU Binary** (Metal shader compiler, slow)
3. **GPU Binary → Archive** (pipeline harvesting, one-time)
4. **Archive → Runtime** (instant load, cached)

The `.mtl4archive` file is the **distribution artifact** - it enables instant startup, native GPU performance, and seamless app bundling. This is MetalFPGA's equivalent to Icarus Verilog's `.vvp` files, but with native GPU execution instead of interpreted CPU execution.

**Key benefit**: Compile once (slow), distribute forever (instant).
