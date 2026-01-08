# Scheduler VM Bytecode Buffer Access Bug

## Problem Summary

PicoRV32 with `--sched-vm` compiles successfully (Metal compilation: 2-8 seconds, MSL size: ~1MB) but fails to execute. All processes complete immediately with `done` state instead of blocking on wait edges.

**Root Cause:** GPU cannot read bytecode instructions - all reads return 0x0.

## Diagnostic Evidence

From `--run-verbose` output:
```
sched-vm-debug: magic=0x564d4447 bc_base=0 len=2 ip=0 instr=0x0 op=0 arg=0 instr0=0x0 instr1=0x0
```

**What works:**
- Magic number validated: `0x564d4447` ("VMDG") ✓
- Bytecode base offsets correct: 0, 624, 1248, 1872... ✓
- Process lengths correct: 2, 2, 4, 102, 15, 30... ✓

**What fails:**
- All instruction reads: `instr=0x0`, `op=0`, `arg=0` ✗
- IP stuck at 0 for all processes ✗
- Processes go `done` instead of executing `wait_edge` ✗

**First-op counts show 24 processes should start with `wait_edge` (opcode 15), but GPU sees opcode 0.**

## Architecture Context

### Argument Buffer Structure

The VM uses Metal argument buffers to pass 22 buffer pointers to the GPU via a single struct:

**MSL Side** ([src/codegen/msl_codegen.cc:19992-20015](src/codegen/msl_codegen.cc#L19992-L20015)):
```cpp
struct GpgaSchedVmArgs {
  device const uint* sched_vm_bytecode [[id(0)]];
  device const uint* sched_vm_proc_bytecode_offset [[id(1)]];
  device const uint* sched_vm_proc_bytecode_length [[id(2)]];
  device uint* sched_vm_cond_val [[id(3)]];
  device uint* sched_vm_cond_xz [[id(4)]];
  // ... 17 more buffers
};
```

**Kernel Parameter** ([src/codegen/msl_codegen.cc:20448](src/codegen/msl_codegen.cc#L20448)):
```cpp
constant GpgaSchedVmArgs& sched_vm_args [[buffer(N)]]
```

### Host-Side Encoding

**Buffer Creation** ([src/main.mm:6045-6078](src/main.mm#L6045-L6078)):
1. Find each buffer by name (e.g., "sched_vm_bytecode")
2. Create `MetalBufferBinding` with index 0-21
3. Call `runtime->EncodeArgumentBuffer()` to create the argument buffer

**Argument Encoder** ([src/runtime/metal_runtime.mm:2512-2555](src/runtime/metal_runtime.mm#L2512-L2555)):
```objc
id<MTLArgumentEncoder> encoder =
    [function newArgumentEncoderWithBufferIndex:buffer_index];
[encoder setArgumentBuffer:arg_buffer offset:0];
for (const auto& binding : bindings) {
  id<MTLBuffer> buffer_obj = (id<MTLBuffer>)binding.buffer->handle_;
  [encoder setBuffer:buffer_obj offset:binding.offset atIndex:binding.index];
}
```

**Bytecode Population** ([src/main.mm:2079-2080](src/main.mm#L2079-L2080)):
```cpp
if (has_layout) {
  std::memcpy(bytecode + vm_base, layout_bytecode,
              sizeof(uint32_t) * layout->bytecode.size());
}
```

## Working Tests vs Failing Test

### Small Tests Work
- `test_system_display.v`: PASSES (outputs "Simple message", format specifiers work)
- `test_system_monitor.v`: PASSES (temporal behavior correct, counter increments)

### PicoRV32 Fails
- 45 processes, 28,080 bytecode words
- All bytecode reads return 0x0
- But offset/length buffers (also in argument buffer) read correctly

## Hypothesis: Argument Buffer Resource Residency

The key observation: **offset and length buffers ARE readable, but bytecode buffer is NOT.**

This suggests a Metal resource residency issue specific to argument buffers.

### Metal Argument Buffer Requirements

When using argument buffers in Metal, buffers referenced by the argument buffer struct require special handling:

1. **Metal 1-3**: Used `MTLArgumentEncoder` with potential `useResource` requirements
2. **Metal 4**: New `MTL4ArgumentTable` API with explicit `setResource(_:bufferIndex:)` calls

### Current Code Analysis

**Buffer Storage Mode** ([src/runtime/metal_runtime.mm:2463-2464](src/runtime/metal_runtime.mm#L2463-L2464)):
```objc
id<MTLBuffer> mtl_buffer =
    [impl_->device newBufferWithLength:length
                            options:MTLResourceStorageModeShared];
```
✓ Correct - `MTLResourceStorageModeShared` allows CPU/GPU access

**Residency Set** ([src/runtime/metal_runtime.mm:2468-2473](src/runtime/metal_runtime.mm#L2468-L2473)):
```objc
if (impl_->residency_set) {
  id<MTLAllocation> allocation = (id<MTLAllocation>)mtl_buffer;
  if (![impl_->residency_set containsAllocation:allocation]) {
    [impl_->residency_set addAllocation:allocation];
    [impl_->residency_set commit];
  }
}
```
✓ Buffers are added to residency set at creation time

**Missing:** Explicit `useResource` calls during command encoding for argument buffer contents.

## Potential Root Causes

### Theory 1: Missing useResource for Argument Buffer Contents
Metal may require buffers referenced through argument buffers to be explicitly made resident via:
```objc
[computeEncoder useResource:bytecode_buffer usage:MTLResourceUsageRead];
```

Even though buffers are in the residency set, argument buffer indirection might need this.

### Theory 2: Argument Buffer Not Propagating to GPU
The argument buffer struct itself is bound, but the GPU might not be dereferencing the pointers correctly. This could be:
- A Metal API usage error
- A driver issue
- An alignment/encoding problem

### Theory 3: Size-Dependent Bug
Small tests work, PicoRV32 fails. PicoRV32 has:
- 28,080 bytecode words (112 KB)
- 45 processes
- Much larger than test cases

Possible size-related issues:
- Buffer size limit in argument buffers
- Cache coherency issues
- Memory mapping problems

### Theory 4: Timing/Synchronization Issue
The bytecode buffer is the largest buffer and might not be synchronized to GPU before kernel launch. Offset/length buffers are smaller and might sync faster.

## Next Steps to Debug

### 1. Verify Host-Side Bytecode Content
Add diagnostic to print first few bytecode words before GPU dispatch:
```cpp
auto* bc = static_cast<uint32_t*>(buffers["sched_vm_bytecode"].contents());
fprintf(stderr, "host bytecode[0]=%08x [1]=%08x [2]=%08x\n",
        bc[0], bc[1], bc[2]);
```

### 2. Try Explicit useResource Calls
Modify dispatch code to add `useResource` for all argument buffer contents:
```objc
for (const auto& binding : vm_arg_bindings) {
  [computeEncoder useResource:binding.buffer->handle_
                        usage:MTLResourceUsageRead];
}
```

### 3. Test with Direct Buffer Binding
Bypass argument buffer by passing bytecode as a separate kernel parameter:
```metal
kernel void test(
  device const uint* sched_vm_bytecode [[buffer(50)]],
  constant GpgaSchedVmArgs& args [[buffer(N)]]
)
```

### 4. Check Smaller PicoRV32 Subset
Create a reduced PicoRV32 test with fewer processes to isolate size dependency.

### 5. Metal Validation Layers
Run with Metal API validation enabled:
```
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ./build/metalfpga_cli ...
```

### 6. GPU Capture
Use Xcode GPU Frame Capture to inspect:
- Argument buffer contents
- Buffer bindings
- Resource state

## Files Involved

### Code Generation
- [src/codegen/msl_codegen.cc:19992-20015](src/codegen/msl_codegen.cc#L19992-L20015) - `GpgaSchedVmArgs` struct definition
- [src/codegen/msl_codegen.cc:20448](src/codegen/msl_codegen.cc#L20448) - Kernel parameter emission
- [src/codegen/msl_codegen.cc:30192](src/codegen/msl_codegen.cc#L30192) - Bytecode fetch in execution loop

### Runtime
- [src/main.mm:1791-2100](src/main.mm#L1791-L2100) - `InitSchedulerVmBuffers` (bytecode population)
- [src/main.mm:6001-6078](src/main.mm#L6001-L6078) - `BuildSchedulerVmArgBuffer` (argument buffer encoding)
- [src/runtime/metal_runtime.mm:2484-2559](src/runtime/metal_runtime.mm#L2484-L2559) - `EncodeArgumentBuffer` implementation
- [src/runtime/metal_runtime.mm:2460-2482](src/runtime/metal_runtime.mm#L2460-L2482) - `CreateBuffer` (storage mode, residency)

### Headers
- [include/gpga_sched.h](include/gpga_sched.h) - `GpgaSchedVmArgs` definition

## Status

**Priority:** CRITICAL - blocks PicoRV32 execution
**Workaround:** None currently
**Small tests:** Working (but they don't use argument buffers as extensively)
**Investigation:** Ongoing

## Related Documentation

- Metal Argument Buffers: [docs/apple/metal4/mtl4argumenttable.json](docs/apple/metal4/mtl4argumenttable.json)
- Resume notes: [docs/METAL4_SCHEDULER_VM_RESUME.md](docs/METAL4_SCHEDULER_VM_RESUME.md)