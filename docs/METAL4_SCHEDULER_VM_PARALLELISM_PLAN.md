# Metal 4 Scheduler VM Parallelism Plan

This plan converts the current single-dispatch, single-sim scheduler step into
a phased, data-parallel pipeline that saturates the GPU and reduces CPU waits.
It is based on the Metal4GPT recommendations and current diagnostics.

## Goals

- Increase intra-sim parallelism (proc-parallel phases).
- Reduce CPU-GPU sync points (batch phases in one command buffer).
- Keep correctness identical to current scheduler behavior.
- Produce clear timing data that separates queue delay from GPU execution time.

## Non-Goals (For Now)

- Full rewrite of the scheduler VM bytecode format.
- Indirect command buffers (ICBs) unless needed later.
- Multi-sim scale-out beyond correctness/perf validation; this plan targets
  `count=1` first, then validates `count > 1`.

## Baseline (Current Observations)

- Dispatch shows `grid=1`, `tg=32`, `tew=32`, `binds=31`.
- `gpu_ms` is close to `wait_ms` (GPU dominates).
- Long gaps between dispatches seen in real-time.

## Metal 4 Execution Model Notes (Must-Haves)

- Command allocators are not thread-safe; use one per encoding thread.
- Allocators may be reset only after the GPU finishes work that used them.
- When binding by GPU address (argument tables), resource lifetime is on us:
  buffers must stay alive until the GPU completes the command buffer.
- Use residency sets for GPU-address bindings (`useResidencySet` or queue-level
  `addResidencySets`) so indirectly referenced buffers stay resident.
- Treat producer -> consumer dependencies explicitly with barriers when one
  dispatch writes data that a later dispatch reads.
- For indirect dispatch, threadgroup size must be fixed/known:
  use `requiredThreadsPerThreadgroup` (and TEW-multiple) or switch to a
  `dispatchThreadgroupsWithIndirectBuffer:threadsPerThreadgroup:` path if
  per-dispatch sizing is required.

## Current Status Summary

- Phase 0/1 complete (indexing confirmed; queue vs exec timing separated).
- Phase 2/2.5/3 complete for exec-ready mode with GPU-driven dispatch.
- Phase 4 batching complete (barriers/visibility toggles wired; single encoder).
- Phase 5 service drain complete (ring-buffer head/tail + drain cadence).
- Phase 6 tuning pending (threadgroup sizing, barrier alias, load balancing).
- Parallelism plan implementation complete; remaining work is tuning +
  verification runs.

## Plan Overview (Phased)

### Phase 0: Sanity Check the Kernel Indexing

Confirm we are not accidentally running only one lane:
- Verify which index is used in `gpga_testbench_sched_step`:
  - `thread_position_in_grid` vs `threadgroup_position_in_grid`.
  - Early-out behavior when `grid=1` or `simCount=1`.
- If we intend one sim per threadgroup, use `dispatchThreadgroups` and index by
  `threadgroup_position_in_grid.x`, with per-thread work via
  `thread_position_in_threadgroup.x`.
- Optional debug: write out `thread_position_in_grid.x`,
  `threadgroup_position_in_grid.x`, and `thread_position_in_threadgroup.x` to a
  small debug buffer for a single dispatch.

Success criteria:
- We can explain exactly how many lanes are active for `count=1`.
- We observe >1 active lane when `grid_size` is artificially raised.

Status:
- Complete. Verified `gid` increments and `tg` increments at `gid=32` with
  `--count 64`, confirming threadgroup indexing is correct.

### Phase 1: Separate Queue Delay from GPU Execution

Add commit feedback timing in the Metal 4 runtime:
- Use `MTL4CommitOptions.addFeedbackHandler` to log
  `gpuStartTime` and `gpuEndTime`.
- Compute queue delay = `gpuStartTime - cpuCommitTime`.
- Keep current counter-heap timestamps for intra-dispatch timing.
- Log `commit_count` since one commit can enqueue multiple command buffers.
- Treat the feedback handler as concurrent (thread-safe logging only).

Success criteria:
- Logs show queue delay vs GPU execution time per commit.

Status:
- Complete. `queue_ms` is small for `--count 1`, while `gpu_exec_ms` ~ `wait_ms`,
  confirming kernel time dominates; counter-heap `gpu_ms` sometimes diverges,
  so we rely on feedback `gpu_exec_ms` for queue-vs-exec separation.
- Added a stop summary log on transitions to `stopped/finished/error` that
  prints `sched_halt_mode` and `sched_error`.
- Latest run shows `queue_ms` sub-ms with `gpu_exec_ms` ~ 420ms, and stop summary
  reporting `status=4(stopped)` with `halt=none` (likely Ctrl-C).

### Phase 2: Introduce Proc-Parallel Phases

Split the scheduler step into data-parallel phases:
1) Wait evaluation (grid = proc_count)
2) Ready compaction + ready_count (grid = proc_count)
3) VM execute for ready procs (grid = ready_count)
4) Commit / bookkeeping

Data changes:
- Add buffers for ready flags, ready list, and ready_count.
- Store compact intermediate state (flags/indices) between phases.
- Start with an atomic append compaction (one atomic per ready proc), then
  optimize later if contention is high.
- Do not assume `ready_count` is usable inside the same dispatch that produces
  it (no global barrier inside a single dispatch).

Success criteria:
- Phase 1 and 2 dispatch with `grid >= proc_count`.
- Ready_count is correct and non-zero for active designs.
- During bring-up, validate `ready_count` with a CPU readback on known designs.

Status:
- Complete. Added `sched_ready` buffer (ready flags + list + count), plus
  `*_sched_ready_reset`, `*_sched_ready_flags`, and `*_sched_ready_compact`
  kernels for diagnostics. `sched_ready_compact` now uses atomic append into a
  global ready list (stores `gpga_sched_index`), increments `ready_count[0]`
  as the total ready count, and runs with `grid = count * proc_count`.
- Extracted per-proc exec into `*_sched_exec_step` in both codegen passes; the
  inner `while (steps > 0u && ready)` now calls the helper. This preserves
  current semantics and is ready to reuse in a proc-parallel exec kernel.
- Emitted `*_sched_exec_ready` (proc-parallel exec) kernels that consume the
  compacted ready list and call `*_sched_exec_step`, and wired them into the
  host dispatch sequence when exec-ready is enabled (diag or non-diag).
- In `sched_step`, exec-ready mode now scans for ready procs, marks
  `did_work = true`, and exits the loop early to avoid advancing time before
  the proc-parallel exec runs.
- Added `*_sched_ready_dispatch` (Phase 2.5) and host-side prefill of
  threads-per-threadgroup for indirect exec (see Phase 2.5/3).
- `sched_ready_dispatch` no longer uses a separate buffer binding; dispatch
  args now live in the tail of `sched_ready` to stay within Metal buffer
  binding limits.
- `METALFPGA_SCHED_READY_EVERY` is forced to 1 when exec-ready is enabled
  (warns in `--run-verbose`) to prevent stale ready lists.
- Ready list now drives indirect exec for `count > 1` without per-sim dispatch.

### Phase 2.5: PrepareIndirectArgs (New)

Write indirect dispatch args after `ready_count` is finalized:
- Dispatch a tiny kernel (grid=1 or small fixed grid).
- Read `ready_count` and write the indirect-args struct (threads-per-grid).
- Keep threadgroup size fixed at pipeline build time; only store
  threads-per-threadgroup in the buffer if using an indirect threadgroups API.
- Insert a barrier if needed for visibility:
  `visibilityOptions = MTL4VisibilityOptions.device`.
- Use `MTLDispatchThreadsIndirectArguments` layout and 4-byte aligned
  `MTLGPUAddress`.

Success criteria:
- Indirect args are written by a dedicated dispatch after `ready_count`.

Status:
- Complete. `*_sched_ready_dispatch` writes `threadsPerGrid` from
  `ready_count[0]` (sum of ready procs); host pre-fills
  `threadsPerThreadgroup` once using the exec-ready kernel’s threadgroup size.
- `threadsPerGrid` is clamped to at least 1 to avoid zero-thread indirect
  dispatch when `ready_count == 0` (exec-ready kernel early-outs by slot).
- Dispatch args are stored in `sched_ready` at offset
  `(count * proc_count * 2 + count) * sizeof(uint32_t)` (one
  `MTLDispatchThreadsIndirectArguments` slot) and passed to
  `DispatchIndirectThreads` with that offset.
- Indirect exec uses `requiredThreadsPerThreadgroup` (default 64; override via
  `METALFPGA_SCHED_EXEC_READY_TG`) to keep TG sizing consistent.

### Phase 3: GPU-Driven Dispatch (Indirect Args)

Make Phase 3 launch size GPU-driven:
- Phase 2.5 writes indirect dispatch args (ready_count).
- Phase 3 uses `dispatchThreadsWithIndirectBuffer:` (indirect buffer address).
- Insert a barrier between producer -> consumer phases, especially when binding
  by GPU address (consider `.device`, and `.resourceAlias` if aliasing).
- Ensure threadgroup size is fixed at pipeline build time
  (`requiredThreadsPerThreadgroup`); tune 64/128 as separate pipeline variants.
- Barrier edges to start with:
  - Phase 2 -> Phase 2.5
  - Phase 2.5 -> Phase 3
  Use `barrierAfterEncoderStages:MTLStageDispatch
       beforeEncoderStages:MTLStageDispatch`.

Success criteria:
- VM exec launches with `grid == ready_count` without CPU readback.
- Indirect dispatch remains correct with multiple threadgroups in Phase 2.
- Handle `ready_count == 0` by dispatching at least 1 thread and early-out if
  `tid >= ready_count`.

Status:
- Complete for exec-ready mode. Host uses `DispatchIndirectThreads` and the
  indirect args buffer to drive `*_sched_exec_ready` with a single global
  dispatch (supports `count > 1`).
- `DispatchBatch` now supports indirect dispatch entries (using
  `dispatchThreadsWithIndirectBuffer:` with 4-byte alignment checks), so
  exec-ready can be batched with ready phases when desired.
- Threadgroup sizing is locked via `requiredThreadsPerThreadgroup`; tuning
  remains in Phase 6.

### Phase 4: Batch Phases in One Command Buffer

Reduce CPU waits by encoding all phases into one compute encoder:
- Single `MTL4ComputeCommandEncoder`.
- Dispatch phases back-to-back in the same command buffer.
- Commit once per scheduler batch, not once per phase.
- Use explicit barriers between phases while validating correctness.

Success criteria:
- Fewer CPU waits per scheduler tick.
- `wait_gap_ms` decreases in dispatch timing logs.

Status:
- Complete. `DispatchBatch` supports per-dispatch grids/indirect entries with a
  single compute encoder and optional `sched_exec_ready` as the final dispatch.
- Intra-encoder barriers stay enabled by default (`.device` visibility), with
  `.resourceAlias` controls available for tuning:
  - `METALFPGA_BATCH_BARRIER_ALIAS=1`
  - `METALFPGA_BATCH_BARRIER_ALIAS_AUTO=1`
  - `METALFPGA_BATCH_BARRIERS_DISABLE=1`

## Post-Implementation Checklist (Metal4GPT-Aligned)

1) Run correctness baselines (count=1 and count>1) and compare outputs.
2) Tune exec-ready threadgroup sizing (Phase 6) via
   `METALFPGA_SCHED_EXEC_READY_TG`.
3) Tune per-phase TG sizes and barrier alias settings once correctness holds.
4) Consider work-queue chunking only if Phase 6 tuning is insufficient.

Latest run metrics (batch path, count=1, exec-ready diag enabled, dispatches=7, binds=217):
- Batch 1 (cold): queue_ms=387.267, gpu_exec_ms=45.238, wait_gap_ms=420.698,
  gpu_ms(counter)=12.709.
- Batch 2: queue_ms=0.6355, gpu_exec_ms=1.9635, wait_gap_ms=1.05125.
- Batch 3: queue_ms=0.205667, gpu_exec_ms=3.3, wait_gap_ms=0.331709.
- Interpretation: first batch is warm-up/queue latency; steady-state is
  GPU-dominated with low queue overhead. Counter-heap `gpu_ms` can under-report
  vs feedback `gpu_exec_ms` on these batched paths.

### Phase 5: Service I/O Drain Strategy

Reduce round-trips for service requests:
- Accumulate service requests in a ring buffer on GPU.
- Drain on batch boundaries or every N ticks.

Success criteria:
- Service I/O no longer forces per-step CPU waits.

Status:
- Complete. Service records now use a ring buffer (tail in
  `sched_service_count[gid]`, head in `sched_service_count[sched.count + gid]`)
  and the CPU updates the head on drain; draining cadence is controlled by
  `METALFPGA_SCHED_SERVICE_DRAIN_EVERY`.

### Phase 6: Tuning and Optional Load Balancing

Tune for GPU efficiency:
- Threadgroup sizes:
  - 256/384 for wait eval and compaction.
  - 64-128 for branchy VM execute.
- Per-phase overrides:
  - `METALFPGA_SCHED_READY_RESET_TG`, `METALFPGA_SCHED_WAIT_EVAL_TG`,
    `METALFPGA_SCHED_READY_FLAGS_TG`, `METALFPGA_SCHED_READY_COMPACT_TG`,
    `METALFPGA_SCHED_READY_DISPATCH_TG`, `METALFPGA_SCHED_EXEC_READY_TG`.
- Consider a chunked work-queue inside VM exec if imbalance remains:
  - One atomic per threadgroup per chunk.
- Respect `maxTotalThreadsPerThreadgroup` per pipeline and keep
  `requiredThreadsPerThreadgroup` consistent with indirect dispatch.

Success criteria:
- `gpu_ms` drops significantly vs baseline.
- GPU occupancy improves with stable correctness.

## Verification Checklist

- Same functional output (VCD / testbench results) vs baseline.
- Timing logs show `grid >= proc_count` for proc-parallel phases.
- Commit feedback shows queue delay and GPU time separated.
- No regressions in `--run` for known designs.

## Resolved Guidance (Metal4GPT, Tentative)

- Barriers: use explicit producer -> consumer barriers between phases,
  especially around indirect args; start with `.device` visibility and add
  `.resourceAlias` if aliasing GPU addresses is involved.
- Encoder layout: default to a single compute encoder for all phases in one
  command buffer; only split into multiple encoders if debugging or if a hard
  pass boundary is needed.
- VM exec threadgroup sizing: start with 64–128 and tune empirically (branchy
  code can benefit from smaller groups).
- Indirect dispatch: use `dispatchThreadsWithIndirectBuffer:` and ensure
  threadgroup sizing is fixed via `requiredThreadsPerThreadgroup`.
- Residency sets: apply a residency set that covers all GPU-address bound
  buffers for the scheduler batch.
