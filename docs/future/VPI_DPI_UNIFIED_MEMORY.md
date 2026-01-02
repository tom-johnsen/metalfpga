# VPI/DPI on Apple Unified Memory Architecture

**Status:** Design document
**Priority:** Post-runtime-optimization
**Unique Advantage:** Apple's unified memory makes low-latency VPI practical for the first time on GPU simulators

---

## Executive Summary

VPI (Verilog Procedural Interface) and DPI (Direct Programming Interface) are the industry-standard C/C++ APIs for co-simulation with Verilog/SystemVerilog. Traditional GPU simulators cannot efficiently implement VPI because discrete GPUs require expensive PCIe copies (10-50µs) for every signal access.

**Apple's unified memory architecture changes everything:** CPU and GPU share the same physical RAM via `MTLResourceStorageModeShared`. This enables **direct pointer access** to GPU signal buffers with ~20-100ns latency—a **100-1000x speedup** over discrete GPUs.

This document presents a practical implementation strategy for adding VPI/DPI to MetalFPGA, leveraging unified memory to provide:
- Zero-copy signal introspection
- Low-latency value read/write
- Standard VPI callback system
- Plugin-based extensibility

---

## Background: VPI vs DPI

### VPI (Verilog Procedural Interface)

**Standard:** IEEE 1364-2005 (Verilog) and IEEE 1800 (SystemVerilog)
**Purpose:** Full-featured C API for simulator introspection and control

**Capabilities:**
- **Design introspection:** Traverse module hierarchy, enumerate signals, query properties
- **Value access:** Read/write signal values at any simulation time
- **Callbacks:** Register functions triggered by events (value changes, time advances, simulation phases)
- **System tasks:** Implement custom `$my_task()` in C/C++
- **Co-simulation:** Connect external models (C/C++ BFMs, MATLAB, Python, etc.)

**Example Use Cases:**
1. **Waveform dumpers** (VCD/FSDB/Verdi)
2. **Bus functional models** (PCIe, DDR, Ethernet PHYs)
3. **CPU/software co-simulation** (RTL + firmware)
4. **Coverage collection** (functional coverage engines)
5. **Custom assertions** (C-based checkers)

**API Example:**
```c
// VPI callback: monitor clock signal
PLI_INT32 clk_monitor(p_cb_data cb_data) {
    vpiHandle clk = vpi_handle_by_name("top.clk", NULL);
    s_vpi_value value;
    value.format = vpiIntVal;
    vpi_get_value(clk, &value);

    if (value.value.integer == 1) {
        printf("Posedge at %llu\n", vpi_get_time(vpiSimTime));
    }
    return 0;
}

// Register value change callback
s_cb_data cb;
cb.reason = cbValueChange;
cb.cb_rtn = clk_monitor;
cb.obj = vpi_handle_by_name("top.clk", NULL);
vpi_register_cb(&cb);
```

---

### DPI (Direct Programming Interface)

**Standard:** IEEE 1800 (SystemVerilog only)
**Purpose:** Lightweight C/C++ function import/export

**Key Differences from VPI:**

| Feature | VPI | DPI |
|---------|-----|-----|
| Scope | Full simulator control | Function-level calls only |
| Complexity | Heavy (handle-based) | Light (direct calls) |
| Type safety | Manual casting | Automatic SV↔C mapping |
| Language | Verilog + SystemVerilog | SystemVerilog only |
| Use case | Introspection, callbacks | Fast algorithms, reference models |

**DPI Example:**
```systemverilog
// SystemVerilog side
import "DPI-C" function int c_crc32(input int data);

initial begin
    int crc = c_crc32(32'hDEADBEEF);
    $display("CRC = %h", crc);
end
```

```c
// C side
#include <stdint.h>

int32_t c_crc32(int32_t data) {
    // Fast C implementation
    return compute_crc(data);
}
```

**When to Use:**
- **VPI:** Need introspection, monitoring, or control flow (callbacks)
- **DPI:** Just want to call C functions (pure computation, no simulation state)

---

## The Unified Memory Advantage

### Traditional Discrete GPU Problem

```
┌─────────────┐       PCIe Bus        ┌─────────────┐
│ CPU Memory  │◄─────────────────────►│ GPU Memory  │
│  (DRAM)     │   cudaMemcpy() ~10µs  │  (VRAM)     │
└─────────────┘                       └─────────────┘
```

**VPI Challenge:** Every `vpi_get_value()` requires:
1. GPU kernel pauses
2. CPU issues copy command
3. PCIe transfer (10µs+)
4. CPU waits for completion
5. Resume GPU execution

**Result:** VPI unusably slow (1000+ signals × 10µs = 10ms overhead per access)

---

### Apple Unified Memory Solution

```
┌────────────────────────────────────────┐
│        Unified Memory (LPDDR)          │
│    ┌─────────┐       ┌─────────┐       │
│    │   CPU   │       │   GPU   │       │
│    │ Cores   │       │ Cores   │       │
│    └─────────┘       └─────────┘       │
│         ▲                 ▲            │
│         └─────────────────┘            │
│         Same physical RAM!             │
└────────────────────────────────────────┘
```

**VPI Solution:** CPU directly reads GPU buffers via `MTLBuffer.contents`

**MetalFPGA Already Uses This:**
```objc
// src/runtime/metal_runtime.mm:1130
id<MTLBuffer> buffer =
    [device newBufferWithLength:length
                        options:MTLResourceStorageModeShared];

// CPU can access directly:
uint32_t* cpu_ptr = (uint32_t*)buffer.contents;
cpu_ptr[0] = 42;  // Write from CPU

// GPU sees same memory:
kernel void my_kernel(device uint32_t* data [[buffer(0)]]) {
    uint32_t value = data[0];  // Reads 42!
}
```

**Performance:**
- CPU read (L1 cache hit): ~1ns
- CPU read (L2 cache hit): ~4ns
- CPU read (main memory): ~100ns
- GPU↔CPU coherency: ~50-100ns

**VPI speedup:** 100-1000× faster than discrete GPU!

---

## Implementation Architecture

### Phase 1: Core VPI Read/Write

#### Component: VPI Handle System

**New Files:**
- `src/runtime/vpi_runtime.hh`
- `src/runtime/vpi_runtime.cc`

**Data Structure:**
```cpp
namespace gpga {

struct VpiSignalHandle {
    std::string hierarchical_name;  // "top.cpu.alu.result"
    std::string flat_name;          // Flattened name in elaborated design

    // Buffer location
    uint32_t buffer_index;          // Which MTL4ArgumentTable buffer slot
    size_t offset;                  // Byte offset within buffer
    uint32_t width;                 // Bit width (1-64+)

    // Signal properties
    bool is_real;                   // Real vs logic type
    bool is_4state;                 // 4-state (X/Z) vs 2-state
    bool is_array;                  // Memory array
    uint32_t array_size;            // Elements (if array)

    // Direct pointers to unified memory (zero-copy access)
    void* value_ptr;                // → MTLBuffer.contents + offset
    void* xz_ptr;                   // → X/Z buffer (if 4-state)
};

class VpiRuntime {
public:
    VpiRuntime(MetalRuntime* metal_rt, const ElaboratedDesign& design);
    ~VpiRuntime();

    // Core VPI API (IEEE 1364-2005)
    vpiHandle vpi_handle_by_name(const char* name, vpiHandle scope);
    vpiHandle vpi_iterate(PLI_INT32 type, vpiHandle ref);
    vpiHandle vpi_scan(vpiHandle iterator);

    void vpi_get_value(vpiHandle obj, p_vpi_value value_p);
    vpiHandle vpi_put_value(vpiHandle obj, p_vpi_value value_p,
                           p_vpi_time time_p, PLI_INT32 flags);

    PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle obj);

    // Callback management (Phase 2)
    vpiHandle vpi_register_cb(p_cb_data cb_data_p);
    PLI_INT32 vpi_remove_cb(vpiHandle cb_obj);

private:
    MetalRuntime* runtime_;
    std::unordered_map<std::string, VpiSignalHandle> signals_;
    std::unordered_map<std::string, std::string> hier_to_flat_;
    std::vector<VpiCallback> callbacks_;

    void BuildSignalMap(const ElaboratedDesign& design);
    void* GetBufferPointer(uint32_t buffer_idx, size_t offset);
};

}  // namespace gpga
```

---

#### Implementation: Direct Memory Access

**Key Insight:** MetalFPGA already allocates shared buffers—VPI just needs pointers!

```cpp
void VpiRuntime::BuildSignalMap(const ElaboratedDesign& design) {
    for (const auto& net : design.top.nets) {
        VpiSignalHandle sig;
        sig.hierarchical_name = design.flat_to_hier[net.name];
        sig.flat_name = net.name;
        sig.width = net.width;
        sig.is_real = net.is_real;
        sig.is_4state = enable_4state_;
        sig.is_array = (net.array_size > 0);
        sig.array_size = net.array_size;

        // Get buffer info from MetalRuntime
        const MetalKernel& kernel = runtime_->GetKernel();
        sig.buffer_index = kernel.BufferIndex(net.name);

        // Get direct pointer to shared memory
        const MetalBuffer& buf = runtime_->GetBuffer(sig.buffer_index);
        sig.value_ptr = buf.contents();  // Already MTLStorageModeShared!

        if (sig.is_4state) {
            const MetalBuffer& xz_buf = runtime_->GetBuffer(sig.buffer_index + 1);
            sig.xz_ptr = xz_buf.contents();
        }

        signals_[sig.hierarchical_name] = sig;
    }
}

void VpiRuntime::vpi_get_value(vpiHandle obj, p_vpi_value value_p) {
    VpiSignalHandle* sig = reinterpret_cast<VpiSignalHandle*>(obj);

    switch (value_p->format) {
        case vpiIntVal: {
            if (sig->width <= 32) {
                // Direct read from unified memory!
                uint32_t* ptr = static_cast<uint32_t*>(sig->value_ptr);
                value_p->value.integer = *ptr;
            } else {
                // Wide value (>32 bits)
                value_p->value.integer = 0;  // Truncated
            }
            break;
        }

        case vpiBinStrVal: {
            // Format as "0101xz10" string
            std::string result;
            uint32_t* val_ptr = static_cast<uint32_t*>(sig->value_ptr);
            uint32_t* xz_ptr = static_cast<uint32_t*>(sig->xz_ptr);

            for (int i = sig->width - 1; i >= 0; --i) {
                int word = i / 32;
                int bit = i % 32;
                uint32_t val = val_ptr[word];
                uint32_t xz = xz_ptr ? xz_ptr[word] : 0;

                if (xz & (1u << bit)) {
                    result += (val & (1u << bit)) ? 'x' : 'z';
                } else {
                    result += (val & (1u << bit)) ? '1' : '0';
                }
            }

            value_p->value.str = strdup(result.c_str());
            break;
        }

        case vpiRealVal: {
            if (sig->is_real) {
                double* ptr = static_cast<double*>(sig->value_ptr);
                value_p->value.real = *ptr;  // Direct read!
            } else {
                value_p->value.real = 0.0;
            }
            break;
        }
    }
}

vpiHandle VpiRuntime::vpi_put_value(vpiHandle obj, p_vpi_value value_p,
                                    p_vpi_time time_p, PLI_INT32 flags) {
    VpiSignalHandle* sig = reinterpret_cast<VpiSignalHandle*>(obj);

    // Direct write to unified memory
    if (value_p->format == vpiIntVal && sig->width <= 32) {
        uint32_t* ptr = static_cast<uint32_t*>(sig->value_ptr);
        *ptr = value_p->value.integer;  // Write!

        // If 4-state, clear X/Z bits (force to determined value)
        if (sig->is_4state && sig->xz_ptr) {
            uint32_t* xz = static_cast<uint32_t*>(sig->xz_ptr);
            *xz = 0;
        }
    }

    return obj;  // Success
}
```

**Performance:**
- `vpi_get_value()`: ~20ns (cache hit) to ~100ns (memory fetch)
- `vpi_put_value()`: ~50ns (write-back cache)
- **No GPU synchronization needed** (unified memory handles coherency)

---

### Phase 2: Callback System

#### Challenge: When to Run Callbacks?

Traditional CPU simulators run callbacks **during** simulation:
```
Main loop:
  1. Evaluate combinational logic
  2. → cbValueChange callbacks
  3. Update sequential logic
  4. → cbAfterDelay callbacks
  5. Advance time
```

MetalFPGA runs 1000s of timesteps per GPU dispatch—how to call VPI mid-simulation?

---

#### Solution A: Timestep Batching (Simple)

Run callbacks **between GPU dispatches**:

```cpp
void MetalRuntime::RunSimulationWithVpi(VpiRuntime* vpi,
                                        uint32_t total_steps) {
    constexpr uint32_t BATCH_SIZE = 1000;  // GPU runs 1000 steps/dispatch

    for (uint32_t batch = 0; batch < total_steps / BATCH_SIZE; ++batch) {
        // GPU runs BATCH_SIZE timesteps (no CPU involvement)
        DispatchKernel(BATCH_SIZE);

        // Wait for GPU to complete this batch
        WaitForCompletion();

        // VPI callbacks read directly from unified memory
        vpi->ProcessCallbacks(cbAfterDelay);
        vpi->CheckValueChanges();  // cbValueChange

        // Callbacks can modify signal values via vpi_put_value()
        // Next GPU dispatch sees updated values (unified memory!)
    }
}

void VpiRuntime::CheckValueChanges() {
    for (auto& cb : value_change_callbacks_) {
        VpiSignalHandle* sig = cb.signal;
        uint32_t new_val = *static_cast<uint32_t*>(sig->value_ptr);

        if (new_val != cb.last_value) {
            cb.last_value = new_val;
            cb.callback_fn(cb.user_data);  // Call user code
        }
    }
}
```

**Pros:**
- Simple implementation
- Callbacks see consistent timestep snapshots
- Works with existing scheduler

**Cons:**
- GPU idle during VPI callbacks (~µs)
- Callbacks only run every BATCH_SIZE steps

**Mitigation:** With unified memory, VPI overhead is <1µs per batch—negligible!

---

#### Solution B: Async Callbacks (Advanced)

Let GPU run ahead while CPU processes callbacks:

```cpp
void MetalRuntime::RunSimulationWithAsyncVpi(VpiRuntime* vpi) {
    constexpr uint32_t BATCH_SIZE = 1000;

    for (uint32_t batch = 0; batch < total_batches; ++batch) {
        // Start GPU on batch N (non-blocking)
        DispatchKernelAsync(batch, BATCH_SIZE);

        // While GPU runs, process VPI callbacks for batch N-1
        if (batch > 0) {
            vpi->ProcessCallbacks(batch - 1);
        }

        // Only synchronize if VPI needs to modify signals
        if (vpi->HasPendingWrites()) {
            WaitForCompletion();
            vpi->ApplyWrites();
        }
    }

    // Final sync
    WaitForCompletion();
}
```

**Pros:**
- GPU never idle
- Maximum throughput

**Cons:**
- Callbacks see slightly stale data (1 batch lag)
- Complex synchronization

---

### Phase 3: GPU-Assisted Change Detection

**Optimization:** Let GPU track changes, CPU only processes changed signals.

#### GPU-Side Change Tracking

Add to Metal kernel:
```metal
struct ChangeRecord {
    uint32_t signal_id;
    uint32_t old_value;
    uint32_t new_value;
    uint64_t timestamp;
};

kernel void simulate_timestep(
    device uint32_t* signals [[buffer(0)]],
    device atomic_uint* change_count [[buffer(10)]],
    device ChangeRecord* changes [[buffer(11)]],
    constant uint32_t& max_changes [[buffer(12)]]
) {
    uint tid = gid;

    uint32_t old_val = signals[tid];
    uint32_t new_val = compute_next_value(old_val, ...);

    if (new_val != old_val) {
        signals[tid] = new_val;

        // Record change
        uint idx = atomic_fetch_add_explicit(change_count, 1,
                                             memory_order_relaxed);
        if (idx < max_changes) {
            changes[idx] = ChangeRecord{tid, old_val, new_val, current_time};
        }
    }
}
```

#### CPU-Side Processing

```cpp
void VpiRuntime::ProcessChanges() {
    // Read from unified memory (zero-copy!)
    uint32_t count = *change_count_ptr_;
    ChangeRecord* changes = static_cast<ChangeRecord*>(changes_ptr_);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t sig_id = changes[i].signal_id;

        // Look up registered callbacks for this signal
        auto it = value_change_callbacks_.find(sig_id);
        if (it != value_change_callbacks_.end()) {
            for (auto& cb : it->second) {
                cb.callback_fn(&changes[i]);  // Call VPI callback
            }
        }
    }

    // Reset count for next batch
    *change_count_ptr_ = 0;
}
```

**Advantage:** Only process signals that **actually changed**, not all monitored signals.

**Cost:** 16 bytes/change (ChangeRecord) + atomic increment (~10ns)

**Trade-off:**
- **High-activity signals:** Worth it (1000 signals, 100 change → process 100, not 1000)
- **Low-activity signals:** Overhead dominates (10 signals, 1 change → simpler to check all 10)

---

## DPI Implementation Strategy

DPI is simpler than VPI but requires different handling based on function type.

### Option A: Service Record Model (Fits Current Architecture)

Treat DPI-C imports as system tasks:

```cpp
// User writes DPI-C function
extern "C" int my_computation(int a, int b) {
    return a * a + b * b;
}

// MetalFPGA handles import:
// 1. MSL kernel queues "DPI call request"
kernel void testbench(..., device DpiRequest* dpi_queue) {
    uint request_id = atomic_fetch_add(&dpi_count, 1);
    dpi_queue[request_id] = DpiRequest{
        .function_id = DPI_MY_COMPUTATION,
        .args = {5, 7},
        .result_signal_id = 42
    };
}

// 2. CPU drains queue, calls C function
void MetalRuntime::ProcessDpiRequests() {
    DpiRequest* queue = static_cast<DpiRequest*>(dpi_buffer_->contents());
    uint32_t count = *dpi_count_ptr_;

    for (uint32_t i = 0; i < count; ++i) {
        int result = my_computation(queue[i].args[0], queue[i].args[1]);

        // Write result back to signal (unified memory!)
        uint32_t* sig_ptr = GetSignalPointer(queue[i].result_signal_id);
        *sig_ptr = result;
    }
}

// 3. Next GPU dispatch sees results
```

**Pros:**
- Fits existing service record architecture
- No MSL code generation needed

**Cons:**
- Latency (batched execution, not real-time)
- Limited to functions that can wait

---

### Option B: Inline Compilation (For Pure Functions)

For **pure functions** (no I/O, no global state), compile C → MSL:

```c
// User's DPI-C function
int square(int x) {
    return x * x;
}
```

→ MetalFPGA translates to:
```metal
int square(int x) {
    return x * x;  // Same syntax!
}
```

Then inline into kernel:
```metal
kernel void testbench(...) {
    int result = square(5);  // Native MSL call, zero overhead!
}
```

**Pros:**
- Zero latency
- Fully GPU-native

**Cons:**
- Only works for simple functions
- No stdlib, no pointers, no I/O
- Requires C→MSL compiler

---

## Performance Expectations

### VPI Operations

| Operation | Traditional GPU | Unified Memory | Speedup |
|-----------|----------------|----------------|---------|
| `vpi_get_value()` | ~10µs (PCIe copy) | **~20ns** (cache) | **500×** |
| `vpi_put_value()` | ~50µs (sync+copy) | **~50ns** (write) | **1000×** |
| `vpi_handle_by_name()` | ~100ns | ~100ns | 1× (same) |
| Value change check (1000 signals) | ~10ms | **~20µs** | **500×** |

### Simulation Overhead

**Baseline (no VPI):** 100,000 steps @ 1M steps/sec = 100ms

**With VPI monitoring (1000 signals, check every 1000 steps):**
- Traditional GPU: +10ms × 100 batches = **+1000ms** (11× slowdown!)
- Unified memory: +20µs × 100 batches = **+2ms** (2% overhead)

**Conclusion:** Unified memory makes VPI **practical** for GPU simulation.

---

## Implementation Roadmap

### Milestone 1: Basic VPI Demo

**Goal:** Prove unified memory concept with minimal VPI subset

**Implement:**
1. `vpi_handle_by_name()` — get signal handle
2. `vpi_get_value()` — read current value (int/real/string)
3. `vpi_put_value()` — force signal value
4. `vpi_iterate()` / `vpi_scan()` — traverse hierarchy
5. `vpi_register_cb(cbEndOfSimulation)` — single callback type

**Test Case:**
```c
// VPI plugin: monitor clock, force reset
void vpi_startup() {
    vpiHandle clk = vpi_handle_by_name("top.clk", NULL);
    vpiHandle rst = vpi_handle_by_name("top.reset", NULL);

    // Force reset high at cycle 100
    s_vpi_time time = {.type = vpiSimTime, .low = 100};
    s_vpi_value val = {.format = vpiIntVal, .value.integer = 1};
    vpi_put_value(rst, &val, &time, vpiNoDelay);

    // Register end-of-sim callback
    s_cb_data cb = {
        .reason = cbEndOfSimulation,
        .cb_rtn = dump_statistics
    };
    vpi_register_cb(&cb);
}
```

**Success Metric:** VPI plugin runs with <1µs overhead per access.

---

### Milestone 2: Value Change Callbacks

**Add:**
1. `vpi_register_cb(cbValueChange)` — monitor signal changes
2. Batched callback execution (Solution A)
3. Optional GPU change tracking (Phase 3 optimization)

**Test Case:**
```c
// VPI plugin: assertion checker
PLI_INT32 check_protocol(p_cb_data cb) {
    s_vpi_value req, ack;
    req.format = ack.format = vpiIntVal;

    vpi_get_value(vpi_handle_by_name("top.req", NULL), &req);
    vpi_get_value(vpi_handle_by_name("top.ack", NULL), &ack);

    if (req.value.integer && !ack.value.integer) {
        uint64_t time = vpi_get_time(vpiSimTime);
        if (time - last_req_time > 10) {
            vpi_printf("ERROR: ACK timeout at %llu\n", time);
            vpi_control(vpiFinish);
        }
    }
    return 0;
}
```

**Success Metric:** 1000-signal monitor with <2% simulation overhead.

---

### Milestone 3: VPI-Based VCD Dumper

**Goal:** Enhance current VCD writer with VPI-based implementation

**Advantages:**
- Standard VPI traversal (no MetalFPGA-specific code)
- Demonstrates VPI introspection capabilities
- Proves performance at scale

**Implementation:**
```c
// VPI VCD dumper
void dump_all_signals(uint64_t time) {
    vpiHandle top = vpi_handle_by_name("top", NULL);
    vpiHandle iter = vpi_iterate(vpiNet, top);

    while (vpiHandle sig = vpi_scan(iter)) {
        s_vpi_value value;
        value.format = vpiBinStrVal;
        vpi_get_value(sig, &value);

        vcd_write_change(sig, value.value.str, time);
        free(value.value.str);
    }
}
```

**Success Metric:** 10,000-signal design dumps VCD with <10% overhead.

---

### Milestone 4: DPI Support

**Add:**
1. Parse `import "DPI-C"` declarations from SystemVerilog (parser change)
2. Service record model for DPI calls
3. Dynamic library loading (`.dylib` plugins)

**Test Case:**
```systemverilog
// SystemVerilog testbench
import "DPI-C" function int crc32(input int data);

initial begin
    int result = crc32(32'hDEADBEEF);
    $display("CRC = %h", result);
end
```

```c
// DPI library (crc32.c)
uint32_t crc32(uint32_t data) {
    // Fast CRC implementation
    return compute_crc(data);
}
```

**Success Metric:** DPI calls work with <10µs latency per call.

---

## Integration with Existing Runtime

### Code Changes Required

#### 1. Buffer Allocation (Already Done!)
**File:** `src/runtime/metal_runtime.mm:1130`

✅ Already using `MTLResourceStorageModeShared`:
```objc
id<MTLBuffer> buffer =
    [device newBufferWithLength:length
                        options:MTLResourceStorageModeShared];
```

**No changes needed!**

---

#### 2. Expose Buffer Contents
**File:** `src/runtime/metal_runtime.hh:160`

```cpp
class MetalBuffer {
public:
    void* contents() const { return contents_; }  // ✅ Already exists

+   // Add: mutable access for VPI writes
+   void* mutable_contents() { return contents_; }
};
```

---

#### 3. Create VPI Runtime
**New files:**
- `src/runtime/vpi_runtime.hh`
- `src/runtime/vpi_runtime.cc`
- `src/runtime/vpi_user.h` (standard VPI header, IEEE 1364)

---

#### 4. Main Integration
**File:** `src/main.mm`

```cpp
if (run_flag) {
    MetalRuntime runtime;
    runtime.Initialize();

+   // Create VPI runtime
+   VpiRuntime vpi(&runtime, elab_design, enable_4state);
+
+   // Load optional VPI plugin
+   if (vpi_plugin_path) {
+       vpi.LoadPlugin(vpi_plugin_path);
+   }

    // Run simulation with VPI support
+   runtime.RunSimulationWithVpi(&vpi, count, max_steps, ...);
-   runtime.RunSimulation(count, max_steps, ...);
}
```

---

## Command-Line Interface

```bash
# Run with VPI plugin
./metalfpga_cli design.v --run \
    --vpi-plugin ./libmonitor.dylib

# Enable GPU change tracking (optimization)
./metalfpga_cli design.v --run \
    --vpi-plugin ./libmonitor.dylib \
    --vpi-track-changes

# Adjust VPI callback frequency
./metalfpga_cli design.v --run \
    --vpi-plugin ./libmonitor.dylib \
    --vpi-batch-size 500  # Call VPI every 500 GPU steps
```

---

## Example VPI Plugins

### 1. Simple Signal Monitor

```c
// monitor.c
#include "vpi_user.h"

PLI_INT32 monitor_cb(p_cb_data cb) {
    s_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(cb->obj, &val);
    vpi_printf("Signal changed: %d\n", val.value.integer);
    return 0;
}

void (*vlog_startup_routines[])() = {
    []() {
        vpiHandle sig = vpi_handle_by_name("top.data", NULL);
        s_cb_data cb = {
            .reason = cbValueChange,
            .cb_rtn = monitor_cb,
            .obj = sig
        };
        vpi_register_cb(&cb);
    },
    NULL
};
```

---

### 2. Protocol Checker

```c
// checker.c
#include "vpi_user.h"

static uint64_t req_time = 0;

PLI_INT32 check_handshake(p_cb_data cb) {
    s_vpi_value req, ack;
    req.format = ack.format = vpiIntVal;

    vpi_get_value(vpi_handle_by_name("top.req", NULL), &req);
    vpi_get_value(vpi_handle_by_name("top.ack", NULL), &ack);

    if (req.value.integer) {
        req_time = vpi_get_time(vpiSimTime);
    }

    if (req.value.integer && ack.value.integer) {
        uint64_t latency = vpi_get_time(vpiSimTime) - req_time;
        if (latency > 100) {
            vpi_printf("ERROR: Handshake timeout (%llu cycles)\n", latency);
            vpi_control(vpiFinish, 1);
        }
    }

    return 0;
}
```

---

### 3. Custom Waveform Dumper

```c
// vcd_dump.c
#include "vpi_user.h"
#include <stdio.h>

static FILE* vcd_file;

void dump_signals(p_cb_data cb) {
    uint64_t time = vpi_get_time(vpiSimTime);
    fprintf(vcd_file, "#%llu\n", time);

    vpiHandle top = vpi_handle_by_name("top", NULL);
    vpiHandle iter = vpi_iterate(vpiNet, top);

    while (vpiHandle sig = vpi_scan(iter)) {
        s_vpi_value val;
        val.format = vpiBinStrVal;
        vpi_get_value(sig, &val);

        const char* name = vpi_get_str(vpiName, sig);
        fprintf(vcd_file, "b%s %s\n", val.value.str, name);
        free(val.value.str);
    }
}
```

---

## VPI

- Signal introspection (`vpi_handle_by_name`, `vpi_iterate`, `vpi_scan`)
- Value read/write (`vpi_get_value`, `vpi_put_value`)
- Callbacks: `cbValueChange`, `cbAfterDelay`, `cbEndOfSimulation`
- Hierarchy traversal
- Property queries (`vpi_get`)
- `cbAtStartOfSimTime` (GPU already running)
- `vpi_control(vpiStop)` → pause simulation (can finish, but not pause)
- Cross-module references (flatten hierarchy first)
- Dynamic object creation (`vpi_put` on properties)

## DPI

- `import "DPI-C" function` (simple types: int, real, string)
- Service record model (batched execution)
- `export "DPI"` (SV → C calls, less common)
- Open arrays (`input int arr[]`)
- `chandle` type
- Context imports (`import "DPI-C" context`)

---

### Timing Semantics

**VPI Force Timing:**
- `vpi_put_value(..., vpiNoDelay)` → takes effect **next GPU batch**
- `vpi_put_value(..., time)` → queued for future timestep

**Not Cycle-Accurate:** VPI writes are **eventually consistent** (unified memory coherency + batch boundary).

**Acceptable For:**
- Testbench stimuli (setup, not tight loops)
- Error injection
- Coverage forcing

**Not Acceptable For:**
- Cycle-accurate co-simulation (use native Verilog or DPI instead)

---

## Why This Matters

### Ecosystem Unlock

**VPI enables:**
1. **Third-party tools** — existing VPI plugins work with MetalFPGA (Verdi, Questa integration)
2. **Custom verification** — teams can write assertion checkers, coverage collectors
3. **Co-simulation** — connect MATLAB, Python, external models
4. **Standard interfaces** — no vendor lock-in, portable testbenches

### Competitive Advantage

**No other GPU simulator has practical VPI:**
- **Verilator:** No GPU support, limited VPI (no value changes)
- **Commercial simulators (VCS, Questa):** CPU-based, no GPU acceleration
- **NVIDIA GPU simulators:** Discrete GPU = slow VPI (10µs per access)

**MetalFPGA on Apple Silicon:** Unique combination of GPU speed + low-latency VPI.

---

## Success Metrics

### Performance Targets

| Metric | Target | Why |
|--------|--------|-----|
| `vpi_get_value()` latency | <100ns | Faster than function call overhead |
| VPI overhead (1000 signals) | <2% | Acceptable for production |
| VCD dump (10K signals) | <10% | Standard waveform use case |
| VPI plugin load time | <10ms | Negligible startup cost |

### Functionality Targets

| Milestone | Feature | Impact |
|-----------|---------|--------|
| M1 | Basic VPI (read/write) | Proof of concept |
| M2 | Value change callbacks | Monitoring, assertions |
| M3 | VCD dumper | Standard waveform flow |
| M4 | DPI functions | Reference models, algorithms |

---

## References

### IEEE Standards
- **IEEE 1364-2005:** Verilog Hardware Description Language (VPI spec in Annex G)
- **IEEE 1800-2017:** SystemVerilog (DPI spec in Chapter 35)

### Apple Documentation
- **Metal 4 Programming Guide:** [docs/apple/metal4/](../apple/metal4/)
- **MTL4ArgumentTable:** [mtl4argumenttable.json](../apple/metal4/mtl4argumenttable.json)
- **Unified Memory Architecture:** Apple Silicon Technical Overview

### Existing VPI References
- **Icarus Verilog VPI:** Open-source reference implementation
- **Verilator VPI:** Subset implementation (good pragmatic example)
