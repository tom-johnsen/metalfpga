# Metal4 Scheduler VM Parallelism — Progress Update (2026-01-10)

This update follows summarizes progress, current issues, and the plan forward. It is intended as a handoff note back to Metal4GPT for review.

## Summary (TL;DR)

- Core phased pipeline is implemented (wait eval → ready compaction → indirect
  args → exec-ready), plus batching and service ring drain.
- Exec-ready uses GPU-driven indirect dispatch with a pipeline-fixed TG size
  (`requiredThreadsPerThreadgroup`, default 64 via `METALFPGA_SCHED_EXEC_READY_TG`;
  `>64` fails due to kernel max threads).
- Performance is still hertz-level on picorv32 because iterations do tiny work
  and are mostly blocked; GPU is busy but each dispatch is small.
- Next meaningful wins require algorithmic changes: time-jump and/or running
  multiple iterations per dispatch.

## Progress Since Last Metal4GPT Review

### Phase 1 (Queue vs GPU execution timing)

- Dispatch timing detail uses commit feedback (`GPUStartTime/GPUEndTime`) to log
  `queue_ms` and `gpu_exec_ms` when `METALFPGA_DISPATCH_TIMING=1` and
  `METALFPGA_DISPATCH_TIMING_DETAIL=1` are enabled.

### Phase 2 / 2.5 / 3 (Proc-parallel + Indirect Dispatch)

- Added `sched_ready` buffer (ready flags + ready list + ready count).
- `*_sched_ready_reset/flags/compact` kernels produce a **global ready list**
  via atomic append; `ready_count[0]` is total ready work.
- `*_sched_ready_dispatch` is the explicit **Phase 2.5** micro-phase:
  - grid=1, reads `ready_count[0]`
  - writes indirect args (`threadsPerGrid` + `threadsPerThreadgroup`, 6 u32)
    into the `sched_ready` tail
  - `threadsPerThreadgroup` is prefilled on host from the exec-ready TG size
    and validated at dispatch; the pipeline uses
    `requiredThreadsPerThreadgroup` (default 64, env override)
- `*_sched_exec_ready` consumes the global ready list and executes procs in
  parallel via indirect dispatch.
- The host uses a **single indirect dispatch** for exec-ready (supports
  `count > 1`), with `ready_count == 0` clamped to 1 thread and early-out.

### Phase 4 (Batching)

- `DispatchBatch` supports multiple dispatches in a single encoder and can
  include indirect dispatch entries.
- Barriers are explicit and tunable:
  - `METALFPGA_BATCH_BARRIERS_DISABLE=1`
  - `METALFPGA_BATCH_BARRIER_ALIAS=1`
  - `METALFPGA_BATCH_BARRIER_ALIAS_AUTO=1`
  - default is an intra-pass barrier between dispatches with `.device`
    visibility; `.resourceAlias` is added when requested (or auto-detected).

### Phase 5 (Service Ring Drain)

- Service records use a GPU ring buffer with head/tail in
  `sched_service_count`.
- CPU drain updates head; cadence controlled by
  `METALFPGA_SCHED_SERVICE_DRAIN_EVERY`.

### Runtime / Safety Alignment

- Implemented optional queue-level residency (`METALFPGA_RESIDENCY_SET_QUEUE`)
  to match Metal4GPT guidance for GPU-address bindings.
- Kept allocator rules and resource lifetime requirements in mind; still
  using a single encoder per batch.

### Scripts / Tooling

- Added `scripts/run_parallelism_verify.sh` and
  `scripts/run_parallelism_tune.sh`.
- Added `scripts/run_metal_trace.sh` to automate Instruments captures.

## Implementation Snippets (verbatim)

- `src/main.mm` (indirect args layout + exec-ready TG prefill)

```objc
  if (use_sched_ready_dispatch) {
    auto ready_it = buffers.find("sched_ready");
    if (ready_it == buffers.end() || !ready_it->second.contents()) {
      if (run_verbose) {
        std::cerr << "sched ready dispatch disabled: sched_ready missing\n";
      }
      use_sched_ready_dispatch = false;
    } else {
      const size_t stride =
          static_cast<size_t>(count) * static_cast<size_t>(sched.proc_count);
      const size_t dispatch_base_u32 = (stride * 2u) + count;
      const size_t required_bytes =
          (dispatch_base_u32 + 6u) *
          sizeof(uint32_t);
      if (ready_it->second.length() < required_bytes) {
        if (run_verbose) {
          std::cerr << "sched ready dispatch disabled: sched_ready too small ("
                    << ready_it->second.length() << " < "
                    << required_bytes << " bytes)\n";
        }
        use_sched_ready_dispatch = false;
      }
    }
    if (use_sched_ready_dispatch) {
      const uint32_t exec_tg =
          runtime.ComputeThreadgroupSize(sched_exec_ready_kernel);
      const size_t stride =
          static_cast<size_t>(count) * static_cast<size_t>(sched.proc_count);
      const size_t dispatch_base_u32 = (stride * 2u) + count;
      sched_ready_dispatch_offset =
          dispatch_base_u32 * sizeof(uint32_t);
      auto* ready_u32 =
          static_cast<uint32_t*>(ready_it->second.contents());
      auto* dispatch_u32 = ready_u32 + dispatch_base_u32;
      dispatch_u32[3u] = exec_tg;
      dispatch_u32[4u] = 1u;
      dispatch_u32[5u] = 1u;
      use_exec_ready_indirect = true;
    }
  }
```

- `src/main.mm` (ready phase dispatch list + indirect exec-ready path)
```objc
        if (!sched_batch) {
          if (ready_batch) {
            std::vector<gpga::MetalDispatch> ready_dispatches;
            ready_dispatches.push_back(
                gpga::MetalDispatch{&sched_ready_reset_kernel,
                                    &ready_reset_bindings, count});
            if (use_sched_wait_eval) {
              ready_dispatches.push_back(
                  gpga::MetalDispatch{&sched_wait_eval_kernel,
                                      &wait_eval_bindings,
                                      static_cast<uint32_t>(ready_grid)});
            }
            ready_dispatches.push_back(
                gpga::MetalDispatch{&sched_ready_flags_kernel,
                                    &ready_flags_bindings,
                                    static_cast<uint32_t>(ready_grid)});
            ready_dispatches.push_back(
                gpga::MetalDispatch{&sched_ready_compact_kernel,
                                    &ready_compact_bindings,
                                    static_cast<uint32_t>(ready_grid)});
            if (use_exec_ready_indirect) {
              ready_dispatches.push_back(
                  gpga::MetalDispatch{&sched_ready_dispatch_kernel,
                                      &ready_dispatch_bindings, 1u});
            }
            if (use_sched_exec_ready) {
              if (use_exec_ready_indirect) {
                auto ready_it = buffers.find("sched_ready");
                if (ready_it == buffers.end()) {
                  if (error) {
                    *error =
                        "sched_ready buffer missing for indirect dispatch";
                  }
                  return false;
                }
                gpga::MetalDispatch exec_dispatch{
                    &sched_exec_ready_kernel, &exec_ready_bindings, 1u};
                exec_dispatch.indirect_buffer = &ready_it->second;
                exec_dispatch.indirect_offset = sched_ready_dispatch_offset;
                ready_dispatches.push_back(exec_dispatch);
              } else {
                ready_dispatches.push_back(
                    gpga::MetalDispatch{&sched_exec_ready_kernel,
                                        &exec_ready_bindings,
                                        static_cast<uint32_t>(ready_grid)});
              }
              exec_ready_batched = true;
              exec_ready_after_drain = true;
            }
            if (!runtime.DispatchBatch(ready_dispatches, count, error,
                                       dispatch_timeout_ms)) {
              return false;
            }
          } else {
            if (!runtime.Dispatch(sched_ready_reset_kernel,
                                  ready_reset_bindings, count, error,
                                  dispatch_timeout_ms)) {
              return false;
            }
            if (use_sched_wait_eval) {
              if (!runtime.Dispatch(sched_wait_eval_kernel, wait_eval_bindings,
                                    static_cast<uint32_t>(ready_grid), error,
                                    dispatch_timeout_ms)) {
                return false;
              }
            }
            if (!runtime.Dispatch(sched_ready_flags_kernel,
                                  ready_flags_bindings,
                                  static_cast<uint32_t>(ready_grid), error,
                                  dispatch_timeout_ms)) {
              return false;
            }
            if (!runtime.Dispatch(sched_ready_compact_kernel,
                                  ready_compact_bindings,
                                  static_cast<uint32_t>(ready_grid), error,
                                  dispatch_timeout_ms)) {
              return false;
            }
            if (use_exec_ready_indirect) {
              if (!runtime.Dispatch(sched_ready_dispatch_kernel,
                                    ready_dispatch_bindings, 1u, error,
                                    dispatch_timeout_ms)) {
                return false;
              }
            }
          }
        }
        if (use_sched_exec_ready && !exec_ready_batched) {
          if (use_exec_ready_indirect) {
            auto ready_it = buffers.find("sched_ready");
            if (ready_it == buffers.end()) {
              if (error) {
                *error = "sched_ready buffer missing for indirect dispatch";
              }
              return false;
            }
            if (!runtime.DispatchIndirectThreads(
                    sched_exec_ready_kernel, exec_ready_bindings,
                    ready_it->second, sched_ready_dispatch_offset, error,
                    dispatch_timeout_ms)) {
              return false;
            }
            exec_ready_after_drain = true;
          } else {
            if (!runtime.Dispatch(sched_exec_ready_kernel,
                                  exec_ready_bindings,
                                  static_cast<uint32_t>(ready_grid), error,
                                  dispatch_timeout_ms)) {
              return false;
            }
            exec_ready_after_drain = true;
          }
        }
```

- `src/runtime/metal_runtime.mm` (DispatchBatch indirect dispatch + barriers)
```objc
    if (use_indirect) {
      if (!indirect_buffer->handle_) {
        if (error) {
          *error = "Metal indirect dispatch buffer unavailable";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      if ((indirect_offset % 4u) != 0u) {
        if (error) {
          *error = "Metal indirect dispatch offset not 4-byte aligned";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      if (indirect_offset + sizeof(MTLDispatchThreadsIndirectArguments) >
          indirect_buffer->length()) {
        if (error) {
          *error = "Metal indirect dispatch buffer too small for arguments";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      uint32_t tg_x = 0u;
      uint32_t tg_y = 0u;
      uint32_t tg_z = 0u;
      if (indirect_buffer->contents()) {
        const uint8_t* base =
            static_cast<const uint8_t*>(indirect_buffer->contents()) +
            indirect_offset;
        tg_x = ReadU32(base, sizeof(uint32_t) * 3u);
        tg_y = ReadU32(base, sizeof(uint32_t) * 4u);
        tg_z = ReadU32(base, sizeof(uint32_t) * 5u);
      }
      uint32_t required_tg = ComputeThreadgroupSize(*kernel);
      if (tg_x == 0u || tg_y == 0u || tg_z == 0u) {
        if (error) {
          *error = "Indirect dispatch buffer missing threadsPerThreadgroup";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      if (required_tg > 0u &&
          (tg_x != required_tg || tg_y != 1u || tg_z != 1u)) {
        if (error) {
          *error = "Indirect dispatch threadsPerThreadgroup mismatch";
        }
        [encoder endEncoding];
        [cmd endCommandBuffer];
        [cmd release];
        return false;
      }
      id<MTLBuffer> buffer = (id<MTLBuffer>)indirect_buffer->handle_;
      MTLGPUAddress address =
          buffer.gpuAddress +
          static_cast<MTLGPUAddress>(indirect_offset);
      [encoder dispatchThreadsWithIndirectBuffer:address];
    } else {
      MTLSize threads_per_group = MTLSizeMake(threadgroup, 1, 1);
      MTLSize grid = MTLSizeMake(dispatch_grid, 1, 1);
      [encoder dispatchThreads:grid threadsPerThreadgroup:threads_per_group];
    }
    if (impl_->batch_barriers && dispatch_index + 1 < dispatches.size()) {
      [encoder barrierAfterEncoderStages:MTLStageDispatch
                     beforeEncoderStages:MTLStageDispatch
                       visibilityOptions:barrier_visibility];
    }
```

- `src/runtime/metal_runtime.mm` (exec-ready TG selection + pipeline enforcement)
```objc
uint32_t RequiredThreadsPerThreadgroupForKernel(
    const std::string& name, uint32_t global_override) {
  if (!IsExecReadyKernelName(name)) {
    return 0u;
  }
  if (auto exec_ready_override = EnvU32("METALFPGA_SCHED_EXEC_READY_TG")) {
    if (*exec_ready_override > 0u) {
      return *exec_ready_override;
    }
  }
  if (global_override > 0u) {
    return global_override;
  }
  return 64u;
}
```

```objc
    if (required_tg > 0u) {
      desc.requiredThreadsPerThreadgroup = MTLSizeMake(required_tg, 1, 1);
    }
    ...
  temp.max_threads_per_threadgroup_ =
      static_cast<uint32_t>(pipeline.maxTotalThreadsPerThreadgroup);
  temp.required_threads_per_threadgroup_ = required_tg;
  if (required_tg > 0u && temp.max_threads_per_threadgroup_ > 0u &&
      required_tg > temp.max_threads_per_threadgroup_) {
    if (error) {
      *error = "required threadgroup size (" + std::to_string(required_tg) +
               ") exceeds max (" +
               std::to_string(temp.max_threads_per_threadgroup_) +
               ") for kernel " + name;
    }
    [pipeline release];
    temp.pipeline_ = nullptr;
    return false;
  }
```

## Generated MSL Excerpts (verbatim)

These kernel bodies are emitted by `src/codegen/msl_codegen.cc` for each
module (the `<module>` token is `MslName(module.name)`). The parameter list is
the auto-generated scheduler param list ending with
`uint gid [[thread_position_in_grid]]`.

- `src/codegen/msl_codegen.cc` (ready list compaction)
```metal
kernel void gpga_<module>_sched_ready_compact(/* sched params */, uint gid [[thread_position_in_grid]]) {
  device uint* __gpga_ready_u32 =
      reinterpret_cast<device uint*>(sched_ready);
  uint __gpga_ready_stride = sched.count * GPGA_SCHED_PROC_COUNT;
  device uint* __gpga_ready_list =
      __gpga_ready_u32 + __gpga_ready_stride;
  device atomic_uint* __gpga_ready_count =
      reinterpret_cast<device atomic_uint*>(
          __gpga_ready_u32 + (__gpga_ready_stride * 2u));
  uint __gpga_sim = gid / GPGA_SCHED_PROC_COUNT;
  if (__gpga_sim >= sched.count) {
    return;
  }
  uint __gpga_pid = gid - (__gpga_sim * GPGA_SCHED_PROC_COUNT);
  uint __gpga_base = __gpga_sim * GPGA_SCHED_PROC_COUNT;
  uint __gpga_idx = __gpga_base + __gpga_pid;
  if (__gpga_ready_u32[__gpga_idx] == 0u) {
    return;
  }
  uint __gpga_out = atomic_fetch_add_explicit(
      &__gpga_ready_count[0], 1u, memory_order_relaxed);
  if (__gpga_out < __gpga_ready_stride) {
    __gpga_ready_list[__gpga_out] = __gpga_idx;
  }
}
```

- `src/codegen/msl_codegen.cc` (indirect dispatch args fill)
```metal
kernel void gpga_<module>_sched_ready_dispatch(/* sched params */, uint gid [[thread_position_in_grid]]) {
  device uint* __gpga_ready_u32 =
      reinterpret_cast<device uint*>(sched_ready);
  uint __gpga_ready_stride = sched.count * GPGA_SCHED_PROC_COUNT;
  device atomic_uint* __gpga_ready_count =
      reinterpret_cast<device atomic_uint*>(
          __gpga_ready_u32 + (__gpga_ready_stride * 2u));
  device uint* __gpga_dispatch_u32 =
      __gpga_ready_u32 + (__gpga_ready_stride * 2u) + sched.count;
  if (gid != 0u) {
    return;
  }
  uint __gpga_ready = atomic_load_explicit(
      &__gpga_ready_count[0], memory_order_relaxed);
  if (__gpga_ready > __gpga_ready_stride) {
    __gpga_ready = __gpga_ready_stride;
  }
  uint __gpga_threads = (__gpga_ready > 0u) ? __gpga_ready : 1u;
  __gpga_dispatch_u32[0u] = __gpga_threads;
  __gpga_dispatch_u32[1u] = 1u;
  __gpga_dispatch_u32[2u] = 1u;
}
```

- `src/codegen/msl_codegen.cc` (exec-ready kernel)
```metal
kernel void gpga_<module>_sched_exec_ready(/* sched params */, uint gid [[thread_position_in_grid]]) {
  device uint* __gpga_ready_u32 =
      reinterpret_cast<device uint*>(sched_ready);
  uint __gpga_ready_stride = sched.count * GPGA_SCHED_PROC_COUNT;
  device uint* __gpga_ready_list =
      __gpga_ready_u32 + __gpga_ready_stride;
  device uint* __gpga_ready_count =
      __gpga_ready_u32 + (__gpga_ready_stride * 2u);
  uint __gpga_ready_total = __gpga_ready_count[0];
  if (__gpga_ready_total > __gpga_ready_stride) {
    __gpga_ready_total = __gpga_ready_stride;
  }
  if (gid >= __gpga_ready_total) {
    return;
  }
  uint __gpga_idx = __gpga_ready_list[gid];
  if (__gpga_idx >= __gpga_ready_stride) {
    return;
  }
  uint __gpga_sim = __gpga_idx / GPGA_SCHED_PROC_COUNT;
  if (__gpga_sim >= sched.count) {
    return;
  }
  uint pid = __gpga_idx - (__gpga_sim * GPGA_SCHED_PROC_COUNT);
  uint idx = __gpga_idx;
  if (sched_state[idx] != GPGA_SCHED_PROC_READY) {
    return;
  }
  if (sched_wait_kind[idx] != GPGA_SCHED_WAIT_NONE) {
    return;
  }
  uint steps = sched.max_proc_steps;
  if (steps == 0u) {
    steps = sched.max_steps;
  }
  bool did_work = false;
  bool finished = false;
  bool stopped = false;
  ulong __gpga_time = sched_time[__gpga_sim];
  while (steps > 0u && sched_state[idx] == GPGA_SCHED_PROC_READY) {
    gpga_<module>_sched_exec_step(/* sched params */, pid, idx, &steps,
                                 &did_work, &finished, &stopped, &__gpga_time);
  }
  if (sched_state[idx] == GPGA_SCHED_PROC_DONE) {
    uint parent = sched_parent[idx];
    if (parent != GPGA_SCHED_NO_PARENT) {
      uint pidx = gpga_sched_index(__gpga_sim, parent);
      if (sched_wait_kind[pidx] == GPGA_SCHED_WAIT_JOIN &&
          sched_wait_id[pidx] == sched_join_tag[idx]) {
        if (sched_join_count[pidx] > 0u) {
          sched_join_count[pidx] -= 1u;
        }
        if (sched_join_count[pidx] == 0u) {
          sched_wait_kind[pidx] = GPGA_SCHED_WAIT_NONE;
          sched_state[pidx] = GPGA_SCHED_PROC_READY;
        }
      }
    }
  }
  if (finished) {
    sched_halt_mode[__gpga_sim] = GPGA_SCHED_HALT_FINISH;
    sched_status[__gpga_sim] = GPGA_SCHED_STATUS_FINISHED;
  } else if (stopped) {
    sched_halt_mode[__gpga_sim] = GPGA_SCHED_HALT_STOP;
    sched_status[__gpga_sim] = GPGA_SCHED_STATUS_STOPPED;
  }
}
```

## Current Issues / Bottlenecks

1) **Hertz-level throughput on picorv32**
   - Most procs are blocked (time/edge waits), so `ready_count` is often 0.
   - GPU does many tiny dispatches, so speed is limited by iteration count,
     not raw GPU throughput.

2) **Threadgroup limit for exec-ready**
   - Exec-ready uses pipeline-fixed TG sizing for indirect dispatch.
   - `METALFPGA_SCHED_EXEC_READY_TG=128` fails:
     `required threadgroup size (128) exceeds max (64)`
   - So `exec_ready_tg <= 64` for this kernel pipeline.

3) **Correctness verification still incomplete**
   - Exec-ready logs diverge from legacy output (expected, due to new
     `sched ready` lines + different iteration counts).
   - Need a correctness-focused compare that ignores verbose diagnostics or
     uses a smaller testbench.

## Tuning Snapshot (picorv32, count=1, 20s runs)

- `exec_ready_tg=64`, `ready_tg=256`, alias off: ~202 it/s (best so far).
- `ready_tg=256` alias on/auto: ~191 / ~186 it/s.
- `ready_tg=384` is slower (~184 / ~174 / ~161 it/s).
- `exec_ready_tg=128` fails fast (max threads exceeded).

## Instruments Findings (Count=2, 30s, exec-ready + batch)

Capture: `tmp/metal_system_trace.20260110_174124.trace` (TRACE_MODE=all).
App-level tables did not list metalfpga command buffer submissions, so we used
`metal-gpu-intervals` entries labeled `metalfpga_cli` (Compute channel).

- Compute intervals: 9,236 over 30s (~308 commands/sec).
- GPU duration per command: avg 2.63 ms, median 2.01 ms, min 0.086 ms,
  max 6.48 ms.
- CPU→GPU latency: avg 0.187 ms, median 0.179 ms, max 6.70 ms.
- Start gap: avg 3.32 ms, median 2.56 ms, max 20.97 ms.
- Idle gap (end→next start): avg 0.69 ms, median 0.49 ms, max 16.26 ms.

Interpretation:
- GPU is busy most of the time (~79% duty), but each dispatch is small.
- Queue latency is low; we are bound by the **number of iterations** and
  small work per dispatch, not by queueing.

## Plan Forward (Proposed)

### 1) Time-Jump / Next-Event Advance (Highest Impact)

Goal: skip empty time between scheduled time waits.

Sketch:
- Only consider time-jump when `ready_count == 0` and no delta waits are pending.
- Reduce `min_wait_time` across all procs with `wait_kind == TIME`.
- If `min_wait_time > sched_time`, advance `sched_time` to it.
- Immediately re-run wait-eval to unblock time waits at the new time.

Notes:
- Do not jump past edge/cond waits. If only edge/cond waits remain and no time
  waits exist, we cannot advance time.

### 2) Multi-Iteration per Dispatch

Goal: reduce CPU/GPU sync overhead by running N scheduler ticks per dispatch.

Sketch:
- Add a bounded loop inside the GPU pipeline that runs N iterations or stops
  on `finished/stopped/error`.
- If service records are produced, exit early so CPU can drain.

### 3) Multi-Sim Throughput

Goal: increase sims/sec via `count > 1`, even if single-sim remains slow.

Sketch:
- Validate `count=2/4/8` with exec-ready + batch.
- Measure sims/sec and GPU duty; tune TGs only after correctness holds.

## Questions for Metal4GPT

1) **Time-jump correctness**
   - Is the rule “only jump when no ready procs and no delta waits” sufficient,
     or should we also guard on edge/cond wait presence?
   - Any recommended GPU reduction pattern for min wait time per sim?

2) **Multi-iteration per dispatch**
   - Best practice for coordinating CPU service drain when a GPU loop runs
     multiple iterations? Poll a ring-buffer watermark in-device, or force a
     periodic exit every N iterations?

3) **Indirect dispatch + fixed TG size**
   - We embed `threadsPerThreadgroup` next to the indirect args for validation,
     but the actual dispatch uses `requiredThreadsPerThreadgroup`. Is this a
     reasonable pattern, or should we stick strictly to
     `MTLDispatchThreadsIndirectArguments` (3 u32) and rely only on pipeline
     state?
   - For tuning 32 vs 64 vs 128, is the recommended approach to build
     multiple pipeline states with different `requiredThreadsPerThreadgroup`
     and switch at runtime?

4) **Profiling visibility**
   - `metal-application-command-buffer-submissions` did not show the
     metalfpga process in Instruments. Any known steps to make Metal4 CLI
     compute work appear there, or is `metal-gpu-intervals` the right tool?

## Requested Review

Please review the time-jump plan, the multi-iteration per dispatch approach,
and any pitfalls for correctness or Metal 4 visibility, given the current
implementation state above.
